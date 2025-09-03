#include "comparison.h"
#include "visualization.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// Simple JSON parser helper functions
std::string extractJSONString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\": \"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        search = "\"" + key + "\": "; // Try without quotes (for numbers/objects)
        pos = json.find(search);
        if (pos == std::string::npos)
            return "";
        pos += search.length();
    } else {
        pos += search.length();
    }

    size_t end = json.find_first_of(",\n}", pos);
    if (end == std::string::npos)
        return "";

    std::string value = json.substr(pos, end - pos);
    // Remove quotes if present
    if (!value.empty() && value.front() == '"')
        value = value.substr(1);
    if (!value.empty() && value.back() == '"')
        value.pop_back();

    return value;
}

double extractJSONNumber(const std::string& json, const std::string& key)
{
    std::string value = extractJSONString(json, key);
    if (value.empty())
        return 0.0;

    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

// MetricComparison implementation
void MetricComparison::calculateStatus(double warning_threshold, double critical_threshold)
{
    // For throughput metrics (higher is better)
    bool higher_is_better = true;
    if (metric_name.find("latency") != std::string::npos || metric_name.find("time") != std::string::npos) {
        higher_is_better = false; // For latency, lower is better
    }

    if (higher_is_better) {
        if (percent_change > 5.0) {
            status = IMPROVED;
        } else if (percent_change >= -warning_threshold) {
            status = UNCHANGED;
        } else if (percent_change >= -critical_threshold) {
            status = DEGRADED;
        } else {
            status = CRITICAL;
        }
    } else {
        // For latency (lower is better), invert the logic
        if (percent_change < -5.0) {
            status = IMPROVED;
        } else if (percent_change <= warning_threshold) {
            status = UNCHANGED;
        } else if (percent_change <= critical_threshold) {
            status = DEGRADED;
        } else {
            status = CRITICAL;
        }
    }
}

std::string MetricComparison::toString() const
{
    std::stringstream ss;

    // Status indicator with colors
    std::string indicator;
    switch (status) {
    case IMPROVED:
        indicator = "\033[32mIMPROVED\033[0m";
        break; // Green
    case UNCHANGED:
        indicator = "\033[37mUNCHANGED\033[0m";
        break; // White
    case DEGRADED:
        indicator = "\033[33mDEGRADED\033[0m";
        break; // Yellow
    case CRITICAL:
        indicator = "\033[31mCRITICAL\033[0m";
        break; // Red
    }

    ss << std::fixed << std::setprecision(2);
    ss << "[" << indicator << "] " << metric_name << ": ";
    ss << baseline_value << " -> " << current_value << " " << unit;
    ss << " (" << std::showpos << percent_change << "%" << std::noshowpos << ")";

    return ss.str();
}

// BenchmarkComparison implementation
void BenchmarkComparison::addMetric(const MetricComparison& metric)
{
    metrics.push_back(metric);
}

void BenchmarkComparison::calculateOverallStatus()
{
    passed = true;
    for (const auto& metric : metrics) {
        if (metric.status == MetricComparison::CRITICAL) {
            passed = false;
            break;
        }
    }
}

std::string BenchmarkComparison::toText() const
{
    std::stringstream ss;

    ss << "\n"
       << benchmark_name << " Benchmark";
    ss << "\n"
       << std::string(benchmark_name.length() + 10, '-') << "\n";

    for (const auto& metric : metrics) {
        ss << "  " << metric.toString() << "\n";
    }

    ss << "  Overall: " << (passed ? "\033[32mPASSED\033[0m" : "\033[31mFAILED\033[0m") << "\n";

    return ss.str();
}

std::string BenchmarkComparison::toMarkdown() const
{
    std::stringstream ss;

    ss << "\n### " << benchmark_name << " Benchmark\n\n";
    ss << "| Metric | Baseline | Current | Change | Status |\n";
    ss << "|--------|----------|---------|--------|--------|\n";

    for (const auto& metric : metrics) {
        std::string status_icon;
        switch (metric.status) {
        case MetricComparison::IMPROVED:
            status_icon = "Improved";
            break;
        case MetricComparison::UNCHANGED:
            status_icon = "Unchanged";
            break;
        case MetricComparison::DEGRADED:
            status_icon = "Degraded";
            break;
        case MetricComparison::CRITICAL:
            status_icon = "Critical";
            break;
        }

        ss << "| " << metric.metric_name
           << " | " << std::fixed << std::setprecision(2) << metric.baseline_value << " " << metric.unit
           << " | " << metric.current_value << " " << metric.unit
           << " | " << std::showpos << metric.percent_change << "%" << std::noshowpos
           << " | " << status_icon << " |\n";
    }

    ss << "\n**Overall Status**: " << (passed ? "PASSED" : "FAILED") << "\n";

    return ss.str();
}

// ComparisonEngine implementation
bool ComparisonEngine::parseJSONReport(const std::string& json_content,
    std::map<std::string, BenchmarkResult>& results,
    std::map<std::string, std::string>& system_info)
{
    // Parse benchmarks array
    size_t benchmarks_start = json_content.find("\"benchmarks\": [");
    if (benchmarks_start == std::string::npos)
        return false;

    size_t bench_pos = benchmarks_start;
    while (true) {
        size_t bench_start = json_content.find("{", bench_pos);
        if (bench_start == std::string::npos)
            break;

        size_t bench_end = json_content.find("}", bench_start);
        if (bench_end == std::string::npos)
            break;

        std::string bench_json = json_content.substr(bench_start, bench_end - bench_start + 1);

        BenchmarkResult result;
        result.name = extractJSONString(bench_json, "name");
        if (result.name.empty())
            break;

        result.status = extractJSONString(bench_json, "status");
        result.throughput = extractJSONNumber(bench_json, "throughput");
        result.throughput_unit = extractJSONString(bench_json, "throughput_unit");

        // Parse latency object
        size_t lat_start = bench_json.find("\"latency\": {");
        if (lat_start != std::string::npos) {
            size_t lat_end = bench_json.find("}", lat_start);
            std::string lat_json = bench_json.substr(lat_start, lat_end - lat_start + 1);

            result.avg_latency = extractJSONNumber(lat_json, "average");
            result.min_latency = extractJSONNumber(lat_json, "minimum");
            result.max_latency = extractJSONNumber(lat_json, "maximum");
            result.p50_latency = extractJSONNumber(lat_json, "p50");
            result.p90_latency = extractJSONNumber(lat_json, "p90");
            result.p99_latency = extractJSONNumber(lat_json, "p99");
            result.latency_unit = extractJSONString(lat_json, "unit");
        }

        results[result.name] = result;

        bench_pos = bench_end + 1;

        // Check if we've reached the end of the benchmarks array
        if (json_content.find("]", bench_pos) < json_content.find("{", bench_pos)) {
            break;
        }
    }

    // Parse system info
    parseSystemInfo(json_content, system_info);

    return !results.empty();
}

void ComparisonEngine::parseSystemInfo(const std::string& json_content,
    std::map<std::string, std::string>& info)
{
    std::string system_info_str = extractJSONString(json_content, "system_info");

    // Extract OS, CPU, Memory from the system_info string
    size_t os_pos = system_info_str.find("OS: ");
    if (os_pos != std::string::npos) {
        size_t end = system_info_str.find('\n', os_pos);
        info["OS"] = system_info_str.substr(os_pos + 4, end - os_pos - 4);
    }

    size_t cpu_pos = system_info_str.find("CPU: ");
    if (cpu_pos != std::string::npos) {
        size_t end = system_info_str.find('\n', cpu_pos);
        info["CPU"] = system_info_str.substr(cpu_pos + 5, end - cpu_pos - 5);
    }

    size_t mem_pos = system_info_str.find("Memory: ");
    if (mem_pos != std::string::npos) {
        size_t end = system_info_str.find('\n', mem_pos);
        if (end == std::string::npos)
            end = system_info_str.length();
        info["Memory"] = system_info_str.substr(mem_pos + 8, end - mem_pos - 8);
    }
}

MetricComparison ComparisonEngine::compareMetric(const std::string& name,
    double baseline,
    double current,
    const std::string& unit,
    bool higher_is_better)
{
    MetricComparison comp;
    comp.metric_name = name;
    comp.baseline_value = baseline;
    comp.current_value = current;
    comp.unit = unit;

    comp.absolute_diff = current - baseline;

    // Calculate percentage change
    if (baseline != 0) {
        comp.percent_change = ((current - baseline) / baseline) * 100.0;
    } else {
        comp.percent_change = (current != 0) ? 100.0 : 0.0;
    }

    comp.calculateStatus(warning_threshold, critical_threshold);

    return comp;
}

void ComparisonEngine::setThresholds(double warning, double critical)
{
    warning_threshold = warning;
    critical_threshold = critical;
}

bool ComparisonEngine::loadBaselineReport(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open baseline file: " << filename << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parseJSONReport(buffer.str(), baseline_results, baseline_system_info);
}

bool ComparisonEngine::loadCurrentReport(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open current file: " << filename << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parseJSONReport(buffer.str(), current_results, current_system_info);
}

std::vector<BenchmarkComparison> ComparisonEngine::compare()
{
    std::vector<BenchmarkComparison> comparisons;

    // Compare each benchmark that exists in both baseline and current
    for (const auto& [bench_name, baseline_bench] : baseline_results) {
        auto current_it = current_results.find(bench_name);
        if (current_it == current_results.end()) {
            continue; // Skip if benchmark not in current results
        }

        const auto& current_bench = current_it->second;
        BenchmarkComparison bench_comp;
        bench_comp.benchmark_name = bench_name;

        // Compare throughput
        if (baseline_bench.throughput > 0 || current_bench.throughput > 0) {
            auto metric = compareMetric(
                "Throughput",
                baseline_bench.throughput,
                current_bench.throughput,
                current_bench.throughput_unit,
                true // Higher is better
            );
            bench_comp.addMetric(metric);
        }

        // Compare latencies
        if (baseline_bench.avg_latency > 0 || current_bench.avg_latency > 0) {
            auto metric = compareMetric(
                "Avg Latency",
                baseline_bench.avg_latency,
                current_bench.avg_latency,
                current_bench.latency_unit,
                false // Lower is better
            );
            bench_comp.addMetric(metric);
        }

        if (baseline_bench.p50_latency > 0 || current_bench.p50_latency > 0) {
            auto metric = compareMetric(
                "P50 Latency",
                baseline_bench.p50_latency,
                current_bench.p50_latency,
                current_bench.latency_unit,
                false);
            bench_comp.addMetric(metric);
        }

        if (baseline_bench.p99_latency > 0 || current_bench.p99_latency > 0) {
            auto metric = compareMetric(
                "P99 Latency",
                baseline_bench.p99_latency,
                current_bench.p99_latency,
                current_bench.latency_unit,
                false);
            bench_comp.addMetric(metric);
        }

        bench_comp.calculateOverallStatus();
        comparisons.push_back(bench_comp);
    }

    return comparisons;
}

std::string ComparisonEngine::generateReport(const std::string& format)
{
    auto comparisons = compare();
    std::stringstream ss;

    // Header
    ss << "=====================================\n";
    ss << "    PERFORMANCE COMPARISON REPORT\n";
    ss << "=====================================\n\n";

    // System info comparison
    ss << getSystemInfoComparison();

    // Benchmark comparisons
    if (format == "markdown") {
        ss << "\n## Benchmark Comparisons\n";
        for (const auto& comp : comparisons) {
            ss << comp.toMarkdown();
        }
    } else {
        ss << "\nBenchmark Comparisons\n";
        ss << "====================\n";
        for (const auto& comp : comparisons) {
            ss << comp.toText();
        }
    }

    // Overall health status
    auto health = getOverallHealth();
    ss << "\n=====================================\n";
    ss << "OVERALL STATUS: ";
    switch (health) {
    case HEALTHY:
        ss << "\033[32mHEALTHY\033[0m - All benchmarks within acceptable thresholds\n";
        break;
    case WARNING:
        ss << "\033[33mWARNING\033[0m - Some benchmarks show degradation\n";
        break;
    case CRITICAL:
        ss << "\033[31mCRITICAL\033[0m - Significant performance degradation detected\n";
        break;
    }
    ss << "=====================================\n";

    return ss.str();
}

std::string ComparisonEngine::generateReportWithCharts(const std::string& format)
{
    std::stringstream ss;

    // Start with regular report
    ss << generateReport(format);

    // Add ASCII charts
    auto comparisons = compare();

    ss << "\n"
       << std::string(60, '=') << "\n";
    ss << "                    VISUAL ANALYSIS\n";
    ss << std::string(60, '=') << "\n";

    // Generate charts using the visualization module
    ASCIIChart::ChartConfig chart_config;
    chart_config.width = 70;
    chart_config.use_colors = true;
    chart_config.show_values = true;

    ss << ComparisonVisualizer::generateComparisonCharts(comparisons, chart_config);

    return ss.str();
}

ComparisonEngine::HealthStatus ComparisonEngine::getOverallHealth() const
{
    auto comparisons = const_cast<ComparisonEngine*>(this)->compare();

    bool has_critical = false;
    bool has_warning = false;

    for (const auto& bench_comp : comparisons) {
        for (const auto& metric : bench_comp.metrics) {
            if (metric.status == MetricComparison::CRITICAL) {
                has_critical = true;
            } else if (metric.status == MetricComparison::DEGRADED) {
                has_warning = true;
            }
        }
    }

    if (has_critical)
        return CRITICAL;
    if (has_warning)
        return WARNING;
    return HEALTHY;
}

std::string ComparisonEngine::getSystemInfoComparison() const
{
    std::stringstream ss;

    ss << "System Information\n";
    ss << "------------------\n";

    ss << "Baseline System:\n";
    for (const auto& [key, value] : baseline_system_info) {
        ss << "  " << key << ": " << value << "\n";
    }

    ss << "\nCurrent System:\n";
    for (const auto& [key, value] : current_system_info) {
        ss << "  " << key << ": " << value << "\n";
    }

    // Highlight differences
    bool same_system = true;
    for (const auto& [key, baseline_value] : baseline_system_info) {
        auto it = current_system_info.find(key);
        if (it != current_system_info.end() && it->second != baseline_value) {
            same_system = false;
            break;
        }
    }

    if (!same_system) {
        ss << "\n\033[33mWARNING\033[0m: Systems have different configurations\n";
    }

    return ss.str();
}