#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <algorithm>
#include <chrono>
#include <string>
#include <cmath>

class LatencyStats {
private:
    std::vector<double> samples;
    
public:
    void addSample(double latency_ms) {
        samples.push_back(latency_ms);
    }
    
    void clear() {
        samples.clear();
    }
    
    double getAverage() const {
        if (samples.empty()) return 0.0;
        double sum = 0.0;
        for (double s : samples) sum += s;
        return sum / samples.size();
    }
    
    double getMin() const {
        if (samples.empty()) return 0.0;
        return *std::min_element(samples.begin(), samples.end());
    }
    
    double getMax() const {
        if (samples.empty()) return 0.0;
        return *std::max_element(samples.begin(), samples.end());
    }
    
    double getPercentile(double percentile) {
        if (samples.empty()) return 0.0;
        std::vector<double> sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        size_t index = static_cast<size_t>(percentile * sorted.size() / 100.0);
        if (index >= sorted.size()) index = sorted.size() - 1;
        return sorted[index];
    }
    
    size_t getCount() const {
        return samples.size();
    }
};

inline std::string getSystemInfo() {
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
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsedMilliseconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_time).count();
    }
    
    double elapsedMicroseconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_time).count();
    }
    
    double elapsedNanoseconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::nano>(end - start_time).count();
    }
    
    double elapsedSeconds() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start_time).count();
    }
};

#endif