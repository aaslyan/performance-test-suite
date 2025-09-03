#ifndef UTILS_H
#define UTILS_H

// Define _GNU_SOURCE before any includes for Linux CPU affinity functions
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
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

// CPU Affinity utilities for production-grade benchmarking
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