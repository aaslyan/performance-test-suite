#ifndef UTILS_H
#define UTILS_H

// Define _GNU_SOURCE before any includes for Linux CPU affinity functions
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Time conversion constants for consistent nanosecond-based timing
static constexpr double NANOSECONDS_PER_SECOND = 1000000000.0;
static constexpr double NANOSECONDS_PER_MILLISECOND = 1000000.0;
static constexpr double MICROSECONDS_PER_SECOND = 1000000.0;
static constexpr double MILLISECONDS_PER_SECOND = 1000.0;
static constexpr double MIN_MEASURABLE_TIME_NS = 1000000.0; // 1ms minimum for reliable measurements

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

class LatencyStats {
private:
    std::vector<double> samples;

public:
    void addSample(double latency_ms)
    {
        samples.push_back(latency_ms);
    }

    void clear()
    {
        samples.clear();
    }

    double getAverage() const
    {
        if (samples.empty())
            return 0.0;
        double sum = 0.0;
        for (double s : samples)
            sum += s;
        return sum / samples.size();
    }

    double getMin() const
    {
        if (samples.empty())
            return 0.0;
        return *std::min_element(samples.begin(), samples.end());
    }

    double getMax() const
    {
        if (samples.empty())
            return 0.0;
        return *std::max_element(samples.begin(), samples.end());
    }

    double getPercentile(double percentile)
    {
        if (samples.empty())
            return 0.0;
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        size_t index = static_cast<size_t>(percentile * sorted.size() / 100.0);
        if (index >= sorted.size())
            index = sorted.size() - 1;
        return sorted[index];
    }

    size_t getCount() const
    {
        return samples.size();
    }
};

inline std::string getSystemInfo()
{
    std::string info;

    // Get OS info
    FILE* pipe = popen("uname -a", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            info += "OS: " + std::string(buffer);
        }
        pclose(pipe);
    }

    // Get CPU info (works on macOS)
    pipe = popen("sysctl -n machdep.cpu.brand_string 2>/dev/null || lscpu | grep 'Model name' | cut -d':' -f2", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            info += "CPU: " + std::string(buffer);
        }
        pclose(pipe);
    }

    // Get memory info (works on macOS)
    pipe = popen("sysctl -n hw.memsize 2>/dev/null | awk '{print $1/1024/1024/1024 \" GB\"}' || free -h | grep Mem | awk '{print $2}'", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            info += "Memory: " + std::string(buffer);
        }
        pclose(pipe);
    }

    return info;
}

class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;

public:
    void start()
    {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double elapsedMilliseconds() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_time).count();
    }

    double elapsedMicroseconds() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_time).count();
    }

    double elapsedNanoseconds() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::nano>(end - start_time).count();
    }

    double elapsedSeconds() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start_time).count();
    }
};

#ifdef __linux__
#include <cerrno>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif

struct PerfCounterSample {
    bool valid { false };
    uint64_t cycles { 0 };
    uint64_t instructions { 0 };
    uint64_t cache_misses { 0 };
    uint64_t branches { 0 };
    uint64_t branch_misses { 0 };
};

class PerfCounterSet {
public:
    PerfCounterSet() = default;
    ~PerfCounterSet()
    {
        stop();
#ifdef __linux__
        closeAll();
#endif
    }

    bool start()
    {
#ifdef __linux__
        closeAll();
        opened_counters = 0;
        fd_cycles = openCounter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
        fd_instructions = openCounter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);
        fd_cache_misses = openCounter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES);
        fd_branches = openCounter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
        fd_branch_misses = openCounter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES);

        if (opened_counters == 0) {
            closeAll();
            active = false;
            return false;
        }

        resetAndEnable();
        active = true;
        return true;
#else
        return false;
#endif
    }

    PerfCounterSample stop()
    {
        PerfCounterSample sample;
#ifdef __linux__
        if (!active) {
            return sample;
        }
        active = false;
        disableAll();

        sample.valid = opened_counters > 0;
        sample.cycles = readCounter(fd_cycles);
        sample.instructions = readCounter(fd_instructions);
        sample.cache_misses = readCounter(fd_cache_misses);
        sample.branches = readCounter(fd_branches);
        sample.branch_misses = readCounter(fd_branch_misses);

        closeAll();
#endif
        return sample;
    }

private:
#ifdef __linux__
    int fd_cycles { -1 };
    int fd_instructions { -1 };
    int fd_cache_misses { -1 };
    int fd_branches { -1 };
    int fd_branch_misses { -1 };
    bool active { false };
    int opened_counters { 0 };

    static long sys_perf_event_open(struct perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags)
    {
        return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
    }

    int openCounter(uint32_t type, uint64_t config)
    {
        struct perf_event_attr attr {};
        attr.size = sizeof(perf_event_attr);
        attr.type = type;
        attr.config = config;
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.exclude_idle = 0;
        attr.read_format = 0;

        int fd = static_cast<int>(sys_perf_event_open(&attr, 0, -1, -1, 0));
        if (fd != -1) {
            ++opened_counters;
        }
        return fd;
    }

    void resetAndEnable()
    {
        if (fd_cycles != -1) {
            ioctl(fd_cycles, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_cycles, PERF_EVENT_IOC_ENABLE, 0);
        }
        if (fd_instructions != -1) {
            ioctl(fd_instructions, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_instructions, PERF_EVENT_IOC_ENABLE, 0);
        }
        if (fd_cache_misses != -1) {
            ioctl(fd_cache_misses, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_cache_misses, PERF_EVENT_IOC_ENABLE, 0);
        }
        if (fd_branches != -1) {
            ioctl(fd_branches, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_branches, PERF_EVENT_IOC_ENABLE, 0);
        }
        if (fd_branch_misses != -1) {
            ioctl(fd_branch_misses, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd_branch_misses, PERF_EVENT_IOC_ENABLE, 0);
        }
    }

    void disableAll()
    {
        if (fd_cycles != -1) {
            ioctl(fd_cycles, PERF_EVENT_IOC_DISABLE, 0);
        }
        if (fd_instructions != -1) {
            ioctl(fd_instructions, PERF_EVENT_IOC_DISABLE, 0);
        }
        if (fd_cache_misses != -1) {
            ioctl(fd_cache_misses, PERF_EVENT_IOC_DISABLE, 0);
        }
        if (fd_branches != -1) {
            ioctl(fd_branches, PERF_EVENT_IOC_DISABLE, 0);
        }
        if (fd_branch_misses != -1) {
            ioctl(fd_branch_misses, PERF_EVENT_IOC_DISABLE, 0);
        }
    }

    void closeAll()
    {
        if (fd_cycles != -1) {
            close(fd_cycles);
            fd_cycles = -1;
        }
        if (fd_instructions != -1) {
            close(fd_instructions);
            fd_instructions = -1;
        }
        if (fd_cache_misses != -1) {
            close(fd_cache_misses);
            fd_cache_misses = -1;
        }
        if (fd_branches != -1) {
            close(fd_branches);
            fd_branches = -1;
        }
        if (fd_branch_misses != -1) {
            close(fd_branch_misses);
            fd_branch_misses = -1;
        }
        opened_counters = 0;
    }

    static uint64_t readCounter(int fd)
    {
        if (fd == -1) {
            return 0;
        }
        uint64_t value = 0;
        if (read(fd, &value, sizeof(value)) != static_cast<ssize_t>(sizeof(value))) {
            value = 0;
        }
        return value;
    }
#endif
};

// CPU Affinity utilities for production-grade benchmarking

inline std::string getBuildCompilerInfo()
{
#ifdef PERF_BUILD_COMPILER
    std::string compiler = PERF_BUILD_COMPILER;
    if (compiler.empty()) {
        compiler = "unknown";
    }
#else
    std::string compiler = "unknown";
#endif
#ifdef PERF_BUILD_COMPILER_VERSION
    std::string version = PERF_BUILD_COMPILER_VERSION;
    if (!version.empty()) {
        compiler += " " + version;
    }
#endif
    return compiler;
}

inline std::string getBuildTypeInfo()
{
#ifdef PERF_BUILD_TYPE
    std::string build_type = PERF_BUILD_TYPE;
    if (build_type.empty()) {
        build_type = "unspecified";
    }
#else
    std::string build_type = "unspecified";
#endif
    return build_type;
}

inline std::string getCMakeVersionInfo()
{
#ifdef PERF_CMAKE_VERSION
    std::string cmake_version = PERF_CMAKE_VERSION;
    if (cmake_version.empty()) {
        cmake_version = "unknown";
    }
#else
    std::string cmake_version = "unknown";
#endif
    return cmake_version;
}

inline std::string getBuildMetadataSummary()
{
    std::ostringstream oss;
    oss << "Build Compiler: " << getBuildCompilerInfo() << "\n";
    oss << "Build Type: " << getBuildTypeInfo() << "\n";
    oss << "CMake Version: " << getCMakeVersionInfo();
    return oss.str();
}

inline std::map<std::string, std::string> getBuildMetadataMap()
{
    return {
        {"build.compiler", getBuildCompilerInfo()},
        {"build.type", getBuildTypeInfo()},
        {"build.cmake", getCMakeVersionInfo()}
    };
}

class CPUAffinity {
public:
    // Get the number of available CPU cores
    static int getNumCores()
    {
#ifdef __APPLE__
        int cores;
        size_t len = sizeof(cores);
        if (sysctlbyname("hw.ncpu", &cores, &len, NULL, 0) == 0) {
            return cores;
        }
        return 1;
#elif defined(__linux__)
        long cores = sysconf(_SC_NPROCESSORS_ONLN);
        return (cores > 0) ? static_cast<int>(cores) : 1;
#else
        return 1;
#endif
    }

    // Pin current thread to a specific CPU core (0-indexed)
    static bool pinThreadToCore(int core_id)
    {
        if (core_id < 0 || core_id >= getNumCores()) {
            return false;
        }

#ifdef __APPLE__
        // macOS uses thread affinity tags (approximate core binding)
        thread_affinity_policy_data_t policy = { core_id + 1 };
        kern_return_t result = thread_policy_set(
            mach_thread_self(),
            THREAD_AFFINITY_POLICY,
            (thread_policy_t)&policy,
            THREAD_AFFINITY_POLICY_COUNT);
        return result == KERN_SUCCESS;

#elif __linux__
        // Linux uses CPU sets for precise core binding
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
        return result == 0;

#else
        return false;
#endif
    }

    // Get current thread's CPU affinity
    static std::vector<int> getCurrentAffinity()
    {
        std::vector<int> cores;

#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        if (pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0) {
            for (int i = 0; i < getNumCores(); ++i) {
                if (CPU_ISSET(i, &cpuset)) {
                    cores.push_back(i);
                }
            }
        }
#else
        // For macOS, we can't easily query affinity, so return all cores
        for (int i = 0; i < getNumCores(); ++i) {
            cores.push_back(i);
        }
#endif
        return cores;
    }

    // Reset thread affinity to use all available cores
    static bool resetAffinity()
    {
#ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for (int i = 0; i < getNumCores(); ++i) {
            CPU_SET(i, &cpuset);
        }
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
        return true; // macOS doesn't need explicit reset
#endif
    }
};

#endif
