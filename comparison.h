#ifndef COMPARISON_H
#define COMPARISON_H

#include "benchmark.h"
#include "report.h"
#include <map>
#include <string>
#include <vector>

// Comparison result for a single metric
struct MetricComparison {
    std::string metric_name;
    double baseline_value;
    double current_value;
    double absolute_diff;
    double percent_change;
    std::string unit;

    enum Status {
        IMPROVED, // Performance improved
        UNCHANGED, // Within threshold
        DEGRADED, // Performance degraded but acceptable
        CRITICAL // Performance critically degraded
    };

    Status status;

    // Helper to determine status based on thresholds
    void calculateStatus(double warning_threshold = 10.0, double critical_threshold = 25.0);

    // Format as a human-readable string
    std::string toString() const;
};

// Comparison result for a complete benchmark
struct BenchmarkComparison {
    std::string benchmark_name;
    std::vector<MetricComparison> metrics;
    bool passed; // Overall pass/fail status

    // Add a metric comparison
    void addMetric(const MetricComparison& metric);

    // Calculate overall pass/fail based on all metrics
    void calculateOverallStatus();

    // Format as text or markdown
    std::string toText() const;
    std::string toMarkdown() const;
};

// Main comparison engine
class ComparisonEngine {
private:
    std::map<std::string, BenchmarkResult> baseline_results;
    std::map<std::string, BenchmarkResult> current_results;
    std::map<std::string, std::string> baseline_system_info;
    std::map<std::string, std::string> current_system_info;

    // Thresholds
    double warning_threshold = 10.0; // 10% degradation = warning
    double critical_threshold = 25.0; // 25% degradation = critical

    // Parse JSON report files
    bool parseJSONReport(const std::string& json_content,
        std::map<std::string, BenchmarkResult>& results,
        std::map<std::string, std::string>& system_info);

    // Compare individual metrics
    MetricComparison compareMetric(const std::string& name,
        double baseline,
        double current,
        const std::string& unit,
        bool higher_is_better = true);

    // Extract system info from JSON
    void parseSystemInfo(const std::string& json_content,
        std::map<std::string, std::string>& info);

public:
    // Set custom thresholds
    void setThresholds(double warning, double critical);

    // Load baseline and current reports from files
    bool loadBaselineReport(const std::string& filename);
    bool loadCurrentReport(const std::string& filename);

    // Perform comparison
    std::vector<BenchmarkComparison> compare();

    // Generate comparison report
    std::string generateReport(const std::string& format = "text");

    // Get overall health status
    enum HealthStatus {
        HEALTHY,
        WARNING,
        CRITICAL
    };
    HealthStatus getOverallHealth() const;

    // Get system info comparison
    std::string getSystemInfoComparison() const;
};

#endif // COMPARISON_H