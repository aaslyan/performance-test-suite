#ifndef PLATFORM_DETECTOR_H
#define PLATFORM_DETECTOR_H

#include <map>
#include <string>
#include <vector>

// Hardware and system configuration information
struct PlatformInfo {
    // CPU information
    std::string cpu_model;
    int cpu_cores;
    int cpu_threads;
    double cpu_base_frequency_ghz;
    double cpu_max_frequency_ghz;
    std::string cpu_architecture;
    bool hyperthreading_enabled;
    std::string cpu_governor; // Linux power governor
    
    // Cache hierarchy
    int l1_cache_size_kb;
    int l2_cache_size_kb;
    int l3_cache_size_kb;
    
    // Memory information
    double total_memory_gb;
    int memory_channels;
    double memory_frequency_mhz;
    std::string memory_type; // DDR4, DDR5, etc.
    bool numa_enabled;
    int numa_nodes;
    
    // Storage information
    std::string primary_storage_type; // NVMe, SATA SSD, HDD
    double storage_capacity_gb;
    std::string filesystem_type;
    
    // Operating system
    std::string os_name;
    std::string os_version;
    std::string kernel_version;
    
    // Virtualization
    bool is_virtualized;
    std::string virtualization_type;
    
    // Performance settings
    bool turbo_boost_enabled;
    std::string power_profile;
    std::vector<std::string> performance_issues;
    
    PlatformInfo();
    std::string toJson() const;
    std::string getSummary() const;
    double getPerformanceScore() const; // 0-100 relative performance estimate
};

// Platform-specific optimization recommendations
struct OptimizationRecommendations {
    std::vector<std::string> cpu_optimizations;
    std::vector<std::string> memory_optimizations;
    std::vector<std::string> storage_optimizations;
    std::vector<std::string> system_optimizations;
    
    std::vector<std::string> getAllRecommendations() const;
    bool hasRecommendations() const;
};

class PlatformDetector {
private:
    PlatformInfo cached_info;
    bool info_cached;
    
    // Detection methods
    void detectCPUInfo(PlatformInfo& info);
    void detectMemoryInfo(PlatformInfo& info);
    void detectStorageInfo(PlatformInfo& info);
    void detectOSInfo(PlatformInfo& info);
    void detectVirtualization(PlatformInfo& info);
    void detectPerformanceSettings(PlatformInfo& info);
    void analyzePerformanceIssues(PlatformInfo& info);
    
    // Platform-specific implementations
    void detectLinuxInfo(PlatformInfo& info);
    void detectMacOSInfo(PlatformInfo& info);
    
    // Helper methods
    std::string readFileContent(const std::string& path);
    std::string executeCommand(const std::string& command);
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    
public:
    PlatformDetector();
    
    // Main detection interface
    PlatformInfo detectPlatform();
    PlatformInfo getCachedPlatformInfo();
    void refreshPlatformInfo();
    
    // Analysis methods
    std::vector<std::string> getPerformanceIssues();
    OptimizationRecommendations getOptimizationRecommendations();
    
    // Comparison utilities
    static bool areComparablePlatforms(const PlatformInfo& info1, const PlatformInfo& info2);
    static std::string comparePerformanceCapability(const PlatformInfo& info1, const PlatformInfo& info2);
    
    // Configuration validation
    bool isOptimalConfiguration() const;
    std::vector<std::string> getConfigurationWarnings() const;
    
    // Utility methods
    static std::string getPerformanceClass(const PlatformInfo& info);
    static bool isHighPerformanceSystem(const PlatformInfo& info);
    static bool isLowPowerSystem(const PlatformInfo& info);
};

// Utility functions for platform detection
namespace PlatformUtils {
    std::string getCurrentPlatform();
    bool isLinux();
    bool isMacOS();
    bool isVirtualized();
    
    // Quick hardware checks
    int getPhysicalCoreCount();
    double getTotalMemoryGB();
    std::string getPrimaryStorageType();
    
    // Performance environment checks
    bool isRunningOnBattery();
    bool isThermalThrottlingLikely();
    bool isSystemIdle();
}

#endif