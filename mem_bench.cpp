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

    // Adaptive iteration count to ensure measurable execution time
    int actual_iterations = iterations;
    double elapsed_nanoseconds = 0.0;
    double bytes_read = 0.0;

    do {
        Timer timer;
        timer.start();

        for (int iter = 0; iter < actual_iterations; ++iter) {
            size_t offset = 0;
            while (offset + BLOCK_SIZE <= size) {
                std::memcpy(dst, src + offset, BLOCK_SIZE);
                offset += BLOCK_SIZE;
            }
        }

        elapsed_nanoseconds = timer.elapsedNanoseconds();
        bytes_read = static_cast<double>(size) * actual_iterations;

        // If test completed too quickly, increase iterations and retry
        // Minimum 1 million nanoseconds (1ms) for accurate measurement
        if (elapsed_nanoseconds < MIN_MEASURABLE_TIME_NS && actual_iterations < 1000) {
            actual_iterations *= 10;
        } else {
            break;
        }
    } while (actual_iterations <= 1000);

    // Ensure we don't get invalid results
    if (elapsed_nanoseconds <= 0.0) {
        elapsed_nanoseconds = MIN_MEASURABLE_TIME_NS; // Minimum 1ms to avoid division by zero
    }

    // Convert nanoseconds to seconds for throughput calculation
    double elapsed_seconds = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    double throughput_mbps = (bytes_read / (1024 * 1024)) / elapsed_seconds;

    // Sanity check: cap at reasonable maximum (100 GB/s)
    if (throughput_mbps > 100000.0) {
        throughput_mbps = 100000.0;
    }

    delete[] dst;
    return throughput_mbps;
}

double MemoryBenchmark::measureSequentialWrite(void* buffer, size_t size, int iterations)
{
    char* dst = static_cast<char*>(buffer);
    char* src = new char[BLOCK_SIZE];

    std::memset(src, 0xAA, BLOCK_SIZE);

    // Adaptive iteration count to ensure measurable execution time
    int actual_iterations = iterations;
    double elapsed_nanoseconds = 0.0;
    double bytes_written = 0.0;

    do {
        Timer timer;
        timer.start();

        for (int iter = 0; iter < actual_iterations; ++iter) {
            size_t offset = 0;
            while (offset + BLOCK_SIZE <= size) {
                std::memcpy(dst + offset, src, BLOCK_SIZE);
                offset += BLOCK_SIZE;
            }
        }

        elapsed_nanoseconds = timer.elapsedNanoseconds();
        bytes_written = static_cast<double>(size) * actual_iterations;

        // If test completed too quickly, increase iterations and retry
        // Minimum 1 million nanoseconds (1ms) for accurate measurement
        if (elapsed_nanoseconds < MIN_MEASURABLE_TIME_NS && actual_iterations < 1000) {
            actual_iterations *= 10;
        } else {
            break;
        }
    } while (actual_iterations <= 1000);

    // Ensure we don't get invalid results
    if (elapsed_nanoseconds <= 0.0) {
        elapsed_nanoseconds = MIN_MEASURABLE_TIME_NS; // Minimum 1ms to avoid division by zero
    }

    // Convert nanoseconds to seconds for throughput calculation
    double elapsed_seconds = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    double throughput_mbps = (bytes_written / (1024 * 1024)) / elapsed_seconds;

    // Sanity check: cap at reasonable maximum (100 GB/s)
    if (throughput_mbps > 100000.0) {
        throughput_mbps = 100000.0;
    }

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

    double elapsed_nanoseconds = overall_timer.elapsedNanoseconds();
    double elapsed_seconds = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    double ops_per_second = iterations / elapsed_seconds;

    if (sum == 0) {
        std::cout << "";
    }

    return ops_per_second;
}

double MemoryBenchmark::measureRandomAccessBatch(void* buffer, size_t size, int iterations, double& avg_latency_ns)
{
    char* mem = static_cast<char*>(buffer);
    size_t num_elements = size / sizeof(long long);
    long long* array = reinterpret_cast<long long*>(mem);

    // Pre-generate random indices to avoid RNG overhead in timing loop
    std::vector<size_t> indices(iterations);
    std::mt19937 gen(42);
    std::uniform_int_distribution<size_t> dis(0, num_elements - 1);

    for (auto& idx : indices) {
        idx = dis(gen);
    }

    // Warm-up run to stabilize cache and TLB
    long long warmup_sum = 0;
    for (int i = 0; i < std::min(1000, iterations); ++i) {
        warmup_sum += array[indices[i]];
        array[indices[i]] = warmup_sum;
    }

    // Actual measurement - batch timing for accuracy
    Timer batch_timer;
    batch_timer.start();

    long long sum = 0;
    for (int i = 0; i < iterations; ++i) {
        sum += array[indices[i]];
        array[indices[i]] = sum;
    }

    double elapsed_nanoseconds = batch_timer.elapsedNanoseconds();
    
    // Calculate accurate average latency
    avg_latency_ns = elapsed_nanoseconds / iterations;
    
    // Calculate operations per second
    double elapsed_seconds = elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
    double ops_per_second = iterations / elapsed_seconds;

    // Prevent compiler optimization
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
            std::cout << "  Running random access test (individual timing for distribution)...\n";
        }

        LatencyStats random_stats;
        double random_ops = measureRandomAccess(buffer, buffer_size, iterations * 100, random_stats);

        if (verbose) {
            std::cout << "  Running random access test (batch timing for accuracy)...\n";
        }

        // Run batch timing for accurate average latency
        double batch_avg_latency_ns = 0.0;
        double batch_random_ops = measureRandomAccessBatch(buffer, buffer_size, iterations * 1000, batch_avg_latency_ns);
        double batch_avg_latency_us = batch_avg_latency_ns / 1000.0;

        if (verbose) {
            std::cout << "  Random Access Timing Comparison:\n";
            std::cout << "    Individual timing: " << random_stats.getAverage() << " us avg (includes timer overhead)\n";
            std::cout << "    Batch timing:      " << batch_avg_latency_us << " us avg (accurate)\n";
            std::cout << "    Timer overhead:    " << (random_stats.getAverage() - batch_avg_latency_us) << " us per measurement\n";
        }

        result.throughput = (read_throughput + write_throughput) / 2.0;
        result.throughput_unit = "MB/s";

        // Use batch timing for accurate average, but keep distribution stats from individual timing
        result.avg_latency = batch_avg_latency_us;  // Use accurate batch-measured average
        result.min_latency = random_stats.getMin();
        result.max_latency = random_stats.getMax();
        result.p50_latency = random_stats.getPercentile(50);
        result.p90_latency = random_stats.getPercentile(90);
        result.p99_latency = random_stats.getPercentile(99);
        result.latency_unit = "us";

        result.extra_metrics["sequential_read_mbps"] = read_throughput;
        result.extra_metrics["sequential_write_mbps"] = write_throughput;
        result.extra_metrics["random_access_ops_sec"] = random_ops;
        result.extra_metrics["random_access_batch_ops_sec"] = batch_random_ops;
        result.extra_metrics["random_latency_batch_ns"] = batch_avg_latency_ns;
        result.extra_metrics["random_latency_overhead_us"] = random_stats.getAverage() - batch_avg_latency_us;
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

        double mt_elapsed_nanoseconds = mt_timer.elapsedNanoseconds();
        double mt_elapsed = mt_elapsed_nanoseconds / NANOSECONDS_PER_SECOND;
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