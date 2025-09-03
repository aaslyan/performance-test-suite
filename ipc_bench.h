#ifndef IPC_BENCH_H
#define IPC_BENCH_H

#include "benchmark.h"
#include "utils.h"
#include <atomic>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <thread>

class IPCBenchmark : public Benchmark {
private:
    static constexpr size_t SHM_SIZE = 16 * 1024 * 1024; // 16MB shared memory
    static constexpr size_t MESSAGE_SIZES[] = { 64, 1024, 65536, 1048576 }; // 64B, 1KB, 64KB, 1MB

    struct SharedMemorySegment {
        sem_t* producer_sem;
        sem_t* consumer_sem;
        void* shm_ptr;
        std::string shm_name;
        std::string prod_sem_name;
        std::string cons_sem_name;
    };

    double measureThroughput(size_t message_size, int duration_seconds, LatencyStats& stats);
    void producer(SharedMemorySegment& segment, size_t message_size,
        std::atomic<bool>& should_stop, std::atomic<uint64_t>& bytes_transferred);
    void consumer(SharedMemorySegment& segment, size_t message_size,
        std::atomic<bool>& should_stop, LatencyStats& stats);

    SharedMemorySegment createSharedMemory();
    void destroySharedMemory(SharedMemorySegment& segment);

public:
    BenchmarkResult run(int duration_seconds, int iterations, bool verbose) override;
    std::string getName() const override { return "IPC Shared Memory"; }
};

#endif