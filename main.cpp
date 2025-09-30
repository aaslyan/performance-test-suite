#include <algorithm>
#include <cctype>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "benchmark.h"
#include "comparison.h"
#include "cpu_bench.h"
#include "disk_bench.h"
#include "integrated_bench.h"
#include "ipc_bench.h"
#include "mem_bench.h"
#include "net_bench.h"
#include "performance_context.h"
#include "report.h"
#include "utils.h"

struct Config {
    std::vector<std::string> modules;
    int duration = 30;
    int iterations = 10;
    std::string report_file;
    std::string report_format = "txt";
    bool verbose = false;
    bool help = false;

    // Comparison mode
    bool compare_mode = false;
    std::string baseline_file;
    std::string current_file;
    std::string compare_format = "text";
    double warning_threshold = 10.0;
    double critical_threshold = 25.0;
    bool show_charts = false;
    
    // Performance context options
    bool context_mode = false;
    bool system_check = false;
    bool show_platform_info = false;

    // Telemetry & instrumentation
    std::string telemetry_file;
    bool dry_run = false;
    bool enable_perf_counters = true;
};

void printUsage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "\nBenchmark Mode Options:\n"
              << "  --modules=LIST      Comma-separated list of modules to run\n"
              << "                      (cpu,mem,disk,net,ipc,integrated,all)\n"
              << "                      Default: all\n"
              << "  --duration=SEC      Duration in seconds per test (default: 30)\n"
              << "  --iterations=N      Number of iterations for averaging (default: 10)\n"
              << "  --report=FILE       Output report file (default: stdout)\n"
              << "  --format=FORMAT     Report format: txt, json, or markdown (default: txt)\n"
              << "  --verbose           Enable verbose output\n"
              << "\nComparison Mode Options:\n"
              << "  --compare           Enable comparison mode\n"
              << "  --baseline=FILE     Baseline JSON report file\n"
              << "  --current=FILE      Current JSON report file\n"
              << "  --compare-format=FORMAT  Comparison format: text or markdown (default: text)\n"
              << "  --chart             Show ASCII charts in comparison output\n"
              << "  --warning=PCT       Warning threshold percentage (default: 10.0)\n"
              << "  --critical=PCT      Critical threshold percentage (default: 25.0)\n"
              << "\nPerformance Context Options:\n"
              << "  --context           Enable contextual benchmarking with system monitoring\n"
              << "  --system-check      Check system readiness for benchmarking\n"
              << "  --platform-info     Show detailed platform information\n"
              << "\nGeneral Options:\n"
              << "  --help              Show this help message\n"
              << "\nExamples:\n"
              << "  Benchmark: " << program_name << " --modules=cpu --duration=60 --report=results.json\n"
              << "  Compare:   " << program_name << " --compare --baseline=old.json --current=new.json\n"
              << "  Charts:    " << program_name << " --compare --baseline=old.json --current=new.json --chart\n"
              << "  Context:   " << program_name << " --context --modules=cpu --verbose\n"
              << "  SysCheck:  " << program_name << " --system-check\n"
              << "  Platform:  " << program_name << " --platform-info\n"
              << std::endl;
}

std::vector<std::string> splitString(const std::string& str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

Config parseArguments(int argc, char* argv[])
{
    Config config;

    static struct option long_options[] = {
        { "modules", required_argument, nullptr, 'm' },
        { "duration", required_argument, nullptr, 'd' },
        { "iterations", required_argument, nullptr, 'i' },
        { "report", required_argument, nullptr, 'r' },
        { "format", required_argument, nullptr, 'f' },
        { "verbose", no_argument, nullptr, 'v' },
        { "help", no_argument, nullptr, 'h' },
        { "compare", no_argument, nullptr, 'c' },
        { "baseline", required_argument, nullptr, 'b' },
        { "current", required_argument, nullptr, 'n' },
        { "compare-format", required_argument, nullptr, 'F' },
        { "chart", no_argument, nullptr, 'H' },
        { "warning", required_argument, nullptr, 'w' },
        { "critical", required_argument, nullptr, 'C' },
        { "context", no_argument, nullptr, 'x' },
        { "system-check", no_argument, nullptr, 's' },
        { "platform-info", no_argument, nullptr, 'p' },
        { "telemetry", required_argument, nullptr, 'T' },
        { "dry-run", no_argument, nullptr, 'D' },
        { "no-perf", no_argument, nullptr, 'P' },
        { nullptr, 0, nullptr, 0 }
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "m:d:i:r:f:vhcb:n:F:Hw:C:xspT:DP", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'm':
            config.modules = splitString(optarg, ',');
            break;
        case 'd':
            config.duration = std::stoi(optarg);
            if (config.duration <= 0) {
                std::cerr << "Duration must be positive\n";
                exit(1);
            }
            break;
        case 'i':
            config.iterations = std::stoi(optarg);
            if (config.iterations <= 0) {
                std::cerr << "Iterations must be positive\n";
                exit(1);
            }
            break;
        case 'r':
            config.report_file = optarg;
            break;
        case 'f':
            config.report_format = optarg;
            if (config.report_format != "json" && config.report_format != "markdown" && config.report_format != "txt") {
                std::cerr << "Format must be 'txt', 'json', or 'markdown'\n";
                exit(1);
            }
            break;
        case 'v':
            config.verbose = true;
            break;
        case 'h':
            config.help = true;
            return config;
        case 'c':
            config.compare_mode = true;
            break;
        case 'b':
            config.baseline_file = optarg;
            break;
        case 'n':
            config.current_file = optarg;
            break;
        case 'F':
            config.compare_format = optarg;
            if (config.compare_format != "text" && config.compare_format != "markdown") {
                std::cerr << "Compare format must be 'text' or 'markdown'\n";
                exit(1);
            }
            break;
        case 'H':
            config.show_charts = true;
            break;
        case 'w':
            config.warning_threshold = std::stod(optarg);
            if (config.warning_threshold < 0) {
                std::cerr << "Warning threshold must be non-negative\n";
                exit(1);
            }
            break;
        case 'C':
            config.critical_threshold = std::stod(optarg);
            if (config.critical_threshold < 0) {
                std::cerr << "Critical threshold must be non-negative\n";
                exit(1);
            }
            break;
        case 'x':
            config.context_mode = true;
            break;
        case 's':
            config.system_check = true;
            break;
        case 'p':
            config.show_platform_info = true;
            break;
        default:
            printUsage(argv[0]);
            exit(1);
        }
    }

    if (config.modules.empty()) {
        config.modules = { "all" };
    }

    // Expand "all" to include all modules
    if (std::find(config.modules.begin(), config.modules.end(), "all") != config.modules.end()) {
        config.modules = { "cpu", "mem", "disk", "net", "ipc", "integrated" };
    }

    return config;
}

std::vector<std::unique_ptr<Benchmark>> createBenchmarks(const std::vector<std::string>& modules)
{
    std::vector<std::unique_ptr<Benchmark>> benchmarks;

    for (const auto& module : modules) {
        if (module == "cpu") {
            benchmarks.push_back(std::make_unique<CPUBenchmark>());
        } else if (module == "mem") {
            benchmarks.push_back(std::make_unique<MemoryBenchmark>());
        } else if (module == "disk") {
            benchmarks.push_back(std::make_unique<DiskBenchmark>());
        } else if (module == "net") {
            benchmarks.push_back(std::make_unique<NetworkBenchmark>());
        } else if (module == "ipc") {
            benchmarks.push_back(std::make_unique<IPCBenchmark>());
        } else if (module == "integrated") {
            benchmarks.push_back(std::make_unique<IntegratedBenchmark>());
        } else {
            std::cerr << "Unknown module: " << module << std::endl;
        }
    }

    return benchmarks;
}

int main(int argc, char* argv[])
{
    Config config = parseArguments(argc, argv);

    int effective_duration = config.dry_run ? std::max(1, std::min(config.duration, 3)) : config.duration;
    int effective_iterations = config.dry_run ? 1 : config.iterations;

    if (config.help) {
        printUsage(argv[0]);
        return 0;
    }

    // Handle performance context modes
    PerformanceContextAnalyzer analyzer;
    const auto build_metadata = getBuildMetadataMap();
    
    if (config.show_platform_info) {
        std::cout << "Platform Information\n";
        std::cout << "===================\n\n";
        
        PlatformInfo platform = analyzer.getCurrentPlatform();
        std::cout << platform.getSummary() << "\n\n";
        std::cout << "Performance Score: " << platform.getPerformanceScore() << "/100\n\n";
        
        if (!platform.performance_issues.empty()) {
            std::cout << "Performance Issues:\n";
            for (const auto& issue : platform.performance_issues) {
                std::cout << "- " << issue << "\n";
            }
            std::cout << "\n";
        }
        
        std::vector<std::string> recommendations = analyzer.getPreBenchmarkRecommendations();
        if (!recommendations.empty()) {
            std::cout << "Optimization Recommendations:\n";
            for (const auto& rec : recommendations) {
                std::cout << "- " << rec << "\n";
            }
        }
        return 0;
    }
    
    if (config.system_check) {
        std::cout << "System Readiness Check\n";
        std::cout << "=====================\n\n";
        
        std::cout << PerformanceContext::getSystemReadinessReport();
        
        bool ready = PerformanceContext::isSystemBenchmarkReady();
        std::cout << "\nSystem Ready for Benchmarking: " << (ready ? "YES" : "NO") << "\n\n";
        
        std::vector<std::string> tips = PerformanceContext::getQuickOptimizationTips();
        if (!tips.empty()) {
            std::cout << "Quick Optimization Tips:\n";
            for (const auto& tip : tips) {
                std::cout << "- " << tip << "\n";
            }
        }
        return 0;
    }

    // Handle comparison mode
    if (config.compare_mode) {
        if (config.baseline_file.empty() || config.current_file.empty()) {
            std::cerr << "Error: Both --baseline and --current files are required for comparison mode\n";
            return 1;
        }

        ComparisonEngine engine;
        engine.setThresholds(config.warning_threshold, config.critical_threshold);

        if (!engine.loadBaselineReport(config.baseline_file)) {
            std::cerr << "Error: Failed to load baseline report: " << config.baseline_file << std::endl;
            return 1;
        }

        if (!engine.loadCurrentReport(config.current_file)) {
            std::cerr << "Error: Failed to load current report: " << config.current_file << std::endl;
            return 1;
        }

        std::string report;
        if (config.show_charts) {
            report = engine.generateReportWithCharts(config.compare_format);
        } else {
            report = engine.generateReport(config.compare_format);
        }
        std::cout << report;

        // Exit with appropriate code based on health status
        auto health = engine.getOverallHealth();
        return (health == ComparisonEngine::CRITICAL) ? 2 : (health == ComparisonEngine::WARNING) ? 1
                                                                                                  : 0;
    }

    SystemMonitor telemetry_monitor;
    bool telemetry_enabled = !config.telemetry_file.empty();
    if (telemetry_enabled) {
        if (config.verbose) {
            std::cout << "Telemetry capture enabled: writing samples to " << config.telemetry_file << "\n";
        }
        telemetry_monitor.startMonitoring();
    }

    std::cout << "Performance Test Suite v1.0\n";
    std::cout << "===========================\n\n";

    if (config.dry_run) {
        std::cout << "Dry run mode active: duration " << effective_duration
                  << "s, iterations " << effective_iterations << "\n\n";
    }

    if (config.verbose) {
        std::cout << "Configuration:\n";
        std::cout << "  Modules: ";
        for (const auto& m : config.modules)
            std::cout << m << ' ';
        std::cout << "\n";
        std::cout << "  Duration: " << effective_duration << " seconds";
        if (config.dry_run && config.duration != effective_duration) {
            std::cout << " (requested " << config.duration << ")";
        }
        std::cout << "\n";
        std::cout << "  Iterations: " << effective_iterations;
        if (config.dry_run && config.iterations != effective_iterations) {
            std::cout << " (requested " << config.iterations << ")";
        }
        std::cout << "\n\n";
    }

    std::string system_info = getSystemInfo();
    std::string build_info = getBuildMetadataSummary();
    if (!build_info.empty()) {
        if (!system_info.empty() && system_info.back() != '\n') {
            system_info += '\n';
        }
        system_info += build_info;
    }
    if (config.verbose) {
        std::cout << "System Information:\n"
                  << system_info << "\n";
    }

    Report report;
    report.setSystemInfo(system_info);

    // Create benchmarks
    auto benchmarks = createBenchmarks(config.modules);

    if (benchmarks.empty()) {
        if (telemetry_enabled) {
            telemetry_monitor.stopMonitoring();
        }
        std::cerr << "No valid benchmarks to run\n";
        return 1;
    }

    // Run benchmarks
    if (config.context_mode) {
        std::cout << "Running benchmarks with performance context analysis...\n\n";

        // Show system environment first
        if (config.verbose) {
            std::cout << "System Environment Analysis:\n";
            std::cout << analyzer.getPlatformSummary() << "\n";

            PerformanceEnvironment env = analyzer.analyzeCurrentEnvironment();
            if (!env.is_optimal_for_benchmarking) {
                std::cout << "Warning: System environment is not optimal for benchmarking\n";
                std::cout << "Environment Score: " << env.environment_score << "/100\n";
                for (const auto& issue : env.environment_issues) {
                    std::cout << "- " << issue << "\n";
                }
                std::cout << "\n";
            }
        }

        for (auto& benchmark : benchmarks) {
            std::cout << "Running " << benchmark->getName() << " benchmark with context analysis...\n";

            try {
                ContextualBenchmarkResult contextual_result = analyzer.runBenchmarkWithContext(
                    benchmark.get(), effective_duration, effective_iterations, config.verbose, config.enable_perf_counters);

                for (const auto& entry : build_metadata) {
                    contextual_result.benchmark_result.extra_info[entry.first] = entry.second;
                }

                // Add the base result to the report
                report.addResult(contextual_result.benchmark_result);

                // Show contextual information
                std::cout << "\nContextual Analysis:\n";
                std::cout << "  Reliability Score: " << static_cast<int>(contextual_result.reliability_score) << "/100\n";
                std::cout << "  Status: " << contextual_result.benchmark_result.status << "\n";

                if (contextual_result.benchmark_result.status == "success") {
                    std::cout << "  Result: " << PerformanceContext::interpretThroughputResult(
                        contextual_result.benchmark_result.throughput,
                        contextual_result.benchmark_result.throughput_unit) << "\n";
                    std::cout << "  Latency: " << PerformanceContext::interpretLatencyResult(
                        contextual_result.benchmark_result.avg_latency,
                        contextual_result.benchmark_result.latency_unit) << "\n";
                }

                std::cout << "  Reliability: " << PerformanceContext::interpretReliabilityScore(
                    contextual_result.reliability_score) << "\n";

                if (contextual_result.interference_report.hasInterference()) {
                    std::cout << "  Interference: " << contextual_result.interference_report.getSummary() << "\n";
                }

                if (!contextual_result.context_warnings.empty() && config.verbose) {
                    std::cout << "  Warnings:\n";
                    for (const auto& warning : contextual_result.context_warnings) {
                        std::cout << "    - " << warning << "\n";
                    }
                }

                if (!contextual_result.optimization_suggestions.empty() && config.verbose) {
                    std::cout << "  Suggestions:\n";
                    for (const auto& suggestion : contextual_result.optimization_suggestions) {
                        std::cout << "    - " << suggestion << "\n";
                    }
                }

            } catch (const std::exception& e) {
                std::cerr << "  Error: " << e.what() << "\n";
                BenchmarkResult error_result;
                error_result.name = benchmark->getName();
                error_result.status = "error";
                error_result.error_message = e.what();
                report.addResult(error_result);
            }

            std::cout << std::endl;
        }
    } else {
        // Standard benchmark mode
        for (auto& benchmark : benchmarks) {
            std::cout << "Running " << benchmark->getName() << " benchmark...\n";

            try {
                PerfCounterSet perf_counters;
                PerfCounterSample perf_sample;
                bool perf_started = config.enable_perf_counters && perf_counters.start();

                BenchmarkResult result = benchmark->run(effective_duration, effective_iterations, config.verbose);

                if (config.enable_perf_counters) {
                    perf_sample = perf_counters.stop();
                    if (perf_sample.valid) {
                        result.extra_metrics["perf_cpu_cycles"] = static_cast<double>(perf_sample.cycles);
                        result.extra_metrics["perf_cpu_instructions"] = static_cast<double>(perf_sample.instructions);
                        result.extra_metrics["perf_l3_cache_misses"] = static_cast<double>(perf_sample.cache_misses);
                        result.extra_metrics["perf_branches"] = static_cast<double>(perf_sample.branches);
                        result.extra_metrics["perf_branch_misses"] = static_cast<double>(perf_sample.branch_misses);
                        if (perf_sample.instructions > 0) {
                            result.extra_metrics["perf_cpi"] = static_cast<double>(perf_sample.cycles) /
                                static_cast<double>(perf_sample.instructions);
                        }
                        result.extra_info["perf.counters"] = "perf_event_open";
                    } else {
                        result.extra_info["perf.counters"] = perf_started ? "unavailable" : "insufficient_permissions";
                    }
                } else {
                    result.extra_info["perf.counters"] = "disabled";
                    perf_counters.stop();
                }

                for (const auto& entry : build_metadata) {
                    result.extra_info[entry.first] = entry.second;
                }

                report.addResult(result);

                if (config.verbose) {
                    std::cout << "  Status: " << result.status << "\n";
                    if (result.status == "success") {
                        std::cout << "  Throughput: " << result.throughput << " " << result.throughput_unit << "\n";
                        std::cout << "  Avg Latency: " << result.avg_latency << " " << result.latency_unit << "\n";
                    } else {
                        std::cout << "  Error: " << result.error_message << "\n";
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "  Error: " << e.what() << "\n";
                BenchmarkResult error_result;
                error_result.name = benchmark->getName();
                error_result.status = "error";
                error_result.error_message = e.what();
                report.addResult(error_result);
            }

            std::cout << std::endl;
        }
    }

    if (telemetry_enabled) {
        telemetry_monitor.stopMonitoring();
        if (!telemetry_monitor.writeSamplesToFile(config.telemetry_file)) {
            std::cerr << "Warning: Unable to write telemetry samples to " << config.telemetry_file << std::endl;
        } else if (config.verbose) {
            std::cout << "Telemetry written to: " << config.telemetry_file << std::endl;
        }
    }

    if (config.report_file.empty()) {
        report.printToConsole(config.report_format);
    } else {
        report.writeToFile(config.report_file, config.report_format);
        std::cout << "Report written to: " << config.report_file << std::endl;
    }
    return 0;
}
