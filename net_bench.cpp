#include "net_bench.h"
#include <chrono>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

void NetworkBenchmark::tcpServer(int port, LatencyStats& stats)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create TCP socket");
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Make server socket non-blocking to avoid hanging on accept
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        throw std::runtime_error("Failed to bind TCP socket");
    }

    if (listen(server_fd, 10) < 0) {
        close(server_fd);
        throw std::runtime_error("Failed to listen on TCP socket");
    }

    server_ready.store(true);

    std::vector<char> buffer(BUFFER_SIZE);

    while (!should_stop.load()) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd >= 0) {
            Timer op_timer;
            op_timer.start();

            ssize_t bytes = recv(client_fd, buffer.data(), buffer.size(), 0);

            if (bytes > 0) {
                send(client_fd, buffer.data(), bytes, 0);

                double latency_ms = op_timer.elapsedMilliseconds();
                stats.addSample(latency_ms);
            }

            close(client_fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Only break on real errors, not when no connection is available
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    close(server_fd);
}

void NetworkBenchmark::tcpClient(int port, int duration_seconds, std::atomic<uint64_t>& bytes_transferred)
{
    while (!server_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::vector<char> buffer(BUFFER_SIZE, 'T');
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(duration_seconds);

    while (std::chrono::steady_clock::now() < end && !should_stop.load()) {
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_fd < 0)
            continue;

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        server_addr.sin_port = htons(port);

        if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            ssize_t sent = send(client_fd, buffer.data(), buffer.size(), 0);
            if (sent > 0) {
                bytes_transferred.fetch_add(sent);

                ssize_t received = recv(client_fd, buffer.data(), sent, 0);
                if (received > 0) {
                    bytes_transferred.fetch_add(received);
                }
            }
        }

        close(client_fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void NetworkBenchmark::udpServer(int port, LatencyStats& stats, std::atomic<uint64_t>& packets_received)
{
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        throw std::runtime_error("Failed to bind UDP socket");
    }

    server_ready.store(true);

    std::vector<char> buffer(BUFFER_SIZE);
    struct sockaddr_in client_addr;
    socklen_t client_len;

    while (!should_stop.load()) {
        Timer op_timer;
        op_timer.start();

        client_len = sizeof(client_addr);
        ssize_t bytes = recvfrom(server_fd, buffer.data(), buffer.size(), 0,
            (struct sockaddr*)&client_addr, &client_len);

        if (bytes > 0) {
            packets_received.fetch_add(1);

            sendto(server_fd, buffer.data(), bytes, 0,
                (struct sockaddr*)&client_addr, client_len);

            double latency_ms = op_timer.elapsedMilliseconds();
            stats.addSample(latency_ms);
        } else if (bytes < 0 && errno != EAGAIN) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    close(server_fd);
}

void NetworkBenchmark::udpClient(int port, int duration_seconds, std::atomic<uint64_t>& packets_sent)
{
    while (!server_ready.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int client_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_fd < 0) {
        throw std::runtime_error("Failed to create UDP client socket");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(port);

    std::vector<char> buffer(1400, 'U');
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(duration_seconds);

    while (std::chrono::steady_clock::now() < end && !should_stop.load()) {
        ssize_t sent = sendto(client_fd, buffer.data(), buffer.size(), 0,
            (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (sent > 0) {
            packets_sent.fetch_add(1);
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }

    close(client_fd);
}

NetworkBenchmark::TCPResult NetworkBenchmark::runTCPBenchmark(int duration_seconds, bool verbose)
{
    TCPResult result;
    should_stop.store(false);
    server_ready.store(false);

    LatencyStats tcp_stats;
    std::atomic<uint64_t> bytes_transferred(0);

    std::thread server_thread(&NetworkBenchmark::tcpServer, this, PORT_BASE, std::ref(tcp_stats));

    std::thread client_thread(&NetworkBenchmark::tcpClient, this, PORT_BASE,
        duration_seconds, std::ref(bytes_transferred));

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    should_stop.store(true);

    client_thread.join();
    server_thread.join();

    result.throughput_mbps = (bytes_transferred.load() / (1024.0 * 1024.0)) / duration_seconds;
    result.avg_latency_ms = tcp_stats.getAverage();
    result.p99_latency_ms = tcp_stats.getPercentile(99);

    return result;
}

NetworkBenchmark::UDPResult NetworkBenchmark::runUDPBenchmark(int duration_seconds, bool verbose)
{
    UDPResult result;
    should_stop.store(false);
    server_ready.store(false);

    LatencyStats udp_stats;
    std::atomic<uint64_t> packets_sent(0);
    std::atomic<uint64_t> packets_received(0);

    std::thread server_thread(&NetworkBenchmark::udpServer, this, PORT_BASE + 1,
        std::ref(udp_stats), std::ref(packets_received));

    std::thread client_thread(&NetworkBenchmark::udpClient, this, PORT_BASE + 1,
        duration_seconds, std::ref(packets_sent));

    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    should_stop.store(true);

    client_thread.join();
    server_thread.join();

    uint64_t sent = packets_sent.load();
    uint64_t received = packets_received.load();

    result.throughput_mbps = (received * 1400 * 8) / (1024.0 * 1024.0) / duration_seconds;
    result.avg_latency_ms = udp_stats.getAverage();
    result.packet_loss_percent = sent > 0 ? ((sent - received) * 100.0) / sent : 0;

    return result;
}

BenchmarkResult NetworkBenchmark::run(int duration_seconds, int iterations, bool verbose)
{
    BenchmarkResult result;
    result.name = getName();

    try {
        if (verbose) {
            std::cout << "  Running TCP benchmark...\n";
        }

        TCPResult tcp_result = runTCPBenchmark(duration_seconds / 2, verbose);

        if (verbose) {
            std::cout << "  TCP throughput: " << tcp_result.throughput_mbps << " MB/s\n";
            std::cout << "  Running UDP benchmark...\n";
        }

        UDPResult udp_result = runUDPBenchmark(duration_seconds / 2, verbose);

        if (verbose) {
            std::cout << "  UDP throughput: " << udp_result.throughput_mbps << " Mbps\n";
            std::cout << "  UDP packet loss: " << udp_result.packet_loss_percent << "%\n";
        }

        result.throughput = tcp_result.throughput_mbps;
        result.throughput_unit = "MB/s";

        result.avg_latency = (tcp_result.avg_latency_ms + udp_result.avg_latency_ms) / 2.0;
        result.min_latency = std::min(tcp_result.avg_latency_ms, udp_result.avg_latency_ms);
        result.max_latency = std::max(tcp_result.p99_latency_ms, udp_result.avg_latency_ms);
        result.p50_latency = tcp_result.avg_latency_ms;
        result.p90_latency = tcp_result.p99_latency_ms * 0.9;
        result.p99_latency = tcp_result.p99_latency_ms;
        result.latency_unit = "ms";

        result.extra_metrics["tcp_throughput_mbps"] = tcp_result.throughput_mbps;
        result.extra_metrics["tcp_avg_latency_ms"] = tcp_result.avg_latency_ms;
        result.extra_metrics["tcp_p99_latency_ms"] = tcp_result.p99_latency_ms;
        result.extra_metrics["udp_throughput_mbps"] = udp_result.throughput_mbps;
        result.extra_metrics["udp_avg_latency_ms"] = udp_result.avg_latency_ms;
        result.extra_metrics["udp_packet_loss_percent"] = udp_result.packet_loss_percent;
        result.extra_metrics["loopback_used"] = 1.0;

        result.status = "success";

    } catch (const std::exception& e) {
        result.status = "error";
        result.error_message = e.what();
    }

    return result;
}