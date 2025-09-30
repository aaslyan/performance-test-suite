#include "performance_context.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

// Helper function to convert BenchmarkResult to JSON
static std::string benchmarkResultToJson(const BenchmarkResult& result)
{
    std::ostringstream json;
    json << "{\n";
    json << "    \"name\": \"" << result.name << "\",\n";
    json << "    \"throughput\": " << result.throughput << ",\n";
    json << "    \"throughput_unit\": \"" << result.throughput_unit << "\",\n";
    json << "    \"avg_latency\": " << result.avg_latency << ",\n";
    json << "    \"min_latency\": " << result.min_latency << ",\n";
    json << "    \"max_latency\": " << result.max_latency << ",\n";
    json << "    \"p50_latency\": " << result.p50_latency << ",\n";
    json << "    \"p90_latency\": " << result.p90_latency << ",\n";
    json << "    \"p99_latency\": " << result.p99_latency << ",\n";
    json << "    \"latency_unit\": \"" << result.latency_unit << "\",\n";
    json << "    \"status\": \"" << result.status << "\",\n";
    json << "    \"error_message\": \"" << result.error_message << "\",\n";
    json << "    \"extra_metrics\": {\n";
    
    size_t count = 0;
    for (const auto& metric : result.extra_metrics) {
        if (count > 0) json << ",\n";
        json << "      \"" << metric.first << "\": " << metric.second;
        count++;
    }
    json << "\n    }\n";
    json << "  }";
    return json.str();
}

// ContextualBenchmarkResult implementation
ContextualBenchmarkResult::ContextualBenchmarkResult()
    : reliability_score(0.0)
{
}

std::string ContextualBenchmarkResult::toJsonWithContext() const
{
    std::ostringstream json;
    json << "{\n";
    json << "  \"benchmark_result\": " << benchmarkResultToJson(benchmark_result) << ",\n";
    json << "  \"platform_info\": " << platform_info.toJson() << ",\n";
    json << "  \"resource_metrics_avg\": " << resource_metrics_avg.toJson() << ",\n";
    json << "  \"resource_metrics_peak\": " << resource_metrics_peak.toJson() << ",\n";
    json << "  \"reliability_score\": " << reliability_score << ",\n";
    json << "  \"interference_detected\": " << (interference_report.hasInterference() ? "true" : "false") << ",\n";
    json << "  \"interference_summary\": \"" << interference_report.getSummary() << "\",\n";
    json << "  \"context_warnings\": [";
    for (size_t i = 0; i < context_warnings.size(); ++i) {
        if (i > 0) json << ", ";
        json << "\"" << context_warnings[i] << "\"";
    }
    json << "],\n";
    json << "  \"optimization_suggestions\": [";
    for (size_t i = 0; i < optimization_suggestions.size(); ++i) {
        if (i > 0) json << ", ";
        json << "\"" << optimization_suggestions[i] << "\"";
    }
    json << "]\n";
    json << "}";
    return json.str();
}

// PerformanceEnvironment implementation
PerformanceEnvironment::PerformanceEnvironment()
    : is_optimal_for_benchmarking(false)
    , environment_score(0.0)
{
}

std::string PerformanceEnvironment::getSummary() const
{
    std::ostringstream summary;
    summary << "Platform: " << platform.getSummary() << "\n";
    summary << "Environment Score: " << static_cast<int>(environment_score) << "/100\n";
    
    if (is_optimal_for_benchmarking) {
        summary << "Status: Optimal for benchmarking\n";
    } else {
        summary << "Status: Suboptimal for benchmarking\n";
        if (!environment_issues.empty()) {
            summary << "Issues: ";
            for (size_t i = 0; i < environment_issues.size(); ++i) {
                if (i > 0) summary << ", ";
                summary << environment_issues[i];
            }
            summary << "\n";
        }
    }
    
    if (environment_interference.hasInterference()) {
        summary << "System Interference: " << environment_interference.getSummary() << "\n";
    }
    
    return summary.str();
}

// ContextualComparison implementation
ContextualComparison::ContextualComparison()
    : platforms_comparable(false)
{
}

std::string ContextualComparison::generateComparisonReport() const
{
    std::ostringstream report;
    report << "# Platform Performance Comparison Report\n\n";
    
    // Platform summary
    report << "## Platforms Analyzed:\n";
    for (size_t i = 0; i < platforms.size(); ++i) {
        report << i + 1 << ". " << platforms[i].getSummary() << "\n";
    }
    report << "\n";
    
    // Comparison validity
    report << "## Comparison Validity:\n";
    report << comparison_validity << "\n";
    if (!comparison_caveats.empty()) {
        report << "Caveats:\n";
        for (const auto& caveat : comparison_caveats) {
            report << "- " << caveat << "\n";
        }
    }
    report << "\n";
    
    // Results summary
    report << "## Performance Results:\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        report << "Platform " << i + 1 << ":\n";
        report << "  Throughput: " << result.benchmark_result.throughput 
               << " " << result.benchmark_result.throughput_unit << "\n";
        report << "  Avg Latency: " << result.benchmark_result.avg_latency 
               << " " << result.benchmark_result.latency_unit << "\n";
        report << "  Reliability: " << static_cast<int>(result.reliability_score) << "%\n";
        
        if (!result.context_warnings.empty()) {
            report << "  Warnings: ";
            for (size_t j = 0; j < result.context_warnings.size(); ++j) {
                if (j > 0) report << ", ";
                report << result.context_warnings[j];
            }
            report << "\n";
        }
        report << "\n";
    }
    
    return report.str();
}

// PerformanceContextAnalyzer implementation
PerformanceContextAnalyzer::PerformanceContextAnalyzer()
{
}

PerformanceContextAnalyzer::~PerformanceContextAnalyzer()
{
    if (system_monitor.isMonitoring()) {
        system_monitor.stopMonitoring();
    }
}

PerformanceEnvironment PerformanceContextAnalyzer::analyzeCurrentEnvironment()
{
    PerformanceEnvironment env;
    
    // Get platform information
    env.platform = platform_detector.detectPlatform();
    
    // Get baseline system resources
    env.baseline_resources = getSystemBaseline(5);
    
    // Analyze interference
    system_monitor.startMonitoring();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    system_monitor.stopMonitoring();
    env.environment_interference = system_monitor.analyzeInterference();
    
    // Get optimization recommendations
    env.optimizations = platform_detector.getOptimizationRecommendations();
    
    // Calculate environment score
    env.environment_score = calculateEnvironmentScore(env);
    env.is_optimal_for_benchmarking = (env.environment_score >= 75.0);
    
    // Generate environment issues
    if (env.environment_interference.hasInterference()) {
        env.environment_issues.push_back("System interference detected");
    }
    
    if (env.platform.getPerformanceScore() < 50.0) {
        env.environment_issues.push_back("Low-performance hardware");
    }
    
    if (!env.platform.performance_issues.empty()) {
        env.environment_issues.insert(env.environment_issues.end(),
            env.platform.performance_issues.begin(),
            env.platform.performance_issues.end());
    }
    
    // Pre-benchmark recommendations
    if (!env.optimizations.hasRecommendations()) {
        env.pre_benchmark_recommendations.push_back("System appears optimally configured");
    } else {
        env.pre_benchmark_recommendations = env.optimizations.getAllRecommendations();
    }
    
    return env;
}

bool PerformanceContextAnalyzer::isEnvironmentOptimalForBenchmarking()
{
    PerformanceEnvironment env = analyzeCurrentEnvironment();
    return env.is_optimal_for_benchmarking;
}

std::vector<std::string> PerformanceContextAnalyzer::getPreBenchmarkRecommendations()
{
    PerformanceEnvironment env = analyzeCurrentEnvironment();
    return env.pre_benchmark_recommendations;
}

ContextualBenchmarkResult PerformanceContextAnalyzer::runBenchmarkWithContext(
    Benchmark* benchmark, int duration_seconds, int iterations, bool verbose, bool collect_perf_counters)
{
    if (!benchmark) {
        ContextualBenchmarkResult result;
        result.benchmark_result.status = "error";
        result.benchmark_result.error_message = "Null benchmark provided";
        return result;
    }
    
    // Pre-benchmark system check
    if (verbose) {
        std::cout << "Analyzing system environment...\n";
    }
    
    // Warmup system
    warmupSystem(3);
    
    // Start system monitoring
    system_monitor.startMonitoring();

    PerfCounterSet perf_counters;
    PerfCounterSample perf_sample;
    bool perf_enabled = collect_perf_counters && perf_counters.start();
    
    // Run the benchmark
    if (verbose) {
        std::cout << "Running benchmark: " << benchmark->getName() << "\n";
    }
    
    BenchmarkResult bench_result = benchmark->run(duration_seconds, iterations, verbose);
    
    if (collect_perf_counters) {
        perf_sample = perf_counters.stop();
        if (perf_sample.valid) {
            bench_result.extra_metrics["perf_cpu_cycles"] = static_cast<double>(perf_sample.cycles);
            bench_result.extra_metrics["perf_cpu_instructions"] = static_cast<double>(perf_sample.instructions);
            bench_result.extra_metrics["perf_l3_cache_misses"] = static_cast<double>(perf_sample.cache_misses);
            bench_result.extra_metrics["perf_branches"] = static_cast<double>(perf_sample.branches);
            bench_result.extra_metrics["perf_branch_misses"] = static_cast<double>(perf_sample.branch_misses);
            if (perf_sample.instructions > 0) {
                bench_result.extra_metrics["perf_cpi"] = static_cast<double>(perf_sample.cycles) /
                    static_cast<double>(perf_sample.instructions);
            }
            bench_result.extra_info["perf.counters"] = "perf_event_open";
        } else {
            bench_result.extra_info["perf.counters"] = perf_enabled ? "unavailable" : "insufficient_permissions";
        }
    } else {
        bench_result.extra_info["perf.counters"] = "disabled";
        perf_counters.stop();
    }
    
    // Stop monitoring and collect results
    system_monitor.stopMonitoring();
    
    ResourceMetrics avg_metrics = system_monitor.getAverageMetrics();
    ResourceMetrics peak_metrics = system_monitor.getPeakMetrics();
    InterferenceReport interference = system_monitor.analyzeInterference();

    for (const auto& entry : getBuildMetadataMap()) {
        bench_result.extra_info[entry.first] = entry.second;
    }
    
    // Cool down system
    cooldownSystem(2);
    
    // Analyze the results
    return analyzeBenchmarkResult(bench_result, avg_metrics, peak_metrics, interference);
}

ContextualBenchmarkResult PerformanceContextAnalyzer::analyzeBenchmarkResult(
    const BenchmarkResult& result,
    const ResourceMetrics& resource_avg,
    const ResourceMetrics& resource_peak,
    const InterferenceReport& interference)
{
    ContextualBenchmarkResult contextual_result;
    contextual_result.benchmark_result = result;
    contextual_result.resource_metrics_avg = resource_avg;
    contextual_result.resource_metrics_peak = resource_peak;
    contextual_result.interference_report = interference;
    contextual_result.platform_info = platform_detector.getCachedPlatformInfo();
    
    // Calculate reliability score
    contextual_result.reliability_score = calculateReliabilityScore(
        result, resource_avg, interference);
    
    // Generate context warnings
    contextual_result.context_warnings = generateContextWarnings(
        result, resource_avg, interference, contextual_result.platform_info);
    
    // Generate optimization suggestions
    contextual_result.optimization_suggestions = generateOptimizationSuggestions(
        result, resource_avg, contextual_result.platform_info);
    
    return contextual_result;
}

double PerformanceContextAnalyzer::calculateReliabilityScore(
    const BenchmarkResult& result,
    const ResourceMetrics& metrics,
    const InterferenceReport& interference)
{
    double score = 100.0; // Start with perfect score
    
    // Check benchmark status
    if (result.status != "success") {
        return 0.0; // Failed benchmarks are not reliable
    }
    
    // Penalize for system interference
    if (interference.high_background_cpu_usage) {
        score -= 20.0;
    }
    if (interference.memory_pressure) {
        score -= 15.0;
    }
    if (interference.high_io_wait) {
        score -= 25.0;
    }
    if (interference.thermal_throttling) {
        score -= 30.0;
    }
    if (interference.network_congestion) {
        score -= 10.0;
    }
    
    // Penalize for insufficient monitoring samples
    if (metrics.sample_count < 10) {
        score -= 10.0;
    }
    
    // Penalize for short test duration (unreliable timing)
    if (metrics.monitoring_duration_seconds < 3.0) {
        score -= 15.0;
    }
    
    // Penalize for very high system load
    if (metrics.load_average_1min > CPUAffinity::getNumCores() * 0.8) {
        score -= 10.0;
    }
    
    return std::max(0.0, std::min(100.0, score));
}

std::vector<std::string> PerformanceContextAnalyzer::generateContextWarnings(
    const BenchmarkResult& result,
    const ResourceMetrics& metrics,
    const InterferenceReport& interference,
    const PlatformInfo& platform)
{
    std::vector<std::string> warnings;
    
    if (result.status != "success") {
        warnings.push_back("Benchmark failed to complete successfully");
    }
    
    if (interference.hasInterference()) {
        warnings.push_back("System interference detected during benchmark");
    }
    
    if (platform.is_virtualized) {
        warnings.push_back("Running in virtualized environment - results may not reflect bare metal performance");
    }
    
    if (platform.cpu_governor == "powersave") {
        warnings.push_back("CPU governor set to power saving mode - performance may be reduced");
    }
    
    if (!platform.turbo_boost_enabled) {
        warnings.push_back("CPU turbo boost disabled - peak performance unavailable");
    }
    
    if (metrics.monitoring_duration_seconds < 5.0) {
        warnings.push_back("Short benchmark duration - results may be less reliable");
    }
    
    if (metrics.thermal_throttling_detected) {
        warnings.push_back("CPU thermal throttling detected - performance limited by temperature");
    }
    
    if (metrics.memory_usage_percent > 90.0) {
        warnings.push_back("High memory usage - potential memory pressure affecting performance");
    }
    
    if (metrics.avg_io_wait_percent > 20.0) {
        warnings.push_back("High I/O wait time - storage bottleneck detected");
    }
    
    return warnings;
}

std::vector<std::string> PerformanceContextAnalyzer::generateOptimizationSuggestions(
    const BenchmarkResult& result,
    const ResourceMetrics& metrics,
    const PlatformInfo& platform)
{
    std::vector<std::string> suggestions;
    
    if (platform.cpu_governor == "powersave") {
        suggestions.push_back("Set CPU governor to 'performance' for maximum benchmark accuracy");
    }
    
    if (!platform.turbo_boost_enabled) {
        suggestions.push_back("Enable CPU turbo boost for peak performance testing");
    }
    
    if (metrics.memory_usage_percent > 80.0) {
        suggestions.push_back("Close unnecessary applications to free memory");
    }
    
    if (metrics.load_average_1min > CPUAffinity::getNumCores() * 0.5) {
        suggestions.push_back("Reduce background system load for more accurate benchmarks");
    }
    
    if (platform.primary_storage_type == "HDD") {
        suggestions.push_back("Consider running I/O benchmarks on SSD for better performance baseline");
    }
    
    if (platform.is_virtualized) {
        suggestions.push_back("Run benchmarks on bare metal for most accurate hardware performance measurement");
    }
    
    // Benchmark-specific suggestions
    if (result.name.find("CPU") != std::string::npos) {
        suggestions.push_back("Pin benchmark threads to specific CPU cores for consistent results");
    }
    
    if (result.name.find("Memory") != std::string::npos && platform.numa_enabled) {
        suggestions.push_back("Consider NUMA-aware memory allocation for multi-socket systems");
    }
    
    if (result.name.find("Disk") != std::string::npos && metrics.avg_io_wait_percent > 10.0) {
        suggestions.push_back("Ensure disk benchmarks run on dedicated storage to avoid interference");
    }
    
    return suggestions;
}

double PerformanceContextAnalyzer::calculateEnvironmentScore(const PerformanceEnvironment& env)
{
    double score = 50.0; // Base score
    
    // Platform performance contribution (40%)
    score += env.platform.getPerformanceScore() * 0.4;
    
    // System interference penalty (30%)
    if (!env.environment_interference.hasInterference()) {
        score += 30.0;
    } else {
        if (env.environment_interference.high_background_cpu_usage) score -= 10.0;
        if (env.environment_interference.memory_pressure) score -= 8.0;
        if (env.environment_interference.high_io_wait) score -= 12.0;
        if (env.environment_interference.thermal_throttling) score -= 15.0;
    }
    
    // Configuration optimality (30%)
    if (env.platform.turbo_boost_enabled) score += 10.0;
    if (env.platform.cpu_governor != "powersave") score += 10.0;
    if (!env.platform.is_virtualized) score += 10.0;
    
    return std::max(0.0, std::min(100.0, score));
}

ContextualComparison PerformanceContextAnalyzer::compareResults(
    const std::vector<ContextualBenchmarkResult>& results)
{
    ContextualComparison comparison;
    comparison.results = results;
    
    // Extract platforms
    for (const auto& result : results) {
        comparison.platforms.push_back(result.platform_info);
    }
    
    // Analyze comparability
    if (results.size() >= 2) {
        comparison.platforms_comparable = true;
        
        // Check for significant platform differences
        for (size_t i = 1; i < results.size(); ++i) {
            if (!PlatformDetector::areComparablePlatforms(
                results[0].platform_info, results[i].platform_info)) {
                comparison.platforms_comparable = false;
                break;
            }
        }
        
        if (comparison.platforms_comparable) {
            comparison.comparison_validity = "Platforms are comparable for direct performance comparison";
        } else {
            comparison.comparison_validity = "Significant platform differences detected - comparison requires adjustment";
        }
        
        // Generate caveats
        bool has_virtualized = false;
        bool has_interference = false;
        bool has_low_reliability = false;
        
        for (const auto& result : results) {
            if (result.platform_info.is_virtualized) has_virtualized = true;
            if (result.interference_report.hasInterference()) has_interference = true;
            if (result.reliability_score < 70.0) has_low_reliability = true;
        }
        
        if (has_virtualized) {
            comparison.comparison_caveats.push_back("Some platforms are virtualized - performance may not reflect bare metal capability");
        }
        if (has_interference) {
            comparison.comparison_caveats.push_back("System interference detected on some platforms during testing");
        }
        if (has_low_reliability) {
            comparison.comparison_caveats.push_back("Some benchmark results have low reliability scores");
        }
    }
    
    return comparison;
}

bool PerformanceContextAnalyzer::areResultsComparable(
    const ContextualBenchmarkResult& result1,
    const ContextualBenchmarkResult& result2)
{
    // Check platform compatibility
    if (!PlatformDetector::areComparablePlatforms(result1.platform_info, result2.platform_info)) {
        return false;
    }
    
    // Check reliability scores
    if (result1.reliability_score < 50.0 || result2.reliability_score < 50.0) {
        return false;
    }
    
    // Check for major interference differences
    bool result1_interference = result1.interference_report.hasInterference();
    bool result2_interference = result2.interference_report.hasInterference();
    
    if (result1_interference != result2_interference) {
        return false; // One has interference, other doesn't
    }
    
    return true;
}

void PerformanceContextAnalyzer::warmupSystem(int seconds)
{
    // Light CPU warmup to stabilize frequency scaling
    auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    volatile int dummy = 0;
    
    while (std::chrono::steady_clock::now() < end_time) {
        for (int i = 0; i < 1000; ++i) {
            dummy += i;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void PerformanceContextAnalyzer::cooldownSystem(int seconds)
{
    // Simple cooldown period
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

ResourceMetrics PerformanceContextAnalyzer::getSystemBaseline(int sample_duration)
{
    system_monitor.startMonitoring();
    std::this_thread::sleep_for(std::chrono::seconds(sample_duration));
    system_monitor.stopMonitoring();
    
    return system_monitor.getAverageMetrics();
}

PlatformInfo PerformanceContextAnalyzer::getCurrentPlatform()
{
    return platform_detector.detectPlatform();
}

std::string PerformanceContextAnalyzer::getPlatformSummary()
{
    PlatformInfo info = platform_detector.getCachedPlatformInfo();
    return info.getSummary();
}

std::vector<std::string> PerformanceContextAnalyzer::getPerformanceIssues()
{
    return platform_detector.getPerformanceIssues();
}

// PerformanceContext namespace implementation
namespace PerformanceContext {

bool isSystemBenchmarkReady()
{
    PerformanceContextAnalyzer analyzer;
    return analyzer.isEnvironmentOptimalForBenchmarking();
}

std::string getSystemReadinessReport()
{
    PerformanceContextAnalyzer analyzer;
    PerformanceEnvironment env = analyzer.analyzeCurrentEnvironment();
    return env.getSummary();
}

std::vector<std::string> getQuickOptimizationTips()
{
    PerformanceContextAnalyzer analyzer;
    return analyzer.getPreBenchmarkRecommendations();
}

std::string interpretThroughputResult(double throughput, const std::string& unit)
{
    std::ostringstream interpretation;
    interpretation << throughput << " " << unit;
    
    if (unit.find("MB/s") != std::string::npos) {
        if (throughput > 10000) {
            interpretation << " (Excellent - High-end NVMe SSD performance)";
        } else if (throughput > 1000) {
            interpretation << " (Good - SSD performance)";
        } else if (throughput > 100) {
            interpretation << " (Fair - Standard disk performance)";
        } else {
            interpretation << " (Poor - Consider storage upgrade)";
        }
    } else if (unit.find("GOPS") != std::string::npos) {
        if (throughput > 10.0) {
            interpretation << " (Excellent - High-performance CPU)";
        } else if (throughput > 5.0) {
            interpretation << " (Good - Modern CPU performance)";
        } else if (throughput > 1.0) {
            interpretation << " (Fair - Standard performance)";
        } else {
            interpretation << " (Poor - Low-end or throttled CPU)";
        }
    }
    
    return interpretation.str();
}

std::string interpretLatencyResult(double latency, const std::string& unit)
{
    std::ostringstream interpretation;
    interpretation << latency << " " << unit;
    
    if (unit.find("us") != std::string::npos) {
        if (latency < 1.0) {
            interpretation << " (Excellent - Sub-microsecond latency)";
        } else if (latency < 10.0) {
            interpretation << " (Good - Low latency)";
        } else if (latency < 100.0) {
            interpretation << " (Fair - Acceptable latency)";
        } else {
            interpretation << " (Poor - High latency detected)";
        }
    } else if (unit.find("ms") != std::string::npos) {
        if (latency < 1.0) {
            interpretation << " (Excellent - Sub-millisecond latency)";
        } else if (latency < 10.0) {
            interpretation << " (Good - Low latency)";
        } else if (latency < 100.0) {
            interpretation << " (Fair - Acceptable latency)";
        } else {
            interpretation << " (Poor - High latency detected)";
        }
    }
    
    return interpretation.str();
}

std::string interpretReliabilityScore(double score)
{
    if (score >= 90.0) return "Excellent - Results highly reliable";
    if (score >= 75.0) return "Good - Results reliable with minor caveats";
    if (score >= 60.0) return "Fair - Results usable but consider optimization";
    if (score >= 40.0) return "Poor - Results unreliable, significant interference";
    return "Very Poor - Results invalid due to system issues";
}

double calculatePlatformAdjustmentFactor(
    const PlatformInfo& reference_platform,
    const PlatformInfo& target_platform)
{
    double reference_score = reference_platform.getPerformanceScore();
    double target_score = target_platform.getPerformanceScore();
    
    if (reference_score == 0.0) return 1.0;
    
    return target_score / reference_score;
}

std::string explainPerformanceDifference(
    const ContextualBenchmarkResult& result1,
    const ContextualBenchmarkResult& result2)
{
    std::ostringstream explanation;
    
    double throughput1 = result1.benchmark_result.throughput;
    double throughput2 = result2.benchmark_result.throughput;
    
    if (throughput1 == 0.0 || throughput2 == 0.0) {
        return "Cannot compare - invalid throughput values";
    }
    
    double ratio = throughput1 / throughput2;
    
    explanation << "Performance ratio: " << ratio << "x\n";
    
    // Platform differences
    double platform1_score = result1.platform_info.getPerformanceScore();
    double platform2_score = result2.platform_info.getPerformanceScore();
    
    if (std::abs(platform1_score - platform2_score) > 20.0) {
        explanation << "Significant platform capability difference detected\n";
    }
    
    // Interference impact
    if (result1.interference_report.hasInterference() && !result2.interference_report.hasInterference()) {
        explanation << "Platform 1 had system interference during testing\n";
    } else if (!result1.interference_report.hasInterference() && result2.interference_report.hasInterference()) {
        explanation << "Platform 2 had system interference during testing\n";
    }
    
    // Reliability impact
    if (std::abs(result1.reliability_score - result2.reliability_score) > 20.0) {
        explanation << "Significant difference in benchmark reliability scores\n";
    }
    
    return explanation.str();
}

}
