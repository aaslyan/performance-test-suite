#include <algorithm>
#include <cctype>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "benchmark.h"
#include "cpu_bench.h"
#include "disk_bench.h"
#include "integrated_bench.h"
#include "ipc_bench.h"
#include "mem_bench.h"
#include "net_bench.h"
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
};

void printUsage(const char* program_name)
{
    std::cout << "Usage: " << program_name << " [OPTIONS]\n"
              << "\nOptions:\n"
              << "  --modules=LIST      Comma-separated list of modules to run\n"
              << "                      (cpu,mem,disk,net,ipc,integrated,all)\n"
              << "                      Default: all\n"
              << "  --duration=SEC      Duration in seconds per test (default: 30)\n"
              << "  --iterations=N      Number of iterations for averaging (default: 10)\n"
              << "  --report=FILE       Output report file (default: stdout)\n"
              << "  --format=FORMAT     Report format: txt, json, or markdown (default: txt)\n"
              << "  --verbose           Enable verbose output\n"
              << "  --help              Show this help message\n"
              << "\nExample:\n"
              << "  " << program_name << " --modules=cpu --duration=60 --report=results.json\n"
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
        { nullptr, 0, nullptr, 0 }
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "m:d:i:r:f:vh", long_options, &option_index)) != -1) {
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

    if (config.help) {
        printUsage(argv[0]);
        return 0;
    }

    std::cout << "Performance Test Suite v1.0\n";
    std::cout << "===========================\n\n";

    if (config.verbose) {
        std::cout << "Configuration:\n";
        std::cout << "  Modules: ";
        for (const auto& m : config.modules)
            std::cout << m << " ";
        std::cout << "\n";
        std::cout << "  Duration: " << config.duration << " seconds\n";
        std::cout << "  Iterations: " << config.iterations << "\n\n";
    }

    std::string system_info = getSystemInfo();
    if (config.verbose) {
        std::cout << "System Information:\n"
                  << system_info << "\n";
    }

    Report report;
    report.setSystemInfo(system_info);

    // Create benchmarks
    auto benchmarks = createBenchmarks(config.modules);

    if (benchmarks.empty()) {
        std::cerr << "No valid benchmarks to run\n";
        return 1;
    }

    // Run benchmarks
    for (auto& benchmark : benchmarks) {
        std::cout << "Running " << benchmark->getName() << " benchmark...\n";

        try {
            BenchmarkResult result = benchmark->run(config.duration, config.iterations, config.verbose);
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

    if (config.report_file.empty()) {
        report.printToConsole(config.report_format);
    } else {
        report.writeToFile(config.report_file, config.report_format);
        std::cout << "Report written to: " << config.report_file << std::endl;
    }

    return 0;
}