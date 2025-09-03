#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <getopt.h>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "benchmark.h"
#include "cpu_bench.h"
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

void printUsage(const char* program_name) {
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

std::vector<std::string> splitString(const std::string& str, char delimiter) {
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

Config parseArguments(int argc, char* argv[]) {
    Config config;
    
    static struct option long_options[] = {
        {"modules", required_argument, nullptr, 'm'},
        {"duration", required_argument, nullptr, 'd'},
        {"iterations", required_argument, nullptr, 'i'},
        {"report", required_argument, nullptr, 'r'},
        {"format", required_argument, nullptr, 'f'},
        {"verbose", no_argument, nullptr, 'v'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
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
        config.modules = {"cpu"};  // Default to just CPU for now
    }
    
    return config;
}

int main(int argc, char* argv[]) {
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
        for (const auto& m : config.modules) std::cout << m << " ";
        std::cout << "\n";
        std::cout << "  Duration: " << config.duration << " seconds\n";
        std::cout << "  Iterations: " << config.iterations << "\n\n";
    }
    
    std::string system_info = getSystemInfo();
    if (config.verbose) {
        std::cout << "System Information:\n" << system_info << "\n";
    }
    
    Report report;
    report.setSystemInfo(system_info);
    
    // For now, just run CPU benchmark as a basic test
    if (std::find(config.modules.begin(), config.modules.end(), "cpu") != config.modules.end()) {
        std::cout << "Running CPU benchmark...\n";
        
        try {
            CPUBenchmark cpu_bench;
            BenchmarkResult result = cpu_bench.run(config.duration, config.iterations, config.verbose);
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