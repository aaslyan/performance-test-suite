#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "utils.h"
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>

// Resource metrics collected during benchmark execution
struct ResourceMetrics {
    // CPU metrics
    double avg_cpu_usage_percent;
    std::vector<double> per_core_usage;
    double cpu_frequency_mhz;
    bool thermal_throttling_detected;
    uint64_t context_switches;
    
    // Memory metrics
    double memory_used_mb;
    double memory_available_mb;
    double memory_usage_percent;
    uint64_t page_faults;
    double cache_hit_ratio;
    
    // I/O metrics
    double disk_read_mbps;
    double disk_write_mbps;
    double avg_io_wait_percent;
    uint64_t disk_operations;
    
    // Network metrics
    double network_rx_mbps;
    double network_tx_mbps;
    
    // System load
    double load_average_1min;
    double load_average_5min;
    uint32_t active_processes;
    
    // Timing
    double monitoring_duration_seconds;
    size_t sample_count;
    
    ResourceMetrics();
    void reset();
    std::string toJson() const;
};

// System interference detection
struct InterferenceReport {
    bool high_background_cpu_usage;
    bool memory_pressure;
    bool high_io_wait;
    bool network_congestion;
    bool thermal_throttling;
    std::vector<std::string> performance_warnings;
    
    InterferenceReport();
    bool hasInterference() const;
    std::string getSummary() const;
};

class SystemMonitor {
private:
    std::atomic<bool> monitoring_active;
    std::thread monitor_thread;
    ResourceMetrics accumulated_metrics;
    std::vector<ResourceMetrics> samples;
    Timer monitoring_timer;
    
    // Platform-specific monitoring implementations
    void monitoringLoop();
    ResourceMetrics collectLinuxMetrics();
    ResourceMetrics collectMacOSMetrics();
    
    // Helper methods
    std::vector<double> getCPUUsagePerCore();
    double getCPUFrequency();
    bool detectThermalThrottling();
    void getMemoryInfo(double& used_mb, double& available_mb);
    void getDiskIOStats(double elapsed_seconds, double& read_mbps, double& write_mbps, uint64_t& operations);
    void getNetworkStats(double elapsed_seconds, double& rx_mbps, double& tx_mbps);
    void getSystemLoad(double& load1, double& load5, uint32_t& processes);

#ifdef __linux__
    struct CpuTimes {
        uint64_t user{0};
        uint64_t nice{0};
        uint64_t system{0};
        uint64_t idle{0};
        uint64_t iowait{0};
        uint64_t irq{0};
        uint64_t softirq{0};
        uint64_t steal{0};
        uint64_t total() const { return user + nice + system + idle + iowait + irq + softirq + steal; }
        uint64_t active() const { return total() - idle - iowait; }
    };

    std::vector<CpuTimes> readCpuTimes(CpuTimes& total_times);
    std::vector<CpuTimes> previous_cpu_times;
    CpuTimes previous_cpu_total;
    bool cpu_times_initialized{false};
    double last_io_wait_percent{0.0};
    bool has_last_sample_time{false};
    std::chrono::steady_clock::time_point last_sample_time{};

    struct DiskSnapshot {
        uint64_t read_sectors{0};
        uint64_t write_sectors{0};
        uint64_t reads_completed{0};
        uint64_t writes_completed{0};
    };
    std::map<std::string, DiskSnapshot> previous_disk_stats;

    struct NetSnapshot {
        uint64_t rx_bytes{0};
        uint64_t tx_bytes{0};
    };
    std::map<std::string, NetSnapshot> previous_net_stats;
#endif

public:
    SystemMonitor();
    ~SystemMonitor();
    
    // Monitoring control
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    
    // Results retrieval
    ResourceMetrics getAverageMetrics();
    ResourceMetrics getPeakMetrics();
    std::vector<ResourceMetrics> getAllSamples();
    ResourceMetrics collectCurrentMetrics();
    
    // Analysis
    InterferenceReport analyzeInterference();
    std::vector<std::string> getPerformanceRecommendations();
    
    // Utility methods
    void reset();
    static bool isSystemBusy();
    static double getSystemLoadThreshold();
};

// Static utility functions for quick checks
namespace SystemUtils {
    double getCurrentCPUUsage();
    double getCurrentMemoryUsage();
    bool isSystemUnderLoad();
    std::string getSystemPerformanceStatus();
}

#endif