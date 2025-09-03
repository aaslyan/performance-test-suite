#ifndef INTEGRATED_BENCH_H
#define INTEGRATED_BENCH_H

#include "benchmark.h"
#include "utils.h"
#include <atomic>
#include <mutex>
#include <thread>

class IntegratedBenchmark : public Benchmark {
private:
    struct WorkflowMetrics {
        double end_to_end_latency_ms;
        double throughput_ops_sec;
        double cpu_utilization_percent;
        double memory_bandwidth_mbps;
    };

    WorkflowMetrics runNetworkToMemoryWorkflow(int duration_seconds);
    WorkflowMetrics runMemoryToDiskWorkflow(int duration_seconds);
    WorkflowMetrics runFullPipeline(int duration_seconds);

public:
    BenchmarkResult run(int duration_seconds, int iterations, bool verbose) override;
    std::string getName() const override { return "Integrated System"; }
};

#endif