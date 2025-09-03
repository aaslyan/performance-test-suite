#include "ipc_bench.h"
#include <chrono>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <random>
#include <sys/wait.h>
#include <unistd.h>

IPCBenchmark::SharedMemorySegment IPCBenchmark::createSharedMemory()
{
    SharedMemorySegment segment;

    pid_t pid = getpid();
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 10000;

    // Use shorter names for macOS compatibility (NAME_MAX is often 31 chars)
    segment.shm_name = "/pf_shm_" + std::to_string(pid % 10000) + "_" + std::to_string(timestamp);
    segment.prod_sem_name = "/pf_prod_" + std::to_string(pid % 10000) + "_" + std::to_string(timestamp);
    segment.cons_sem_name = "/pf_cons_" + std::to_string(pid % 10000) + "_" + std::to_string(timestamp);

    int shm_fd = shm_open(segment.shm_name.c_str(), O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        std::string error_msg = "Failed to create shared memory: ";
        error_msg += strerror(errno);
        error_msg += " (name: " + segment.shm_name + ")";
        throw std::runtime_error(error_msg);
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        close(shm_fd);
        shm_unlink(segment.shm_name.c_str());
        throw std::runtime_error("Failed to set shared memory size");
    }

    segment.shm_ptr = mmap(0, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (segment.shm_ptr == MAP_FAILED) {
        shm_unlink(segment.shm_name.c_str());
        throw std::runtime_error("Failed to map shared memory");
    }

    memset(segment.shm_ptr, 0, SHM_SIZE);
    
    // Initialize shared control block
    SharedControlBlock* control = static_cast<SharedControlBlock*>(segment.shm_ptr);
    control->should_stop = false;
    control->bytes_transferred = 0;

    sem_unlink(segment.prod_sem_name.c_str());
    sem_unlink(segment.cons_sem_name.c_str());

    segment.producer_sem = sem_open(segment.prod_sem_name.c_str(), O_CREAT, 0666, 1);
    segment.consumer_sem = sem_open(segment.cons_sem_name.c_str(), O_CREAT, 0666, 0);

    if (segment.producer_sem == SEM_FAILED || segment.consumer_sem == SEM_FAILED) {
        destroySharedMemory(segment);
        throw std::runtime_error("Failed to create semaphores");
    }

    return segment;
}

void IPCBenchmark::destroySharedMemory(SharedMemorySegment& segment)
{
    if (segment.shm_ptr && segment.shm_ptr != MAP_FAILED) {
        munmap(segment.shm_ptr, SHM_SIZE);
    }

    if (segment.producer_sem && segment.producer_sem != SEM_FAILED) {
        sem_close(segment.producer_sem);
    }

    if (segment.consumer_sem && segment.consumer_sem != SEM_FAILED) {
        sem_close(segment.consumer_sem);
    }

    shm_unlink(segment.shm_name.c_str());
    sem_unlink(segment.prod_sem_name.c_str());
    sem_unlink(segment.cons_sem_name.c_str());

    segment.shm_ptr = nullptr;
    segment.producer_sem = nullptr;
    segment.consumer_sem = nullptr;
}

void IPCBenchmark::producer(SharedMemorySegment& segment, size_t message_size)
{
    SharedControlBlock* control = static_cast<SharedControlBlock*>(segment.shm_ptr);
    char* buffer = control->data;
    std::vector<char> message(message_size, 'P');

    for (size_t i = 0; i < message_size; ++i) {
        message[i] = static_cast<char>('A' + (i % 26));
    }

    while (!control->should_stop) {
        if (sem_wait(segment.producer_sem) == -1) {
            continue;
        }

        if (control->should_stop)
            break;

        memcpy(buffer, message.data(), message_size);

        if (sem_post(segment.consumer_sem) == -1) {
            break;
        }

        __sync_fetch_and_add(&control->bytes_transferred, message_size);
    }
}

void IPCBenchmark::consumer(SharedMemorySegment& segment, size_t message_size, LatencyStats& stats)
{
    SharedControlBlock* control = static_cast<SharedControlBlock*>(segment.shm_ptr);
    char* buffer = control->data;
    std::vector<char> received(message_size);

    while (!control->should_stop) {
        Timer op_timer;
        op_timer.start();

        if (sem_wait(segment.consumer_sem) == -1) {
            continue;
        }

        if (control->should_stop)
            break;

        memcpy(received.data(), buffer, message_size);

        if (sem_post(segment.producer_sem) == -1) {
            break;
        }

        double latency_ms = op_timer.elapsedMilliseconds();
        stats.addSample(latency_ms);
    }
}

double IPCBenchmark::measureThroughput(size_t message_size, int duration_seconds, LatencyStats& stats)
{
    SharedMemorySegment segment = createSharedMemory();
    SharedControlBlock* control = static_cast<SharedControlBlock*>(segment.shm_ptr);

    try {
        pid_t child_pid = fork();

        if (child_pid == -1) {
            throw std::runtime_error("Failed to fork process");
        } else if (child_pid == 0) {
            // Child process - consumer
            LatencyStats child_stats;
            consumer(segment, message_size, child_stats);
            exit(0);
        } else {
            // Parent process - producer
            Timer benchmark_timer;
            benchmark_timer.start();

            std::thread producer_thread(&IPCBenchmark::producer, this, std::ref(segment), message_size);

            std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
            control->should_stop = true;

            sem_post(segment.producer_sem);
            sem_post(segment.consumer_sem);

            producer_thread.join();

            int status;
            waitpid(child_pid, &status, 0);

            double elapsed_seconds = benchmark_timer.elapsedSeconds();
            double throughput_mbps = (control->bytes_transferred / (1024.0 * 1024.0)) / elapsed_seconds;

            destroySharedMemory(segment);
            return throughput_mbps;
        }
    } catch (...) {
        destroySharedMemory(segment);
        throw;
    }

    return 0.0;
}

BenchmarkResult IPCBenchmark::run(int duration_seconds, int iterations, bool verbose)
{
    BenchmarkResult result;
    result.name = getName();

    try {
        std::vector<double> throughputs;
        LatencyStats combined_stats;

        for (size_t message_size : MESSAGE_SIZES) {
            if (verbose) {
                std::cout << "  Testing message size: " << message_size << " bytes\n";
            }

            LatencyStats size_stats;

            int size_iterations = iterations / (sizeof(MESSAGE_SIZES) / sizeof(MESSAGE_SIZES[0]));
            if (size_iterations < 1)
                size_iterations = 1;

            double total_throughput = 0.0;
            for (int i = 0; i < size_iterations; ++i) {
                double throughput = measureThroughput(message_size,
                    duration_seconds / (sizeof(MESSAGE_SIZES) / sizeof(MESSAGE_SIZES[0])),
                    size_stats);
                total_throughput += throughput;

                for (size_t j = 0; j < size_stats.getCount(); ++j) {
                    combined_stats.addSample(size_stats.getPercentile((j * 100.0) / size_stats.getCount()));
                }
                size_stats.clear();
            }

            double avg_throughput = total_throughput / size_iterations;
            throughputs.push_back(avg_throughput);

            result.extra_metrics["throughput_" + std::to_string(message_size) + "b_mbps"] = avg_throughput;
        }

        double total_throughput = 0.0;
        for (double t : throughputs) {
            total_throughput += t;
        }

        result.throughput = total_throughput / throughputs.size();
        result.throughput_unit = "MB/s";

        result.avg_latency = combined_stats.getAverage();
        result.min_latency = combined_stats.getMin();
        result.max_latency = combined_stats.getMax();
        result.p50_latency = combined_stats.getPercentile(50);
        result.p90_latency = combined_stats.getPercentile(90);
        result.p99_latency = combined_stats.getPercentile(99);
        result.latency_unit = "ms";

        result.extra_metrics["max_throughput_mbps"] = *std::max_element(throughputs.begin(), throughputs.end());
        result.extra_metrics["min_throughput_mbps"] = *std::min_element(throughputs.begin(), throughputs.end());
        result.extra_metrics["message_sizes_tested"] = sizeof(MESSAGE_SIZES) / sizeof(MESSAGE_SIZES[0]);
        result.extra_metrics["shared_memory_size_mb"] = SHM_SIZE / (1024.0 * 1024.0);
        result.extra_metrics["latency_samples_collected"] = static_cast<double>(combined_stats.getCount());

        result.status = "success";

    } catch (const std::exception& e) {
        result.status = "error";
        result.error_message = e.what();
    }

    return result;
}