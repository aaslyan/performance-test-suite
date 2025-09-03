#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <chrono>
#include <map>
#include <string>
#include <vector>

struct BenchmarkResult {
    std::string name;
    double throughput;
    std::string throughput_unit;
    double avg_latency;
    double min_latency;
    double max_latency;
    double p50_latency;
    double p90_latency;
    double p99_latency;
    std::string latency_unit;
    std::map<std::string, double> extra_metrics;
    std::string status;
    std::string error_message;
};

class Benchmark {
public:
    virtual ~Benchmark() = default;
    virtual BenchmarkResult run(int duration_seconds, int iterations, bool verbose) = 0;
    virtual std::string getName() const = 0;
};

#endif