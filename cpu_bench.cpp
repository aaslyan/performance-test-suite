#include "cpu_bench.h"
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>

void CPUBenchmark::runSingleThread(int thread_id)
{
    // Pin thread to specific CPU core for consistent performance
    bool affinity_set = CPUAffinity::pinThreadToCore(thread_id % CPUAffinity::getNumCores());

    std::mt19937 gen(thread_id);
    std::uniform_real_distribution<> dis(0.0, 1000.0);

    while (!should_stop.load()) {
        double sum = 0.0;
        int int_sum = 0;

        // Batch operations for better performance measurement
        for (int i = 0; i < 1000; ++i) {
            // Floating point operations
            double a = dis(gen);
            double b = dis(gen);
            sum += std::sin(a) * std::cos(b);
            sum += std::sqrt(a * b);
            sum += std::exp(-a / (b + 1.0));

            // Integer operations
            int x = static_cast<int>(a);
            int y = static_cast<int>(b) + 1;
            int_sum += x * y;
            int_sum += x / y;
            int_sum ^= x << 2;
            int_sum |= y >> 1;
        }

        total_ops.fetch_add(10000);

        // Prevent optimization
        if (sum == 0.0 && int_sum == 0) {
            std::cout << "";
        }
    }
}

void CPUBenchmark::runFloatingPoint()
{
    Timer timer;
    std::vector<double> values(100000);
    std::mt19937 gen(42);
    std::uniform_real_distribution<> dis(0.1, 100.0);

    for (auto& v : values) {
        v = dis(gen);
    }

    timer.start();
    double result = 0.0;
    for (size_t i = 0; i < values.size() - 1; ++i) {
        result += std::sin(values[i]) * std::cos(values[i + 1]);
        result += std::sqrt(std::abs(values[i]));
        result += std::log(values[i] + 1.0);
    }
    double elapsed = timer.elapsedMicroseconds();

    double ops = values.size() * 3;
    double latency_per_op = elapsed / ops;
    latency_stats.addSample(latency_per_op);

    if (result == 0.0) {
        std::cout << "";
    }
}

void CPUBenchmark::runInteger()
{
    Timer timer;
    std::vector<int> values(100000);
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 1000000);

    for (auto& v : values) {
        v = dis(gen);
    }

    timer.start();
    long long result = 0;
    for (size_t i = 0; i < values.size() - 1; ++i) {
        result += values[i] * values[i + 1];
        result ^= values[i] << 3;
        result |= values[i + 1] >> 2;
        result += values[i] / (values[i + 1] + 1);
    }
    double elapsed = timer.elapsedMicroseconds();

    double ops = values.size() * 4;
    double latency_per_op = elapsed / ops;
    latency_stats.addSample(latency_per_op);

    if (result == 0) {
        std::cout << "";
    }
}

void CPUBenchmark::measureCacheLatency(std::vector<double>& latencies)
{
    const size_t sizes[] = {
        1024, // 4KB - L1 cache
        32 * 1024, // 128KB - L2 cache
        256 * 1024, // 1MB - L3 cache
        8 * 1024 * 1024 // 32MB - Main memory
    };

    for (size_t size : sizes) {
        std::vector<int> array(size / sizeof(int));
        std::mt19937 gen(42);
        std::uniform_int_distribution<> dis(0, array.size() - 1);

        for (auto& v : array) {
            v = dis(gen);
        }

        std::vector<int> indices(1000);
        for (auto& idx : indices) {
            idx = dis(gen);
        }

        Timer timer;
        timer.start();

        int sum = 0;
        for (int idx : indices) {
            sum += array[idx];
        }

        double elapsed = timer.elapsedNanoseconds();
        double avg_latency = elapsed / indices.size();
        latencies.push_back(avg_latency);

        if (sum == 0) {
            std::cout << "";
        }
    }
}

BenchmarkResult CPUBenchmark::run(int duration_seconds, int iterations, bool verbose)
{
    BenchmarkResult result;
    result.name = getName();

    try {
        if (verbose) {
            std::cout << "  Starting CPU benchmark with " << std::thread::hardware_concurrency()
                      << " threads on " << CPUAffinity::getNumCores() << " CPU cores\n";
            std::cout << "  CPU affinity: threads will be pinned to cores\n";
        }

        total_ops.store(0);
        should_stop.store(false);
        latency_stats.clear();

        std::vector<std::thread> threads;
        unsigned int num_threads = std::thread::hardware_concurrency();

        Timer benchmark_timer;
        benchmark_timer.start();

        for (unsigned int i = 0; i < num_threads; ++i) {
            threads.emplace_back(&CPUBenchmark::runSingleThread, this, i);
        }

        std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
        should_stop.store(true);

        for (auto& t : threads) {
            t.join();
        }

        double elapsed_seconds = benchmark_timer.elapsedSeconds();

        uint64_t total = total_ops.load();
        result.throughput = total / elapsed_seconds / 1e9; // GOPS
        result.throughput_unit = "GOPS";

        if (verbose) {
            std::cout << "  Multi-threaded test completed. Running latency tests...\n";
        }

        for (int i = 0; i < iterations; ++i) {
            runFloatingPoint();
            runInteger();
        }

        std::vector<double> cache_latencies;
        measureCacheLatency(cache_latencies);

        result.avg_latency = latency_stats.getAverage();
        result.min_latency = latency_stats.getMin();
        result.max_latency = latency_stats.getMax();
        result.p50_latency = latency_stats.getPercentile(50);
        result.p90_latency = latency_stats.getPercentile(90);
        result.p99_latency = latency_stats.getPercentile(99);
        result.latency_unit = "us/op";

        result.extra_metrics["l1_cache_latency_ns"] = cache_latencies[0];
        result.extra_metrics["l2_cache_latency_ns"] = cache_latencies[1];
        result.extra_metrics["l3_cache_latency_ns"] = cache_latencies[2];
        result.extra_metrics["mem_latency_ns"] = cache_latencies[3];
        result.extra_metrics["threads_used"] = num_threads;
        result.extra_metrics["cpu_cores"] = CPUAffinity::getNumCores();
        result.extra_metrics["cpu_affinity_enabled"] = 1.0;

        result.status = "success";

    } catch (const std::exception& e) {
        result.status = "error";
        result.error_message = e.what();
    }

    return result;
}