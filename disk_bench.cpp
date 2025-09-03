#include "disk_bench.h"
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <random>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

// Compatibility helper for older C++ standards
namespace compat {
inline bool file_exists(const std::string& path)
{
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

inline bool remove_file(const std::string& path)
{
    return (unlink(path.c_str()) == 0);
}
}

DiskBenchmark::DiskBenchmark()
{
    char temp_template[] = "/tmp/perf_test_XXXXXX";
    int fd = mkstemp(temp_template);
    if (fd != -1) {
        test_file_path = temp_template;
        close(fd);
    } else {
        test_file_path = "/tmp/perf_test_disk.dat";
    }
}

DiskBenchmark::~DiskBenchmark()
{
    cleanup();
}

void DiskBenchmark::cleanup()
{
    if (!test_file_path.empty() && compat::file_exists(test_file_path)) {
        compat::remove_file(test_file_path);
    }
}

double DiskBenchmark::measureSequentialWrite(const std::string& path, size_t size, LatencyStats& stats)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing");
    }

    std::vector<char> buffer(BLOCK_SIZE, 'X');
    size_t bytes_written = 0;

    Timer overall_timer;
    overall_timer.start();

    while (bytes_written < size) {
        Timer op_timer;
        op_timer.start();

        size_t write_size = std::min(BLOCK_SIZE, size - bytes_written);
        file.write(buffer.data(), write_size);

        if (!file) {
            throw std::runtime_error("Write operation failed");
        }

        double latency_ms = op_timer.elapsedMilliseconds();
        stats.addSample(latency_ms);

        bytes_written += write_size;
    }

    file.flush();
    file.close();

    double elapsed_seconds = overall_timer.elapsedSeconds();
    double throughput_mbps = (bytes_written / (1024.0 * 1024.0)) / elapsed_seconds;

    return throughput_mbps;
}

double DiskBenchmark::measureSequentialRead(const std::string& path, size_t size, LatencyStats& stats)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file for reading");
    }

    std::vector<char> buffer(BLOCK_SIZE);
    size_t bytes_read = 0;

    Timer overall_timer;
    overall_timer.start();

    while (bytes_read < size && file) {
        Timer op_timer;
        op_timer.start();

        file.read(buffer.data(), BLOCK_SIZE);
        size_t read_size = file.gcount();

        double latency_ms = op_timer.elapsedMilliseconds();
        stats.addSample(latency_ms);

        bytes_read += read_size;
    }

    file.close();

    double elapsed_seconds = overall_timer.elapsedSeconds();
    double throughput_mbps = (bytes_read / (1024.0 * 1024.0)) / elapsed_seconds;

    return throughput_mbps;
}

double DiskBenchmark::measureRandomWrite(const std::string& path, size_t size, int ops, LatencyStats& stats)
{
    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        throw std::runtime_error("Failed to open file for random write");
    }

    if (ftruncate(fd, size) != 0) {
        close(fd);
        throw std::runtime_error("Failed to set file size");
    }

    std::vector<char> buffer(4096, 'R');
    std::mt19937 gen(42);
    std::uniform_int_distribution<off_t> dis(0, size - 4096);

    Timer overall_timer;
    overall_timer.start();

    for (int i = 0; i < ops; ++i) {
        Timer op_timer;
        op_timer.start();

        off_t offset = dis(gen);
        lseek(fd, offset, SEEK_SET);
        write(fd, buffer.data(), 4096);

        fsync(fd);

        double latency_ms = op_timer.elapsedMilliseconds();
        stats.addSample(latency_ms);
    }

    close(fd);

    double elapsed_seconds = overall_timer.elapsedSeconds();
    double iops = ops / elapsed_seconds;

    return iops;
}

double DiskBenchmark::measureRandomRead(const std::string& path, size_t size, int ops, LatencyStats& stats)
{
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Failed to open file for random read");
    }

    std::vector<char> buffer(4096);
    std::mt19937 gen(42);
    std::uniform_int_distribution<off_t> dis(0, size - 4096);

    Timer overall_timer;
    overall_timer.start();

    for (int i = 0; i < ops; ++i) {
        Timer op_timer;
        op_timer.start();

        off_t offset = dis(gen);
        lseek(fd, offset, SEEK_SET);
        read(fd, buffer.data(), 4096);

        double latency_ms = op_timer.elapsedMilliseconds();
        stats.addSample(latency_ms);
    }

    close(fd);

    double elapsed_seconds = overall_timer.elapsedSeconds();
    double iops = ops / elapsed_seconds;

    return iops;
}

BenchmarkResult DiskBenchmark::run(int duration_seconds, int iterations, bool verbose)
{
    BenchmarkResult result;
    result.name = getName();

    try {
        struct statvfs stat;
        if (statvfs("/tmp", &stat) == 0) {
            unsigned long available = stat.f_bavail * stat.f_frsize;
            if (verbose) {
                std::cout << "  Available disk space: " << (available / (1024 * 1024 * 1024)) << " GB\n";
            }

            if (available < FILE_SIZE * 2) {
                throw std::runtime_error("Insufficient disk space for test");
            }
        }

        size_t test_size = FILE_SIZE;

        if (test_size > 1024 * 1024 * 1024) {
            test_size = 256 * 1024 * 1024;
        }

        if (verbose) {
            std::cout << "  Using test file: " << test_file_path << "\n";
            std::cout << "  Test file size: " << (test_size / (1024 * 1024)) << " MB\n";
            std::cout << "  Running sequential write test...\n";
        }

        LatencyStats write_stats, read_stats, random_write_stats, random_read_stats;

        double seq_write_throughput = measureSequentialWrite(test_file_path, test_size, write_stats);

        if (verbose) {
            std::cout << "  Running sequential read test...\n";
        }

        double seq_read_throughput = measureSequentialRead(test_file_path, test_size, read_stats);

        if (verbose) {
            std::cout << "  Running random write test...\n";
        }

        int random_ops = 1000;
        double random_write_iops = measureRandomWrite(test_file_path, test_size, random_ops, random_write_stats);

        if (verbose) {
            std::cout << "  Running random read test...\n";
        }

        double random_read_iops = measureRandomRead(test_file_path, test_size, random_ops, random_read_stats);

        result.throughput = (seq_write_throughput + seq_read_throughput) / 2.0;
        result.throughput_unit = "MB/s";

        LatencyStats combined_stats;
        for (size_t i = 0; i < write_stats.getCount(); ++i) {
            combined_stats.addSample(write_stats.getPercentile((i * 100.0) / write_stats.getCount()));
        }
        for (size_t i = 0; i < read_stats.getCount(); ++i) {
            combined_stats.addSample(read_stats.getPercentile((i * 100.0) / read_stats.getCount()));
        }

        result.avg_latency = combined_stats.getAverage();
        result.min_latency = combined_stats.getMin();
        result.max_latency = combined_stats.getMax();
        result.p50_latency = combined_stats.getPercentile(50);
        result.p90_latency = combined_stats.getPercentile(90);
        result.p99_latency = combined_stats.getPercentile(99);
        result.latency_unit = "ms";

        result.extra_metrics["sequential_write_mbps"] = seq_write_throughput;
        result.extra_metrics["sequential_read_mbps"] = seq_read_throughput;
        result.extra_metrics["random_write_iops"] = random_write_iops;
        result.extra_metrics["random_read_iops"] = random_read_iops;
        result.extra_metrics["random_write_latency_ms"] = random_write_stats.getAverage();
        result.extra_metrics["random_read_latency_ms"] = random_read_stats.getAverage();
        result.extra_metrics["test_file_size_mb"] = test_size / (1024.0 * 1024.0);

        if (random_read_iops > 5000) {
            result.extra_metrics["likely_disk_type"] = 1.0; // SSD
        } else {
            result.extra_metrics["likely_disk_type"] = 0.0; // HDD
        }

        result.status = "success";

        cleanup();

    } catch (const std::exception& e) {
        result.status = "error";
        result.error_message = e.what();
        cleanup();
    }

    return result;
}