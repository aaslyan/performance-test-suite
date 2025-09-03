#ifndef CPU_BENCH_H
#define CPU_BENCH_H

#include "benchmark.h"
#include "utils.h"
#include <thread>
#include <vector>
#include <atomic>

class CPUBenchmark : public Benchmark {
private:
    std::atomic<uint64_t> total_ops{0};
    std::atomic<bool> should_stop{false};
    LatencyStats latency_stats;
    
    void runSingleThread(int thread_id);
    void runFloatingPoint();
    void runInteger();
    void measureCacheLatency(std::vector<double>& latencies);
    
public:
    BenchmarkResult run(int duration_seconds, int iterations, bool verbose) override;
    std::string getName() const override { return "CPU"; }
};

#endif