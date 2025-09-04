#include "system_monitor.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#endif

// ResourceMetrics implementation
ResourceMetrics::ResourceMetrics()
{
    reset();
}

void ResourceMetrics::reset()
{
    avg_cpu_usage_percent = 0.0;
    per_core_usage.clear();
    cpu_frequency_mhz = 0.0;
    thermal_throttling_detected = false;
    context_switches = 0;
    
    memory_used_mb = 0.0;
    memory_available_mb = 0.0;
    memory_usage_percent = 0.0;
    page_faults = 0;
    cache_hit_ratio = 0.0;
    
    disk_read_mbps = 0.0;
    disk_write_mbps = 0.0;
    avg_io_wait_percent = 0.0;
    disk_operations = 0;
    
    network_rx_mbps = 0.0;
    network_tx_mbps = 0.0;
    
    load_average_1min = 0.0;
    load_average_5min = 0.0;
    active_processes = 0;
    
    monitoring_duration_seconds = 0.0;
    sample_count = 0;
}

std::string ResourceMetrics::toJson() const
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"cpu_usage_percent\": " << avg_cpu_usage_percent << ",\n";
    json << "  \"cpu_frequency_mhz\": " << cpu_frequency_mhz << ",\n";
    json << "  \"thermal_throttling\": " << (thermal_throttling_detected ? "true" : "false") << ",\n";
    json << "  \"memory_used_mb\": " << memory_used_mb << ",\n";
    json << "  \"memory_usage_percent\": " << memory_usage_percent << ",\n";
    json << "  \"disk_read_mbps\": " << disk_read_mbps << ",\n";
    json << "  \"disk_write_mbps\": " << disk_write_mbps << ",\n";
    json << "  \"io_wait_percent\": " << avg_io_wait_percent << ",\n";
    json << "  \"network_rx_mbps\": " << network_rx_mbps << ",\n";
    json << "  \"network_tx_mbps\": " << network_tx_mbps << ",\n";
    json << "  \"load_average_1min\": " << load_average_1min << ",\n";
    json << "  \"load_average_5min\": " << load_average_5min << ",\n";
    json << "  \"active_processes\": " << active_processes << ",\n";
    json << "  \"monitoring_duration_seconds\": " << monitoring_duration_seconds << ",\n";
    json << "  \"sample_count\": " << sample_count << "\n";
    json << "}";
    return json.str();
}

// InterferenceReport implementation
InterferenceReport::InterferenceReport()
    : high_background_cpu_usage(false)
    , memory_pressure(false)
    , high_io_wait(false)
    , network_congestion(false)
    , thermal_throttling(false)
{
}

bool InterferenceReport::hasInterference() const
{
    return high_background_cpu_usage || memory_pressure || high_io_wait || 
           network_congestion || thermal_throttling;
}

std::string InterferenceReport::getSummary() const
{
    if (!hasInterference()) {
        return "No significant system interference detected";
    }
    
    std::ostringstream summary;
    summary << "Performance interference detected: ";
    std::vector<std::string> issues;
    
    if (high_background_cpu_usage) issues.push_back("high background CPU usage");
    if (memory_pressure) issues.push_back("memory pressure");
    if (high_io_wait) issues.push_back("high I/O wait");
    if (network_congestion) issues.push_back("network congestion");
    if (thermal_throttling) issues.push_back("thermal throttling");
    
    for (size_t i = 0; i < issues.size(); ++i) {
        if (i > 0 && i == issues.size() - 1) {
            summary << " and ";
        } else if (i > 0) {
            summary << ", ";
        }
        summary << issues[i];
    }
    
    return summary.str();
}

// SystemMonitor implementation
SystemMonitor::SystemMonitor()
    : monitoring_active(false)
{
}

SystemMonitor::~SystemMonitor()
{
    if (monitoring_active.load()) {
        stopMonitoring();
    }
}

void SystemMonitor::startMonitoring()
{
    if (monitoring_active.load()) {
        return;
    }
    
    reset();
    monitoring_active.store(true);
    monitoring_timer.start();
    
    monitor_thread = std::thread(&SystemMonitor::monitoringLoop, this);
}

void SystemMonitor::stopMonitoring()
{
    if (!monitoring_active.load()) {
        return;
    }
    
    monitoring_active.store(false);
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
}

bool SystemMonitor::isMonitoring() const
{
    return monitoring_active.load();
}

void SystemMonitor::monitoringLoop()
{
    const auto sample_interval = std::chrono::milliseconds(250); // 4 samples per second
    
    while (monitoring_active.load()) {
        try {
            ResourceMetrics current = collectCurrentMetrics();
            samples.push_back(current);
            
            std::this_thread::sleep_for(sample_interval);
        } catch (const std::exception& e) {
            // Continue monitoring even if one sample fails
            std::cerr << "Monitoring sample failed: " << e.what() << std::endl;
        }
    }
}

ResourceMetrics SystemMonitor::collectCurrentMetrics()
{
#ifdef __APPLE__
    return collectMacOSMetrics();
#elif defined(__linux__)
    return collectLinuxMetrics();
#else
    ResourceMetrics metrics;
    return metrics;
#endif
}

ResourceMetrics SystemMonitor::collectLinuxMetrics()
{
    ResourceMetrics metrics;
    
    // CPU usage and frequency
    metrics.per_core_usage = getCPUUsagePerCore();
    if (!metrics.per_core_usage.empty()) {
        metrics.avg_cpu_usage_percent = std::accumulate(
            metrics.per_core_usage.begin(), 
            metrics.per_core_usage.end(), 
            0.0) / metrics.per_core_usage.size();
    }
    metrics.cpu_frequency_mhz = getCPUFrequency();
    metrics.thermal_throttling_detected = detectThermalThrottling();
    
    // Memory information
    getMemoryInfo(metrics.memory_used_mb, metrics.memory_available_mb);
    if (metrics.memory_available_mb > 0) {
        metrics.memory_usage_percent = (metrics.memory_used_mb / 
            (metrics.memory_used_mb + metrics.memory_available_mb)) * 100.0;
    }
    
    // Disk I/O
    getDiskIOStats(metrics.disk_read_mbps, metrics.disk_write_mbps, metrics.disk_operations);
    
    // Network stats
    getNetworkStats(metrics.network_rx_mbps, metrics.network_tx_mbps);
    
    // System load
    getSystemLoad(metrics.load_average_1min, metrics.load_average_5min, metrics.active_processes);
    
    return metrics;
}

ResourceMetrics SystemMonitor::collectMacOSMetrics()
{
    ResourceMetrics metrics;
    
    // CPU usage
    metrics.per_core_usage = getCPUUsagePerCore();
    if (!metrics.per_core_usage.empty()) {
        metrics.avg_cpu_usage_percent = std::accumulate(
            metrics.per_core_usage.begin(), 
            metrics.per_core_usage.end(), 
            0.0) / metrics.per_core_usage.size();
    }
    
    // CPU frequency
    metrics.cpu_frequency_mhz = getCPUFrequency();
    
    // Memory information
    getMemoryInfo(metrics.memory_used_mb, metrics.memory_available_mb);
    if (metrics.memory_available_mb > 0) {
        metrics.memory_usage_percent = (metrics.memory_used_mb / 
            (metrics.memory_used_mb + metrics.memory_available_mb)) * 100.0;
    }
    
    // System load
    getSystemLoad(metrics.load_average_1min, metrics.load_average_5min, metrics.active_processes);
    
    return metrics;
}

std::vector<double> SystemMonitor::getCPUUsagePerCore()
{
    std::vector<double> usage;
    
#ifdef __linux__
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return usage;
    }
    
    std::string line;
    while (std::getline(stat_file, line)) {
        if (line.substr(0, 3) == "cpu" && line[3] >= '0' && line[3] <= '9') {
            std::istringstream ss(line);
            std::string cpu_name;
            uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
            
            ss >> cpu_name >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
            uint64_t active = total - idle - iowait;
            
            if (total > 0) {
                usage.push_back((static_cast<double>(active) / total) * 100.0);
            }
        }
    }
#elif defined(__APPLE__)
    // macOS CPU usage per core is more complex and requires additional frameworks
    // For now, provide overall CPU usage
    mach_port_t host = mach_host_self();
    host_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    
    if (host_statistics(host, HOST_CPU_LOAD_INFO, 
                       (host_info_t)&cpu_info, &count) == KERN_SUCCESS) {
        uint64_t total = cpu_info.cpu_ticks[CPU_STATE_USER] +
                        cpu_info.cpu_ticks[CPU_STATE_SYSTEM] +
                        cpu_info.cpu_ticks[CPU_STATE_IDLE] +
                        cpu_info.cpu_ticks[CPU_STATE_NICE];
        
        uint64_t active = total - cpu_info.cpu_ticks[CPU_STATE_IDLE];
        
        if (total > 0) {
            usage.push_back((static_cast<double>(active) / total) * 100.0);
        }
    }
#endif
    
    return usage;
}

double SystemMonitor::getCPUFrequency()
{
#ifdef __linux__
    std::ifstream freq_file("/proc/cpuinfo");
    if (freq_file.is_open()) {
        std::string line;
        while (std::getline(freq_file, line)) {
            if (line.find("cpu MHz") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    return std::stod(line.substr(colon + 1));
                }
            }
        }
    }
#elif defined(__APPLE__)
    uint64_t freq = 0;
    size_t size = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
        return static_cast<double>(freq) / 1000000.0; // Convert to MHz
    }
#endif
    
    return 0.0;
}

bool SystemMonitor::detectThermalThrottling()
{
#ifdef __linux__
    // Check thermal zones for throttling
    std::ifstream thermal_file("/sys/class/thermal/thermal_zone0/temp");
    if (thermal_file.is_open()) {
        std::string temp_str;
        thermal_file >> temp_str;
        int temp = std::stoi(temp_str);
        // Temperature is in millidegrees Celsius
        return temp > 85000; // Above 85°C suggests potential throttling
    }
#endif
    return false;
}

void SystemMonitor::getMemoryInfo(double& used_mb, double& available_mb)
{
#ifdef __linux__
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        used_mb = available_mb = 0.0;
        return;
    }
    
    std::string line;
    uint64_t total_kb = 0, available_kb = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            std::istringstream ss(line);
            std::string label, value, unit;
            ss >> label >> value >> unit;
            total_kb = std::stoull(value);
        } else if (line.find("MemAvailable:") == 0) {
            std::istringstream ss(line);
            std::string label, value, unit;
            ss >> label >> value >> unit;
            available_kb = std::stoull(value);
            break;
        }
    }
    
    available_mb = available_kb / 1024.0;
    used_mb = (total_kb - available_kb) / 1024.0;
    
#elif defined(__APPLE__)
    mach_port_t host = mach_host_self();
    vm_size_t page_size;
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    
    if (host_page_size(host, &page_size) == KERN_SUCCESS &&
        host_statistics64(host, HOST_VM_INFO64, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
        
        uint64_t used_pages = vm_stats.active_count + vm_stats.wire_count;
        
        used_mb = (used_pages * page_size) / (1024.0 * 1024.0);
        available_mb = (vm_stats.free_count * page_size) / (1024.0 * 1024.0);
    }
#endif
}

void SystemMonitor::getDiskIOStats(double& read_mbps, double& write_mbps, uint64_t& operations)
{
    read_mbps = write_mbps = 0.0;
    operations = 0;
    
#ifdef __linux__
    std::ifstream diskstats("/proc/diskstats");
    if (!diskstats.is_open()) {
        return;
    }
    
    std::string line;
    while (std::getline(diskstats, line)) {
        std::istringstream ss(line);
        std::string device;
        uint64_t reads, writes, read_sectors, write_sectors;
        
        // Skip first 3 fields (major, minor, device name)
        for (int i = 0; i < 3; ++i) {
            ss >> device;
        }
        
        // Skip to read/write stats
        for (int i = 0; i < 3; ++i) {
            ss >> reads;
        }
        ss >> read_sectors;
        for (int i = 0; i < 3; ++i) {
            ss >> writes;
        }
        ss >> write_sectors;
        
        // Sectors are typically 512 bytes
        read_mbps += (read_sectors * 512.0) / (1024.0 * 1024.0);
        write_mbps += (write_sectors * 512.0) / (1024.0 * 1024.0);
        operations += reads + writes;
    }
#endif
}

void SystemMonitor::getNetworkStats(double& rx_mbps, double& tx_mbps)
{
    rx_mbps = tx_mbps = 0.0;
    
#ifdef __linux__
    std::ifstream net_dev("/proc/net/dev");
    if (!net_dev.is_open()) {
        return;
    }
    
    std::string line;
    // Skip header lines
    std::getline(net_dev, line);
    std::getline(net_dev, line);
    
    while (std::getline(net_dev, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        
        std::string interface = line.substr(0, colon);
        // Skip loopback interface
        if (interface.find("lo") != std::string::npos) continue;
        
        std::istringstream ss(line.substr(colon + 1));
        uint64_t rx_bytes, tx_bytes;
        
        ss >> rx_bytes;
        // Skip 7 fields to get to tx_bytes
        for (int i = 0; i < 7; ++i) {
            uint64_t dummy;
            ss >> dummy;
        }
        ss >> tx_bytes;
        
        rx_mbps += rx_bytes / (1024.0 * 1024.0);
        tx_mbps += tx_bytes / (1024.0 * 1024.0);
    }
#endif
}

void SystemMonitor::getSystemLoad(double& load1, double& load5, uint32_t& processes)
{
    load1 = load5 = 0.0;
    processes = 0;
    
#ifdef __linux__
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg.is_open()) {
        loadavg >> load1 >> load5;
    }
    
    // Count processes
    std::ifstream stat_file("/proc/stat");
    if (stat_file.is_open()) {
        std::string line;
        while (std::getline(stat_file, line)) {
            if (line.find("processes") == 0) {
                std::istringstream ss(line);
                std::string label;
                ss >> label >> processes;
                break;
            }
        }
    }
    
#elif defined(__APPLE__)
    double loads[3];
    if (getloadavg(loads, 3) != -1) {
        load1 = loads[0];
        load5 = loads[1];
    }
    
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    size_t size;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) == 0) {
        processes = size / sizeof(struct kinfo_proc);
    }
#endif
}

ResourceMetrics SystemMonitor::getAverageMetrics()
{
    if (samples.empty()) {
        return ResourceMetrics();
    }
    
    ResourceMetrics avg;
    
    for (const auto& sample : samples) {
        avg.avg_cpu_usage_percent += sample.avg_cpu_usage_percent;
        avg.cpu_frequency_mhz += sample.cpu_frequency_mhz;
        avg.memory_used_mb += sample.memory_used_mb;
        avg.memory_available_mb += sample.memory_available_mb;
        avg.memory_usage_percent += sample.memory_usage_percent;
        avg.disk_read_mbps += sample.disk_read_mbps;
        avg.disk_write_mbps += sample.disk_write_mbps;
        avg.avg_io_wait_percent += sample.avg_io_wait_percent;
        avg.network_rx_mbps += sample.network_rx_mbps;
        avg.network_tx_mbps += sample.network_tx_mbps;
        avg.load_average_1min += sample.load_average_1min;
        avg.load_average_5min += sample.load_average_5min;
        avg.active_processes += sample.active_processes;
        
        if (sample.thermal_throttling_detected) {
            avg.thermal_throttling_detected = true;
        }
    }
    
    size_t count = samples.size();
    avg.avg_cpu_usage_percent /= count;
    avg.cpu_frequency_mhz /= count;
    avg.memory_used_mb /= count;
    avg.memory_available_mb /= count;
    avg.memory_usage_percent /= count;
    avg.disk_read_mbps /= count;
    avg.disk_write_mbps /= count;
    avg.avg_io_wait_percent /= count;
    avg.network_rx_mbps /= count;
    avg.network_tx_mbps /= count;
    avg.load_average_1min /= count;
    avg.load_average_5min /= count;
    avg.active_processes /= count;
    
    avg.monitoring_duration_seconds = monitoring_timer.elapsedSeconds();
    avg.sample_count = count;
    
    return avg;
}

ResourceMetrics SystemMonitor::getPeakMetrics()
{
    if (samples.empty()) {
        return ResourceMetrics();
    }
    
    ResourceMetrics peak;
    
    for (const auto& sample : samples) {
        peak.avg_cpu_usage_percent = std::max(peak.avg_cpu_usage_percent, sample.avg_cpu_usage_percent);
        peak.cpu_frequency_mhz = std::max(peak.cpu_frequency_mhz, sample.cpu_frequency_mhz);
        peak.memory_used_mb = std::max(peak.memory_used_mb, sample.memory_used_mb);
        peak.memory_usage_percent = std::max(peak.memory_usage_percent, sample.memory_usage_percent);
        peak.disk_read_mbps = std::max(peak.disk_read_mbps, sample.disk_read_mbps);
        peak.disk_write_mbps = std::max(peak.disk_write_mbps, sample.disk_write_mbps);
        peak.avg_io_wait_percent = std::max(peak.avg_io_wait_percent, sample.avg_io_wait_percent);
        peak.network_rx_mbps = std::max(peak.network_rx_mbps, sample.network_rx_mbps);
        peak.network_tx_mbps = std::max(peak.network_tx_mbps, sample.network_tx_mbps);
        peak.load_average_1min = std::max(peak.load_average_1min, sample.load_average_1min);
        peak.load_average_5min = std::max(peak.load_average_5min, sample.load_average_5min);
        peak.active_processes = std::max(peak.active_processes, sample.active_processes);
        
        if (sample.thermal_throttling_detected) {
            peak.thermal_throttling_detected = true;
        }
    }
    
    peak.monitoring_duration_seconds = monitoring_timer.elapsedSeconds();
    peak.sample_count = samples.size();
    
    return peak;
}

std::vector<ResourceMetrics> SystemMonitor::getAllSamples()
{
    return samples;
}

InterferenceReport SystemMonitor::analyzeInterference()
{
    InterferenceReport report;
    
    if (samples.empty()) {
        return report;
    }
    
    ResourceMetrics avg = getAverageMetrics();
    
    // Define thresholds for interference detection
    const double HIGH_CPU_THRESHOLD = 20.0; // >20% background CPU usage
    const double HIGH_MEMORY_THRESHOLD = 80.0; // >80% memory usage
    const double HIGH_IO_WAIT_THRESHOLD = 10.0; // >10% I/O wait
    const double HIGH_LOAD_THRESHOLD = CPUAffinity::getNumCores() * 0.8; // >80% of cores
    
    report.high_background_cpu_usage = (avg.avg_cpu_usage_percent > HIGH_CPU_THRESHOLD);
    report.memory_pressure = (avg.memory_usage_percent > HIGH_MEMORY_THRESHOLD);
    report.high_io_wait = (avg.avg_io_wait_percent > HIGH_IO_WAIT_THRESHOLD);
    report.thermal_throttling = avg.thermal_throttling_detected;
    
    // Generate specific warnings
    if (report.high_background_cpu_usage) {
        report.performance_warnings.push_back("High background CPU usage detected (" + 
            std::to_string(static_cast<int>(avg.avg_cpu_usage_percent)) + "%)");
    }
    
    if (report.memory_pressure) {
        report.performance_warnings.push_back("High memory usage detected (" + 
            std::to_string(static_cast<int>(avg.memory_usage_percent)) + "%)");
    }
    
    if (report.high_io_wait) {
        report.performance_warnings.push_back("High I/O wait detected (" + 
            std::to_string(static_cast<int>(avg.avg_io_wait_percent)) + "%)");
    }
    
    if (report.thermal_throttling) {
        report.performance_warnings.push_back("CPU thermal throttling detected");
    }
    
    if (avg.load_average_1min > HIGH_LOAD_THRESHOLD) {
        report.performance_warnings.push_back("High system load detected (" + 
            std::to_string(avg.load_average_1min) + ")");
    }
    
    return report;
}

std::vector<std::string> SystemMonitor::getPerformanceRecommendations()
{
    std::vector<std::string> recommendations;
    InterferenceReport interference = analyzeInterference();
    
    if (interference.high_background_cpu_usage) {
        recommendations.push_back("Close unnecessary applications to reduce background CPU usage");
        recommendations.push_back("Check for resource-intensive processes with 'top' or 'htop'");
    }
    
    if (interference.memory_pressure) {
        recommendations.push_back("Close memory-intensive applications");
        recommendations.push_back("Consider increasing system RAM or enabling swap");
    }
    
    if (interference.high_io_wait) {
        recommendations.push_back("Check for disk-intensive processes");
        recommendations.push_back("Consider using faster storage (SSD vs HDD)");
        recommendations.push_back("Verify disk health and available space");
    }
    
    if (interference.thermal_throttling) {
        recommendations.push_back("Check CPU cooling and reduce ambient temperature");
        recommendations.push_back("Clean dust from cooling system");
    }
    
    // General recommendations
    recommendations.push_back("Run benchmarks with minimal background activity");
    recommendations.push_back("Ensure consistent power settings (performance mode)");
    
    return recommendations;
}

void SystemMonitor::reset()
{
    samples.clear();
    accumulated_metrics.reset();
}

bool SystemMonitor::isSystemBusy()
{
    return SystemUtils::isSystemUnderLoad();
}

double SystemMonitor::getSystemLoadThreshold()
{
    return CPUAffinity::getNumCores() * 0.7; // 70% of available cores
}

// SystemUtils namespace implementation
namespace SystemUtils {
    
double getCurrentCPUUsage()
{
    SystemMonitor monitor;
    monitor.startMonitoring();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    monitor.stopMonitoring();
    
    ResourceMetrics metrics = monitor.getAverageMetrics();
    return metrics.avg_cpu_usage_percent;
}

double getCurrentMemoryUsage()
{
    SystemMonitor temp_monitor;
    ResourceMetrics metrics = temp_monitor.collectCurrentMetrics();
    
    if (metrics.memory_used_mb + metrics.memory_available_mb > 0) {
        return (metrics.memory_used_mb / (metrics.memory_used_mb + metrics.memory_available_mb)) * 100.0;
    }
    return 0.0;
}

bool isSystemUnderLoad()
{
    double load = 0.0;
    
#ifdef __linux__
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg.is_open()) {
        loadavg >> load;
    }
#elif defined(__APPLE__)
    double loads[1];
    if (getloadavg(loads, 1) != -1) {
        load = loads[0];
    }
#endif
    
    return load > (CPUAffinity::getNumCores() * 0.7);
}

std::string getSystemPerformanceStatus()
{
    double cpu_usage = getCurrentCPUUsage();
    double mem_usage = getCurrentMemoryUsage();
    bool under_load = isSystemUnderLoad();
    
    std::ostringstream status;
    status << "CPU: " << static_cast<int>(cpu_usage) << "%, ";
    status << "Memory: " << static_cast<int>(mem_usage) << "%";
    
    if (under_load) {
        status << " (System under high load)";
    }
    
    return status.str();
}

}