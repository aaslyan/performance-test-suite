#include "platform_detector.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdint>

#ifdef __APPLE__
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/statvfs.h>
#include <unistd.h>
#endif

// PlatformInfo implementation
PlatformInfo::PlatformInfo()
{
    cpu_cores = 0;
    cpu_threads = 0;
    cpu_base_frequency_ghz = 0.0;
    cpu_max_frequency_ghz = 0.0;
    hyperthreading_enabled = false;
    
    l1_cache_size_kb = 0;
    l2_cache_size_kb = 0;
    l3_cache_size_kb = 0;
    
    total_memory_gb = 0.0;
    memory_channels = 0;
    memory_frequency_mhz = 0.0;
    numa_enabled = false;
    numa_nodes = 0;
    
    storage_capacity_gb = 0.0;
    
    is_virtualized = false;
    turbo_boost_enabled = false;
}

std::string PlatformInfo::toJson() const
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"cpu_model\": \"" << cpu_model << "\",\n";
    json << "  \"cpu_cores\": " << cpu_cores << ",\n";
    json << "  \"cpu_threads\": " << cpu_threads << ",\n";
    json << "  \"cpu_base_frequency_ghz\": " << cpu_base_frequency_ghz << ",\n";
    json << "  \"cpu_max_frequency_ghz\": " << cpu_max_frequency_ghz << ",\n";
    json << "  \"cpu_architecture\": \"" << cpu_architecture << "\",\n";
    json << "  \"hyperthreading_enabled\": " << (hyperthreading_enabled ? "true" : "false") << ",\n";
    json << "  \"cpu_governor\": \"" << cpu_governor << "\",\n";
    json << "  \"l1_cache_size_kb\": " << l1_cache_size_kb << ",\n";
    json << "  \"l2_cache_size_kb\": " << l2_cache_size_kb << ",\n";
    json << "  \"l3_cache_size_kb\": " << l3_cache_size_kb << ",\n";
    json << "  \"total_memory_gb\": " << total_memory_gb << ",\n";
    json << "  \"memory_channels\": " << memory_channels << ",\n";
    json << "  \"memory_frequency_mhz\": " << memory_frequency_mhz << ",\n";
    json << "  \"memory_type\": \"" << memory_type << "\",\n";
    json << "  \"numa_enabled\": " << (numa_enabled ? "true" : "false") << ",\n";
    json << "  \"numa_nodes\": " << numa_nodes << ",\n";
    json << "  \"primary_storage_type\": \"" << primary_storage_type << "\",\n";
    json << "  \"storage_capacity_gb\": " << storage_capacity_gb << ",\n";
    json << "  \"filesystem_type\": \"" << filesystem_type << "\",\n";
    json << "  \"os_name\": \"" << os_name << "\",\n";
    json << "  \"os_version\": \"" << os_version << "\",\n";
    json << "  \"kernel_version\": \"" << kernel_version << "\",\n";
    json << "  \"is_virtualized\": " << (is_virtualized ? "true" : "false") << ",\n";
    json << "  \"virtualization_type\": \"" << virtualization_type << "\",\n";
    json << "  \"turbo_boost_enabled\": " << (turbo_boost_enabled ? "true" : "false") << ",\n";
    json << "  \"power_profile\": \"" << power_profile << "\",\n";
    json << "  \"performance_score\": " << getPerformanceScore() << "\n";
    json << "}";
    return json.str();
}

std::string PlatformInfo::getSummary() const
{
    std::ostringstream summary;
    summary << cpu_model;
    if (cpu_cores > 0) {
        summary << " (" << cpu_cores;
        if (cpu_threads > cpu_cores) {
            summary << "/" << cpu_threads;
        }
        summary << " cores)";
    }
    if (cpu_max_frequency_ghz > 0) {
        summary << " @ " << cpu_max_frequency_ghz << " GHz";
    }
    if (total_memory_gb > 0) {
        summary << ", " << total_memory_gb << " GB RAM";
    }
    if (!primary_storage_type.empty()) {
        summary << ", " << primary_storage_type;
    }
    if (!os_name.empty()) {
        summary << " on " << os_name;
    }
    if (is_virtualized) {
        summary << " (Virtualized)";
    }
    return summary.str();
}

double PlatformInfo::getPerformanceScore() const
{
    double score = 50.0; // Base score
    
    // CPU contribution (40% of total score)
    if (cpu_cores > 0) {
        score += std::min(20.0, cpu_cores * 2.5); // Up to 20 points for cores
    }
    if (cpu_max_frequency_ghz > 0) {
        score += std::min(20.0, (cpu_max_frequency_ghz - 1.0) * 10.0); // Up to 20 points for frequency
    }
    
    // Memory contribution (30% of total score)
    if (total_memory_gb > 0) {
        score += std::min(20.0, total_memory_gb / 2.0); // Up to 20 points for memory
    }
    if (memory_frequency_mhz > 0) {
        score += std::min(10.0, (memory_frequency_mhz - 1600.0) / 400.0); // Up to 10 points for memory speed
    }
    
    // Storage contribution (20% of total score)
    if (primary_storage_type == "NVMe") {
        score += 15.0;
    } else if (primary_storage_type == "SATA SSD") {
        score += 10.0;
    } else if (primary_storage_type == "HDD") {
        score += 2.0;
    }
    
    // Cache contribution (10% of total score)
    if (l3_cache_size_kb > 0) {
        score += std::min(5.0, l3_cache_size_kb / 1024.0); // Up to 5 points for L3 cache
    }
    if (l2_cache_size_kb > 0) {
        score += std::min(5.0, l2_cache_size_kb / 512.0); // Up to 5 points for L2 cache
    }
    
    // Performance penalties
    if (is_virtualized) {
        score *= 0.8; // 20% penalty for virtualization
    }
    if (!turbo_boost_enabled) {
        score *= 0.9; // 10% penalty for no turbo boost
    }
    if (cpu_governor == "powersave") {
        score *= 0.7; // 30% penalty for power saving mode
    }
    
    return std::max(0.0, std::min(100.0, score));
}

// OptimizationRecommendations implementation
std::vector<std::string> OptimizationRecommendations::getAllRecommendations() const
{
    std::vector<std::string> all;
    all.insert(all.end(), cpu_optimizations.begin(), cpu_optimizations.end());
    all.insert(all.end(), memory_optimizations.begin(), memory_optimizations.end());
    all.insert(all.end(), storage_optimizations.begin(), storage_optimizations.end());
    all.insert(all.end(), system_optimizations.begin(), system_optimizations.end());
    return all;
}

bool OptimizationRecommendations::hasRecommendations() const
{
    return !cpu_optimizations.empty() || !memory_optimizations.empty() ||
           !storage_optimizations.empty() || !system_optimizations.empty();
}

// PlatformDetector implementation
PlatformDetector::PlatformDetector()
    : info_cached(false)
{
}

PlatformInfo PlatformDetector::detectPlatform()
{
    if (info_cached) {
        return cached_info;
    }
    
    PlatformInfo info;
    
    detectCPUInfo(info);
    detectMemoryInfo(info);
    detectStorageInfo(info);
    detectOSInfo(info);
    detectVirtualization(info);
    detectPerformanceSettings(info);
    analyzePerformanceIssues(info);
    
    cached_info = info;
    info_cached = true;
    
    return info;
}

PlatformInfo PlatformDetector::getCachedPlatformInfo()
{
    if (!info_cached) {
        return detectPlatform();
    }
    return cached_info;
}

void PlatformDetector::refreshPlatformInfo()
{
    info_cached = false;
    detectPlatform();
}

void PlatformDetector::detectCPUInfo(PlatformInfo& info)
{
#ifdef __APPLE__
    detectMacOSInfo(info);
#elif defined(__linux__)
    detectLinuxInfo(info);
#endif
}

void PlatformDetector::detectLinuxInfo(PlatformInfo& info)
{
    // Read /proc/cpuinfo for CPU details
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        int core_count = 0;
        
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos && info.cpu_model.empty()) {
                    info.cpu_model = line.substr(colon + 2);
                }
            } else if (line.find("processor") != std::string::npos) {
                core_count++;
            } else if (line.find("cpu MHz") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    double freq_mhz = std::stod(line.substr(colon + 2));
                    info.cpu_base_frequency_ghz = freq_mhz / 1000.0;
                }
            } else if (line.find("cache size") != std::string::npos) {
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string cache_str = line.substr(colon + 2);
                    if (cache_str.find("KB") != std::string::npos) {
                        info.l3_cache_size_kb = std::stoi(cache_str);
                    }
                }
            }
        }
        
        info.cpu_threads = core_count;
    }
    
    // Get physical core count
    std::ifstream core_siblings("/sys/devices/system/cpu/cpu0/topology/core_siblings_list");
    if (core_siblings.is_open()) {
        std::string siblings;
        std::getline(core_siblings, siblings);
        // Count commas + 1 to get core count
        info.cpu_cores = std::count(siblings.begin(), siblings.end(), ',') + 1;
    }
    
    info.hyperthreading_enabled = (info.cpu_threads > info.cpu_cores);
    
    // CPU governor
    std::ifstream governor_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    if (governor_file.is_open()) {
        std::getline(governor_file, info.cpu_governor);
    }
    
    // Max frequency
    std::ifstream max_freq_file("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (max_freq_file.is_open()) {
        std::string max_freq_str;
        std::getline(max_freq_file, max_freq_str);
        if (!max_freq_str.empty()) {
            double max_freq_khz = std::stod(max_freq_str);
            info.cpu_max_frequency_ghz = max_freq_khz / 1000000.0;
        }
    }
    
    // Architecture
    std::string arch_output = executeCommand("uname -m");
    if (!arch_output.empty()) {
        info.cpu_architecture = arch_output.substr(0, arch_output.find('\n'));
    }
}

#ifdef __APPLE__
void PlatformDetector::detectMacOSInfo(PlatformInfo& info)
{
    // CPU model
    char cpu_brand[256];
    size_t size = sizeof(cpu_brand);
    if (sysctlbyname("machdep.cpu.brand_string", cpu_brand, &size, NULL, 0) == 0) {
        info.cpu_model = std::string(cpu_brand);
    }
    
    // CPU cores and threads
    int cores, threads;
    size = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &size, NULL, 0) == 0) {
        info.cpu_cores = cores;
    }
    
    size = sizeof(threads);
    if (sysctlbyname("hw.logicalcpu", &threads, &size, NULL, 0) == 0) {
        info.cpu_threads = threads;
    }
    
    info.hyperthreading_enabled = (info.cpu_threads > info.cpu_cores);
    
    // CPU frequency
    uint64_t freq;
    size = sizeof(freq);
    if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
        info.cpu_max_frequency_ghz = static_cast<double>(freq) / 1000000000.0;
    }
    
    // Cache sizes
    uint64_t cache_size;
    size = sizeof(cache_size);
    if (sysctlbyname("hw.l1icachesize", &cache_size, &size, NULL, 0) == 0) {
        info.l1_cache_size_kb = static_cast<int>(cache_size / 1024);
    }
    
    size = sizeof(cache_size);
    if (sysctlbyname("hw.l2cachesize", &cache_size, &size, NULL, 0) == 0) {
        info.l2_cache_size_kb = static_cast<int>(cache_size / 1024);
    }
    
    size = sizeof(cache_size);
    if (sysctlbyname("hw.l3cachesize", &cache_size, &size, NULL, 0) == 0) {
        info.l3_cache_size_kb = static_cast<int>(cache_size / 1024);
    }
    
    // Architecture
    info.cpu_architecture = "Apple Silicon"; // Assume modern Macs
}
#else
void PlatformDetector::detectMacOSInfo(PlatformInfo& info)
{
    // Empty stub for non-macOS platforms
    (void)info; // Suppress unused parameter warning
}
#endif

void PlatformDetector::detectMemoryInfo(PlatformInfo& info)
{
#ifdef __APPLE__
    uint64_t memsize;
    size_t size = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) == 0) {
        info.total_memory_gb = static_cast<double>(memsize) / (1024.0 * 1024.0 * 1024.0);
    }
    
#elif defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                std::istringstream ss(line);
                std::string label, value, unit;
                ss >> label >> value >> unit;
                uint64_t total_kb = std::stoull(value);
                info.total_memory_gb = total_kb / (1024.0 * 1024.0);
                break;
            }
        }
    }
    
    // NUMA information
    std::string numa_output = executeCommand("ls /sys/devices/system/node/ | grep node | wc -l");
    if (!numa_output.empty()) {
        info.numa_nodes = std::stoi(numa_output);
        info.numa_enabled = (info.numa_nodes > 1);
    }
#endif
}

void PlatformDetector::detectStorageInfo(PlatformInfo& info)
{
    // Try to detect primary storage type
#ifdef __linux__
    std::string lsblk_output = executeCommand("lsblk -d -o NAME,ROTA 2>/dev/null | head -2 | tail -1");
    if (!lsblk_output.empty()) {
        if (lsblk_output.find("0") != std::string::npos) {
            // Non-rotating storage (SSD/NVMe)
            std::string nvme_check = executeCommand("lsblk -d -o NAME | grep nvme");
            if (!nvme_check.empty()) {
                info.primary_storage_type = "NVMe";
            } else {
                info.primary_storage_type = "SATA SSD";
            }
        } else {
            info.primary_storage_type = "HDD";
        }
    }
    
    // File system type
    std::string fs_output = executeCommand("df -T / | tail -1 | awk '{print $2}'");
    if (!fs_output.empty()) {
        info.filesystem_type = fs_output.substr(0, fs_output.find('\n'));
    }
    
#elif defined(__APPLE__)
    // macOS storage detection
    std::string diskutil_output = executeCommand("diskutil info / | grep 'Solid State'");
    if (!diskutil_output.empty()) {
        info.primary_storage_type = "NVMe"; // Modern Macs use NVMe SSDs
    } else {
        info.primary_storage_type = "Unknown";
    }
    
    // File system
    std::string fs_output = executeCommand("df -T / | tail -1 | awk '{print $2}'");
    if (!fs_output.empty()) {
        info.filesystem_type = fs_output.substr(0, fs_output.find('\n'));
    }
#endif
    
    // Storage capacity
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        info.storage_capacity_gb = static_cast<double>(stat.f_blocks * stat.f_frsize) / (1024.0 * 1024.0 * 1024.0);
    }
}

void PlatformDetector::detectOSInfo(PlatformInfo& info)
{
    std::string uname_output = executeCommand("uname -s");
    if (!uname_output.empty()) {
        info.os_name = uname_output.substr(0, uname_output.find('\n'));
    }
    
    std::string version_output = executeCommand("uname -r");
    if (!version_output.empty()) {
        info.kernel_version = version_output.substr(0, version_output.find('\n'));
    }
    
#ifdef __APPLE__
    std::string sw_vers_output = executeCommand("sw_vers -productVersion");
    if (!sw_vers_output.empty()) {
        info.os_version = sw_vers_output.substr(0, sw_vers_output.find('\n'));
    }
    
#elif defined(__linux__)
    std::ifstream os_release("/etc/os-release");
    if (os_release.is_open()) {
        std::string line;
        while (std::getline(os_release, line)) {
            if (line.find("PRETTY_NAME=") == 0) {
                size_t quote1 = line.find('"');
                size_t quote2 = line.find('"', quote1 + 1);
                if (quote1 != std::string::npos && quote2 != std::string::npos) {
                    info.os_version = line.substr(quote1 + 1, quote2 - quote1 - 1);
                }
                break;
            }
        }
    }
#endif
}

void PlatformDetector::detectVirtualization(PlatformInfo& info)
{
    // Check common virtualization indicators
    std::string dmi_output = executeCommand("sudo dmidecode -s system-manufacturer 2>/dev/null");
    if (dmi_output.find("VMware") != std::string::npos) {
        info.is_virtualized = true;
        info.virtualization_type = "VMware";
    } else if (dmi_output.find("QEMU") != std::string::npos) {
        info.is_virtualized = true;
        info.virtualization_type = "QEMU/KVM";
    } else if (dmi_output.find("VirtualBox") != std::string::npos) {
        info.is_virtualized = true;
        info.virtualization_type = "VirtualBox";
    }
    
    // Check for Docker/container
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("docker") != std::string::npos || 
                line.find("containerd") != std::string::npos) {
                info.is_virtualized = true;
                info.virtualization_type = "Container";
                break;
            }
        }
    }
}

void PlatformDetector::detectPerformanceSettings(PlatformInfo& info)
{
#ifdef __linux__
    // Check if turbo boost is enabled
    std::ifstream turbo_file("/sys/devices/system/cpu/intel_pstate/no_turbo");
    if (turbo_file.is_open()) {
        std::string turbo_status;
        std::getline(turbo_file, turbo_status);
        info.turbo_boost_enabled = (turbo_status == "0");
    }
    
    // Power profile
    if (info.cpu_governor == "performance") {
        info.power_profile = "High Performance";
    } else if (info.cpu_governor == "powersave") {
        info.power_profile = "Power Saving";
    } else {
        info.power_profile = "Balanced";
    }
    
#elif defined(__APPLE__)
    // macOS power settings are harder to detect programmatically
    info.turbo_boost_enabled = true; // Assume enabled on macOS
    info.power_profile = "Adaptive";
#endif
}

void PlatformDetector::analyzePerformanceIssues(PlatformInfo& info)
{
    if (info.cpu_governor == "powersave") {
        info.performance_issues.push_back("CPU governor set to 'powersave' mode");
    }
    
    if (!info.turbo_boost_enabled) {
        info.performance_issues.push_back("CPU turbo boost is disabled");
    }
    
    if (info.is_virtualized) {
        info.performance_issues.push_back("Running in virtualized environment");
    }
    
    if (info.total_memory_gb < 8.0) {
        info.performance_issues.push_back("Low system memory (< 8GB)");
    }
    
    if (info.primary_storage_type == "HDD") {
        info.performance_issues.push_back("Using traditional hard drive storage");
    }
    
    if (info.cpu_cores < 4) {
        info.performance_issues.push_back("Low CPU core count (< 4 cores)");
    }
    
    if (info.cpu_max_frequency_ghz < 2.0) {
        info.performance_issues.push_back("Low CPU frequency (< 2.0 GHz)");
    }
}

std::vector<std::string> PlatformDetector::getPerformanceIssues()
{
    PlatformInfo info = getCachedPlatformInfo();
    return info.performance_issues;
}

OptimizationRecommendations PlatformDetector::getOptimizationRecommendations()
{
    OptimizationRecommendations recommendations;
    PlatformInfo info = getCachedPlatformInfo();
    
    // CPU optimizations
    if (info.cpu_governor == "powersave") {
        recommendations.cpu_optimizations.push_back("Set CPU governor to 'performance' mode");
        recommendations.cpu_optimizations.push_back("sudo cpupower frequency-set -g performance");
    }
    
    if (!info.turbo_boost_enabled) {
        recommendations.cpu_optimizations.push_back("Enable CPU turbo boost");
        recommendations.cpu_optimizations.push_back("echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo");
    }
    
    // Memory optimizations
    if (info.total_memory_gb < 8.0) {
        recommendations.memory_optimizations.push_back("Consider upgrading system memory to 8GB or more");
    }
    
    if (info.numa_enabled && info.numa_nodes > 1) {
        recommendations.memory_optimizations.push_back("Consider NUMA-aware application tuning");
        recommendations.memory_optimizations.push_back("Use 'numactl' for memory binding in benchmarks");
    }
    
    // Storage optimizations
    if (info.primary_storage_type == "HDD") {
        recommendations.storage_optimizations.push_back("Consider upgrading to SSD storage for better I/O performance");
    }
    
    if (info.filesystem_type == "ext4") {
        recommendations.storage_optimizations.push_back("Consider using a higher-performance filesystem like XFS or Btrfs");
    }
    
    // System optimizations
    if (info.is_virtualized) {
        recommendations.system_optimizations.push_back("For best performance, run benchmarks on bare metal");
        recommendations.system_optimizations.push_back("Ensure VM has adequate CPU and memory allocation");
    }
    
    recommendations.system_optimizations.push_back("Disable unnecessary services during benchmarking");
    recommendations.system_optimizations.push_back("Set CPU affinity for benchmark processes");
    
    return recommendations;
}

bool PlatformDetector::areComparablePlatforms(const PlatformInfo& info1, const PlatformInfo& info2)
{
    // Platforms are comparable if they have similar performance characteristics
    double score_diff = std::abs(info1.getPerformanceScore() - info2.getPerformanceScore());
    
    // Allow 20% performance score difference for "comparable"
    return score_diff <= 20.0;
}

std::string PlatformDetector::comparePerformanceCapability(const PlatformInfo& info1, const PlatformInfo& info2)
{
    double score1 = info1.getPerformanceScore();
    double score2 = info2.getPerformanceScore();
    double diff = score1 - score2;
    
    std::ostringstream comparison;
    
    if (std::abs(diff) < 5.0) {
        comparison << "Platforms have similar performance capability";
    } else if (diff > 0) {
        comparison << "Platform 1 is approximately " << static_cast<int>(diff) << "% more capable";
    } else {
        comparison << "Platform 2 is approximately " << static_cast<int>(-diff) << "% more capable";
    }
    
    return comparison.str();
}

bool PlatformDetector::isOptimalConfiguration() const
{
    if (!info_cached) return false;
    
    return cached_info.performance_issues.empty() &&
           cached_info.turbo_boost_enabled &&
           cached_info.cpu_governor != "powersave" &&
           !cached_info.is_virtualized;
}

std::vector<std::string> PlatformDetector::getConfigurationWarnings() const
{
    if (!info_cached) return {};
    
    return cached_info.performance_issues;
}

std::string PlatformDetector::getPerformanceClass(const PlatformInfo& info)
{
    double score = info.getPerformanceScore();
    
    if (score >= 80.0) return "High Performance";
    if (score >= 60.0) return "Medium Performance";
    if (score >= 40.0) return "Low Performance";
    return "Very Low Performance";
}

bool PlatformDetector::isHighPerformanceSystem(const PlatformInfo& info)
{
    return info.getPerformanceScore() >= 70.0;
}

bool PlatformDetector::isLowPowerSystem(const PlatformInfo& info)
{
    return info.cpu_governor == "powersave" ||
           info.power_profile.find("Power Saving") != std::string::npos ||
           info.cpu_max_frequency_ghz < 2.0;
}

std::string PlatformDetector::readFileContent(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

std::string PlatformDetector::executeCommand(const std::string& command)
{
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            result += buffer;
        }
        pclose(pipe);
    }
    return result;
}

std::vector<std::string> PlatformDetector::splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

// PlatformUtils namespace implementation
namespace PlatformUtils {

std::string getCurrentPlatform()
{
#ifdef __APPLE__
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

bool isLinux()
{
#ifdef __linux__
    return true;
#else
    return false;
#endif
}

bool isMacOS()
{
#ifdef __APPLE__
    return true;
#else
    return false;
#endif
}

bool isVirtualized()
{
    PlatformDetector detector;
    PlatformInfo info = detector.detectPlatform();
    return info.is_virtualized;
}

int getPhysicalCoreCount()
{
    PlatformDetector detector;
    PlatformInfo info = detector.detectPlatform();
    return info.cpu_cores;
}

double getTotalMemoryGB()
{
    PlatformDetector detector;
    PlatformInfo info = detector.detectPlatform();
    return info.total_memory_gb;
}

std::string getPrimaryStorageType()
{
    PlatformDetector detector;
    PlatformInfo info = detector.detectPlatform();
    return info.primary_storage_type;
}

bool isRunningOnBattery()
{
#ifdef __APPLE__
    std::string power_source = "";
    FILE* pipe = popen("pmset -g batt | head -1", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            power_source = std::string(buffer);
        }
        pclose(pipe);
    }
    return power_source.find("Battery Power") != std::string::npos;
#elif defined(__linux__)
    std::ifstream ac_power("/sys/class/power_supply/AC/online");
    if (ac_power.is_open()) {
        std::string status;
        std::getline(ac_power, status);
        return status == "0";
    }
#endif
    return false;
}

bool isThermalThrottlingLikely()
{
#ifdef __linux__
    std::ifstream thermal_file("/sys/class/thermal/thermal_zone0/temp");
    if (thermal_file.is_open()) {
        std::string temp_str;
        thermal_file >> temp_str;
        int temp = std::stoi(temp_str);
        return temp > 80000; // Above 80°C
    }
#endif
    return false;
}

bool isSystemIdle()
{
#ifdef __linux__
    std::ifstream loadavg("/proc/loadavg");
    if (loadavg.is_open()) {
        double load;
        loadavg >> load;
        return load < 0.5; // System is idle if load < 0.5
    }
#elif defined(__APPLE__)
    double loads[1];
    if (getloadavg(loads, 1) != -1) {
        return loads[0] < 0.5;
    }
#endif
    return true; // Assume idle if we can't determine
}

}
