#ifndef PERFORMANCE_CONTEXT_H
#define PERFORMANCE_CONTEXT_H

#include "platform_detector.h"
#include "report.h"
#include "system_monitor.h"
#include <map>
#include <string>
#include <vector>

// Enhanced benchmark result with system context
struct ContextualBenchmarkResult {
    BenchmarkResult benchmark_result;
    ResourceMetrics resource_metrics_avg;
    ResourceMetrics resource_metrics_peak;
    InterferenceReport interference_report;
    PlatformInfo platform_info;
    
    // Analysis results
    double reliability_score; // 0-100, how reliable the benchmark result is
    std::vector<std::string> context_warnings;
    std::vector<std::string> optimization_suggestions;
    
    ContextualBenchmarkResult();
    std::string toJsonWithContext() const;
};

// Performance environment analysis
struct PerformanceEnvironment {
    PlatformInfo platform;
    ResourceMetrics baseline_resources;
    InterferenceReport environment_interference;
    OptimizationRecommendations optimizations;
    
    bool is_optimal_for_benchmarking;
    double environment_score; // 0-100
    std::vector<std::string> environment_issues;
    std::vector<std::string> pre_benchmark_recommendations;
    
    PerformanceEnvironment();
    std::string getSummary() const;
};

// Cross-platform comparison with context
struct ContextualComparison {
    std::vector<ContextualBenchmarkResult> results;
    std::vector<PlatformInfo> platforms;
    
    // Analysis
    bool platforms_comparable;
    std::string comparison_validity;
    std::vector<std::string> comparison_caveats;
    std::map<std::string, double> normalized_scores; // Platform-adjusted scores
    
    ContextualComparison();
    std::string generateComparisonReport() const;
};

class PerformanceContextAnalyzer {
private:
    PlatformDetector platform_detector;
    SystemMonitor system_monitor;
    
    // Analysis methods
    double calculateReliabilityScore(
        const BenchmarkResult& result,
        const ResourceMetrics& metrics,
        const InterferenceReport& interference);
    
    std::vector<std::string> generateContextWarnings(
        const BenchmarkResult& result,
        const ResourceMetrics& metrics,
        const InterferenceReport& interference,
        const PlatformInfo& platform);
    
    std::vector<std::string> generateOptimizationSuggestions(
        const BenchmarkResult& result,
        const ResourceMetrics& metrics,
        const PlatformInfo& platform);
    
    double calculateEnvironmentScore(const PerformanceEnvironment& env);
    
public:
    PerformanceContextAnalyzer();
    ~PerformanceContextAnalyzer();
    
    // Environment analysis
    PerformanceEnvironment analyzeCurrentEnvironment();
    bool isEnvironmentOptimalForBenchmarking();
    std::vector<std::string> getPreBenchmarkRecommendations();
    
    // Benchmark execution with monitoring
    ContextualBenchmarkResult runBenchmarkWithContext(
        Benchmark* benchmark, 
        int duration_seconds = 10, 
        int iterations = 5, 
        bool verbose = false,
        bool collect_perf_counters = true);
    
    // Analysis methods
    ContextualBenchmarkResult analyzeBenchmarkResult(
        const BenchmarkResult& result,
        const ResourceMetrics& resource_avg,
        const ResourceMetrics& resource_peak,
        const InterferenceReport& interference);
    
    // Comparison methods
    ContextualComparison compareResults(
        const std::vector<ContextualBenchmarkResult>& results);
    
    static bool areResultsComparable(
        const ContextualBenchmarkResult& result1,
        const ContextualBenchmarkResult& result2);
    
    // Utility methods
    void warmupSystem(int seconds = 5);
    void cooldownSystem(int seconds = 3);
    ResourceMetrics getSystemBaseline(int sample_duration = 10);
    
    // Platform detection shortcuts
    PlatformInfo getCurrentPlatform();
    std::string getPlatformSummary();
    std::vector<std::string> getPerformanceIssues();
};

// Utility functions for performance context
namespace PerformanceContext {
    // Quick environment checks
    bool isSystemBenchmarkReady();
    std::string getSystemReadinessReport();
    std::vector<std::string> getQuickOptimizationTips();
    
    // Result interpretation helpers
    std::string interpretThroughputResult(double throughput, const std::string& unit);
    std::string interpretLatencyResult(double latency, const std::string& unit);
    std::string interpretReliabilityScore(double score);
    
    // Platform comparison utilities
    double calculatePlatformAdjustmentFactor(
        const PlatformInfo& reference_platform,
        const PlatformInfo& target_platform);
    
    std::string explainPerformanceDifference(
        const ContextualBenchmarkResult& result1,
        const ContextualBenchmarkResult& result2);
}

#endif
