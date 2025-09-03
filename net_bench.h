#ifndef NET_BENCH_H
#define NET_BENCH_H

#include "benchmark.h"
#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <thread>

class NetworkBenchmark : public Benchmark {
private:
    static constexpr int PORT_BASE = 8080;
    static constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB buffer
    
    std::atomic<bool> server_ready{false};
    std::atomic<bool> should_stop{false};
    
    struct TCPResult {
        double throughput_mbps;
        double avg_latency_ms;
        double p99_latency_ms;
    };
    
    struct UDPResult {
        double throughput_mbps;
        double avg_latency_ms;
        double packet_loss_percent;
    };
    
    TCPResult runTCPBenchmark(int duration_seconds, bool verbose);
    UDPResult runUDPBenchmark(int duration_seconds, bool verbose);
    
    void tcpServer(int port, LatencyStats& stats);
    void tcpClient(int port, int duration_seconds, std::atomic<uint64_t>& bytes_transferred);
    
    void udpServer(int port, LatencyStats& stats, std::atomic<uint64_t>& packets_received);
    void udpClient(int port, int duration_seconds, std::atomic<uint64_t>& packets_sent);
    
public:
    BenchmarkResult run(int duration_seconds, int iterations, bool verbose) override;
    std::string getName() const override { return "Network"; }
};

#endif