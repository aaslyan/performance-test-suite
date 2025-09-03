#ifndef DISK_BENCH_H
#define DISK_BENCH_H

#include "benchmark.h"
#include "utils.h"
#include <string>

class DiskBenchmark : public Benchmark {
private:
    static constexpr size_t FILE_SIZE = 256 * 1024 * 1024; // 256MB test file
    static constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024; // 4MB blocks
    std::string test_file_path;

    double measureSequentialWrite(const std::string& path, size_t size, LatencyStats& stats);
    double measureSequentialRead(const std::string& path, size_t size, LatencyStats& stats);
    double measureRandomWrite(const std::string& path, size_t size, int ops, LatencyStats& stats);
    double measureRandomRead(const std::string& path, size_t size, int ops, LatencyStats& stats);
    void cleanup();

public:
    DiskBenchmark();
    ~DiskBenchmark();
    BenchmarkResult run(int duration_seconds, int iterations, bool verbose) override;
    std::string getName() const override { return "Disk I/O"; }
};

#endif