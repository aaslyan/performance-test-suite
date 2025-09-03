#ifndef MEM_BENCH_H
#define MEM_BENCH_H

#include "benchmark.h"
#include "utils.h"
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

class MemoryBenchmark : public Benchmark {
private:
    static constexpr size_t BUFFER_SIZE = 256 * 1024 * 1024; // 256MB
    static constexpr size_t BLOCK_SIZE = 4096; // 4KB blocks

    double measureSequentialRead(void* buffer, size_t size, int iterations);
    double measureSequentialWrite(void* buffer, size_t size, int iterations);
    double measureRandomAccess(void* buffer, size_t size, int iterations, LatencyStats& stats);

public:
    BenchmarkResult run(int duration_seconds, int iterations, bool verbose) override;
    std::string getName() const override { return "Memory"; }
};

#endif