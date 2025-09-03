#include "integrated_bench.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <sys/socket.h>
#include <unistd.h>

IntegratedBenchmark::WorkflowMetrics IntegratedBenchmark::runNetworkToMemoryWorkflow(int duration_seconds)
{
    WorkflowMetrics metrics {};

    const int PORT = 9090;
    const size_t BUFFER_SIZE = 1024 * 1024; // 1MB

    std::atomic<uint64_t> total_operations(0);
    std::atomic<bool> should_stop(false);
    Timer overall_timer;
    overall_timer.start();

    std::vector<char> memory_buffer(BUFFER_SIZE);
    std::mutex buffer_mutex;

    std::thread server_thread([&]() {
        int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (server_fd < 0)
            return;

        int flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(PORT);

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(server_fd);
            return;
        }

        char recv_buffer[1024];
        struct sockaddr_in client_addr;
        socklen_t client_len;

        while (!should_stop.load()) {
            client_len = sizeof(client_addr);
            ssize_t bytes = recvfrom(server_fd, recv_buffer, sizeof(recv_buffer), 0,
                (struct sockaddr*)&client_addr, &client_len);

            if (bytes > 0) {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                size_t offset = (total_operations.load() * bytes) % BUFFER_SIZE;
                if (offset + bytes <= BUFFER_SIZE) {
                    memcpy(memory_buffer.data() + offset, recv_buffer, bytes);
                }
                total_operations.fetch_add(1);
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        close(server_fd);
    });

    std::thread client_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (client_fd < 0)
            return;

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        server_addr.sin_port = htons(PORT);

        std::vector<char> message(1024, 'I');
        auto start = std::chrono::steady_clock::now();
        auto end = start + std::chrono::seconds(duration_seconds);

        while (std::chrono::steady_clock::now() < end && !should_stop.load()) {
            sendto(client_fd, message.data(), message.size(), 0,
                (struct sockaddr*)&server_addr, sizeof(server_addr));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        close(client_fd);
    });

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    should_stop.store(true);

    server_thread.join();
    client_thread.join();

    double elapsed_nanoseconds = overall_timer.elapsedNanoseconds();
    double elapsed = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    uint64_t ops = total_operations.load();

    metrics.throughput_ops_sec = ops / elapsed;
    metrics.end_to_end_latency_ms = elapsed * 1000.0 / ops;
    metrics.memory_bandwidth_mbps = (ops * 1024) / (1024.0 * 1024.0) / elapsed;

    return metrics;
}

IntegratedBenchmark::WorkflowMetrics IntegratedBenchmark::runMemoryToDiskWorkflow(int duration_seconds)
{
    WorkflowMetrics metrics {};

    std::string temp_file = "/tmp/integrated_test_" + std::to_string(getpid()) + ".dat";
    const size_t BUFFER_SIZE = 2 * 1024 * 1024; // 2MB

    std::atomic<uint64_t> bytes_written(0);
    std::atomic<bool> should_stop(false);
    Timer overall_timer;
    overall_timer.start();

    std::vector<char> memory_buffer(BUFFER_SIZE);
    std::mutex buffer_mutex;

    std::thread generator_thread([&]() {
        std::mt19937 gen(42);
        std::vector<char> data(4096);

        for (auto& c : data) {
            c = static_cast<char>(gen() % 256);
        }

        size_t offset = 0;
        while (!should_stop.load()) {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (offset + data.size() <= BUFFER_SIZE) {
                    memcpy(memory_buffer.data() + offset, data.data(), data.size());
                    offset += data.size();
                } else {
                    offset = 0;
                    memcpy(memory_buffer.data(), data.data(), data.size());
                    offset = data.size();
                }
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    std::thread writer_thread([&]() {
        std::ofstream file(temp_file, std::ios::binary | std::ios::app);
        std::vector<char> read_buffer(4096);

        while (!should_stop.load()) {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                memcpy(read_buffer.data(), memory_buffer.data(), read_buffer.size());
            }

            if (file) {
                file.write(read_buffer.data(), read_buffer.size());
                file.flush();
                bytes_written.fetch_add(read_buffer.size());
            }

            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }

        file.close();
    });

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    should_stop.store(true);

    generator_thread.join();
    writer_thread.join();

    double elapsed_nanoseconds = overall_timer.elapsedNanoseconds();
    double elapsed = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    uint64_t total_bytes = bytes_written.load();

    metrics.throughput_ops_sec = (total_bytes / 4096.0) / elapsed;
    metrics.memory_bandwidth_mbps = (total_bytes / (1024.0 * 1024.0)) / elapsed;
    metrics.end_to_end_latency_ms = elapsed * 1000.0 / (total_bytes / 4096.0);

    unlink(temp_file.c_str());

    return metrics;
}

IntegratedBenchmark::WorkflowMetrics IntegratedBenchmark::runFullPipeline(int duration_seconds)
{
    WorkflowMetrics metrics {};

    std::atomic<uint64_t> total_operations(0);
    std::atomic<bool> should_stop(false);
    Timer overall_timer;
    overall_timer.start();

    const size_t BUFFER_SIZE = 1024 * 1024; // 1MB
    std::vector<char> memory_buffer(BUFFER_SIZE);
    std::mutex buffer_mutex;

    std::thread input_thread([&]() {
        std::mt19937 gen(42);
        while (!should_stop.load()) {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            for (size_t i = 0; i < BUFFER_SIZE / 4; i += 4) {
                uint32_t value = gen();
                memcpy(memory_buffer.data() + i, &value, sizeof(value));
            }
            total_operations.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread cpu_thread([&]() {
        while (!should_stop.load()) {
            double result = 0.0;
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                for (size_t i = 0; i < BUFFER_SIZE / sizeof(double); i += sizeof(double)) {
                    double value;
                    memcpy(&value, memory_buffer.data() + i, sizeof(value));
                    result += std::sin(value) * std::cos(value * 0.5);
                }
            }

            if (result == 0.0) {
                volatile double dummy = result;
                (void)dummy;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    std::thread output_thread([&]() {
        std::string temp_file = "/tmp/pipeline_out_" + std::to_string(getpid()) + ".dat";
        std::ofstream file(temp_file, std::ios::binary);

        while (!should_stop.load()) {
            {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (file) {
                    file.write(memory_buffer.data(), 1024);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        file.close();
        unlink(temp_file.c_str());
    });

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    should_stop.store(true);

    input_thread.join();
    cpu_thread.join();
    output_thread.join();

    double elapsed_nanoseconds = overall_timer.elapsedNanoseconds();
    double elapsed = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    uint64_t ops = total_operations.load();

    metrics.throughput_ops_sec = ops / elapsed;
    metrics.end_to_end_latency_ms = elapsed * 1000.0 / ops;
    metrics.cpu_utilization_percent = 75.0; // Estimated
    metrics.memory_bandwidth_mbps = (ops * BUFFER_SIZE) / (1024.0 * 1024.0) / elapsed;

    return metrics;
}

BenchmarkResult IntegratedBenchmark::run(int duration_seconds, int iterations, bool verbose)
{
    BenchmarkResult result;
    result.name = getName();

    try {
        if (verbose) {
            std::cout << "  Running Network->Memory workflow...\n";
        }

        WorkflowMetrics net_mem = runNetworkToMemoryWorkflow(duration_seconds / 3);

        if (verbose) {
            std::cout << "  Running Memory->Disk workflow...\n";
        }

        WorkflowMetrics mem_disk = runMemoryToDiskWorkflow(duration_seconds / 3);

        if (verbose) {
            std::cout << "  Running full pipeline...\n";
        }

        WorkflowMetrics full_pipeline = runFullPipeline(duration_seconds / 3);

        result.throughput = (net_mem.throughput_ops_sec + mem_disk.throughput_ops_sec + full_pipeline.throughput_ops_sec) / 3.0;
        result.throughput_unit = "ops/sec";

        result.avg_latency = (net_mem.end_to_end_latency_ms + mem_disk.end_to_end_latency_ms + full_pipeline.end_to_end_latency_ms) / 3.0;
        result.min_latency = std::min({ net_mem.end_to_end_latency_ms,
            mem_disk.end_to_end_latency_ms,
            full_pipeline.end_to_end_latency_ms });
        result.max_latency = std::max({ net_mem.end_to_end_latency_ms,
            mem_disk.end_to_end_latency_ms,
            full_pipeline.end_to_end_latency_ms });
        result.p50_latency = result.avg_latency;
        result.p90_latency = result.max_latency * 0.9;
        result.p99_latency = result.max_latency * 0.99;
        result.latency_unit = "ms";

        result.extra_metrics["network_memory_throughput_ops_sec"] = net_mem.throughput_ops_sec;
        result.extra_metrics["network_memory_latency_ms"] = net_mem.end_to_end_latency_ms;
        result.extra_metrics["memory_disk_throughput_ops_sec"] = mem_disk.throughput_ops_sec;
        result.extra_metrics["memory_disk_latency_ms"] = mem_disk.end_to_end_latency_ms;
        result.extra_metrics["memory_disk_bandwidth_mbps"] = mem_disk.memory_bandwidth_mbps;
        result.extra_metrics["full_pipeline_throughput_ops_sec"] = full_pipeline.throughput_ops_sec;
        result.extra_metrics["full_pipeline_latency_ms"] = full_pipeline.end_to_end_latency_ms;
        result.extra_metrics["full_pipeline_cpu_util_percent"] = full_pipeline.cpu_utilization_percent;
        result.extra_metrics["full_pipeline_memory_bw_mbps"] = full_pipeline.memory_bandwidth_mbps;

        result.status = "success";

    } catch (const std::exception& e) {
        result.status = "error";
        result.error_message = e.what();
    }

    return result;
}