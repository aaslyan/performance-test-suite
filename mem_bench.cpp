#include "mem_bench.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <new>
#include <random>

double MemoryBenchmark::measureSequentialRead(void* buffer, size_t size, int iterations)
{
    char* src = static_cast<char*>(buffer);
    char* dst = new char[BLOCK_SIZE];

    Timer timer;
    timer.start();

    for (int iter = 0; iter < iterations; ++iter) {
        size_t offset = 0;
        while (offset + BLOCK_SIZE <= size) {
            std::memcpy(dst, src + offset, BLOCK_SIZE);
            offset += BLOCK_SIZE;
        }
    }

    double elapsed_seconds = timer.elapsedSeconds();
    double bytes_read = static_cast<double>(size) * iterations;
    double throughput_mbps = (bytes_read / (1024 * 1024)) / elapsed_seconds;

    delete[] dst;
    return throughput_mbps;
}

double MemoryBenchmark::measureSequentialWrite(void* buffer, size_t size, int iterations)
{
    char* dst = static_cast<char*>(buffer);
    char* src = new char[BLOCK_SIZE];

    std::memset(src, 0xAA, BLOCK_SIZE);

    Timer timer;
    timer.start();

    for (int iter = 0; iter < iterations; ++iter) {
        size_t offset = 0;
        while (offset + BLOCK_SIZE <= size) {
            std::memcpy(dst + offset, src, BLOCK_SIZE);
            offset += BLOCK_SIZE;
        }
    }

    double elapsed_seconds = timer.elapsedSeconds();
    double bytes_written = static_cast<double>(size) * iterations;
    double throughput_mbps = (bytes_written / (1024 * 1024)) / elapsed_seconds;

    delete[] src;
    return throughput_mbps;
}

double MemoryBenchmark::measureRandomAccess(void* buffer, size_t size, int iterations, LatencyStats& stats)
{
    char* mem = static_cast<char*>(buffer);
    size_t num_elements = size / sizeof(long long);
    long long* array = reinterpret_cast<long long*>(mem);

    std::vector<size_t> indices(iterations);
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dis(0, num_elements - 1);

    for (auto& idx : indices) {
        idx = dis(gen);
    }

    Timer overall_timer;
    overall_timer.start();

    long long sum = 0;
    for (int i = 0; i < iterations; ++i) {
        Timer op_timer;
        op_timer.start();

        sum += array[indices[i]];
        array[indices[i]] = sum;

        double latency_us = op_timer.elapsedMicroseconds();
        stats.addSample(latency_us);
    }

    double elapsed_seconds = overall_timer.elapsedSeconds();
    double ops_per_second = iterations / elapsed_seconds;

    if (sum == 0) {
        std::cout << "";
    }

    return ops_per_second;
}

BenchmarkResult MemoryBenchmark::run(int duration_seconds, int iterations, bool verbose)
{
    BenchmarkResult result;
    result.name = getName();

    try {
        size_t buffer_size = BUFFER_SIZE;
        void* buffer = nullptr;

        while (buffer_size >= 16 * 1024 * 1024 && buffer == nullptr) {
            try {
                buffer = std::aligned_alloc(4096, buffer_size);
                if (buffer)
                    break;
            } catch (...) {
                // Allocation failed
            }
            buffer_size /= 2;
        }

        if (!buffer) {
            buffer = std::malloc(buffer_size);
            if (!buffer) {
                throw std::runtime_error("Failed to allocate memory buffer");
            }
        }

        if (verbose) {
            std::cout << "  Allocated " << (buffer_size / (1024 * 1024)) << " MB buffer\n";
            std::cout << "  Running sequential read test...\n";
        }

        std::memset(buffer, 0x55, buffer_size);

        double read_throughput = measureSequentialRead(buffer, buffer_size, 10);

        if (verbose) {
            std::cout << "  Running sequential write test...\n";
        }

        double write_throughput = measureSequentialWrite(buffer, buffer_size, 10);

        if (verbose) {
            std::cout << "  Running random access test...\n";
        }

        LatencyStats random_stats;
        double random_ops = measureRandomAccess(buffer, buffer_size, iterations * 100, random_stats);

        result.throughput = (read_throughput + write_throughput) / 2.0;
        result.throughput_unit = "MB/s";

        result.avg_latency = random_stats.getAverage();
        result.min_latency = random_stats.getMin();
        result.max_latency = random_stats.getMax();
        result.p50_latency = random_stats.getPercentile(50);
        result.p90_latency = random_stats.getPercentile(90);
        result.p99_latency = random_stats.getPercentile(99);
        result.latency_unit = "us";

        result.extra_metrics["sequential_read_mbps"] = read_throughput;
        result.extra_metrics["sequential_write_mbps"] = write_throughput;
        result.extra_metrics["random_access_ops_sec"] = random_ops;
        result.extra_metrics["buffer_size_mb"] = buffer_size / (1024.0 * 1024.0);

        if (verbose) {
            std::cout << "  Running multi-threaded contention test with CPU affinity...\n";
        }

        std::atomic<uint64_t> total_ops(0);
        std::atomic<bool> should_stop(false);
        std::vector<std::thread> threads;
        unsigned int num_threads = std::thread::hardware_concurrency();

        Timer mt_timer;
        mt_timer.start();

        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&, i]() {
                // Pin thread to CPU core for consistent memory performance
                CPUAffinity::pinThreadToCore(i % CPUAffinity::getNumCores());
                
                size_t thread_offset = (buffer_size / num_threads) * i;
                char* thread_buffer = static_cast<char*>(buffer) + thread_offset;
                size_t thread_size = buffer_size / num_threads;

                while (!should_stop.load()) {
                    for (size_t j = 0; j < thread_size; j += 64) {
                        thread_buffer[j]++;
                    }
                    total_ops.fetch_add(thread_size / 64);
                }
            });
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        should_stop.store(true);

        for (auto& t : threads) {
            t.join();
        }

        double mt_elapsed = mt_timer.elapsedSeconds();
        double mt_throughput = (total_ops.load() * 64) / (1024.0 * 1024.0) / mt_elapsed;
        result.extra_metrics["multithread_throughput_mbps"] = mt_throughput;
        result.extra_metrics["threads_used"] = num_threads;

        result.status = "success";

        std::free(buffer);

    } catch (const std::exception& e) {
        result.status = "error";
        result.error_message = e.what();
    }

    return result;
}