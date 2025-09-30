#include "report.h"
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Report::Report()
{
    timestamp = getCurrentTimestamp();
}

std::string Report::getCurrentTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Report::addResult(const BenchmarkResult& result)
{
    results.push_back(result);
}

void Report::setSystemInfo(const std::string& info)
{
    system_info = info;
}

std::string Report::toTXT() const
{
    std::stringstream txt;

    txt << "=====================================\n";
    txt << "    PERFORMANCE TEST REPORT\n";
    txt << "=====================================\n\n";

    txt << "Generated: " << timestamp << "\n\n";

    txt << "System Information:\n";
    txt << "-------------------\n";
    txt << system_info << "\n";

    txt << "Benchmark Results:\n";
    txt << "==================\n\n";

    for (const auto& r : results) {
        txt << "+-- " << r.name << " ";
        for (size_t i = r.name.length(); i < 47; ++i)
            txt << "-";
        txt << "+\n";

        if (r.status == "success") {
            txt << "| Status:          SUCCESS" << std::string(30, ' ') << "|\n";
            txt << "| Throughput:      " << std::fixed << std::setprecision(2)
                << std::left << std::setw(10) << r.throughput
                << " " << std::left << std::setw(19) << r.throughput_unit << "|\n";
            txt << "| Avg Latency:     " << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << r.avg_latency
                << " " << std::left << std::setw(19) << r.latency_unit << "|\n";
            txt << "| Min Latency:     " << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << r.min_latency
                << " " << std::left << std::setw(19) << r.latency_unit << "|\n";
            txt << "| Max Latency:     " << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << r.max_latency
                << " " << std::left << std::setw(19) << r.latency_unit << "|\n";
            txt << "| P50 Latency:     " << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << r.p50_latency
                << " " << std::left << std::setw(19) << r.latency_unit << "|\n";
            txt << "| P90 Latency:     " << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << r.p90_latency
                << " " << std::left << std::setw(19) << r.latency_unit << "|\n";
            txt << "| P99 Latency:     " << std::fixed << std::setprecision(3)
                << std::left << std::setw(10) << r.p99_latency
                << " " << std::left << std::setw(19) << r.latency_unit << "|\n";

            if (!r.extra_metrics.empty() || !r.extra_info.empty()) {
                txt << "|" << std::string(54, ' ') << "|\n";
                txt << "| Additional Metrics:" << std::string(33, ' ') << "|\n";
                for (const auto& metric : r.extra_metrics) {
                    std::string name = metric.first;
                    if (name.length() > 15) {
                        name = name.substr(0, 12) + "...";
                    }
                    txt << "|   " << std::left << std::setw(15) << name << ": "
                        << std::fixed << std::setprecision(3) << std::left << std::setw(27)
                        << metric.second << "|\n";
                }
                for (const auto& info : r.extra_info) {
                    std::string name = info.first;
                    if (name.length() > 15) {
                        name = name.substr(0, 12) + "...";
                    }
                    txt << "|   " << std::left << std::setw(15) << name << ": "
                        << std::left << std::setw(27) << info.second << "|\n";
                }
            }
        } else {
            txt << "| Status:          ERROR" << std::string(32, ' ') << "|\n";
            txt << "| Error:           " << std::left << std::setw(36) << r.error_message << "|\n";
        }

        txt << "+" << std::string(54, '-') << "+\n\n";
    }

    // Summary table
    txt << "SUMMARY\n";
    txt << "=======\n\n";
    txt << std::left << std::setw(18) << "Benchmark"
        << std::setw(12) << "Status"
        << std::setw(20) << "Throughput"
        << std::setw(15) << "Avg Latency" << "\n";
    txt << std::string(65, '-') << "\n";

    for (const auto& r : results) {
        txt << std::left << std::setw(18) << r.name
            << std::setw(12) << r.status;
        if (r.status == "success") {
            std::stringstream throughput_str;
            throughput_str << std::fixed << std::setprecision(2) << r.throughput << " " << r.throughput_unit;
            std::stringstream latency_str;
            latency_str << std::fixed << std::setprecision(3) << r.avg_latency << " " << r.latency_unit;

            txt << std::setw(20) << throughput_str.str()
                << std::setw(15) << latency_str.str();
        } else {
            txt << std::setw(20) << "N/A"
                << std::setw(15) << "N/A";
        }
        txt << "\n";
    }

    txt << "\n=====================================\n";
    txt << "Report generated by Performance Test Suite v1.0\n";
    txt << "=====================================\n";

    return txt.str();
}

std::string Report::toJSON() const
{
    std::stringstream json;

    json << "{\n";
    json << "  \"timestamp\": \"" << timestamp << "\",\n";
    json << "  \"system_info\": \"" << system_info << "\",\n";
    json << "  \"benchmarks\": [\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        json << "    {\n";
        json << "      \"name\": \"" << r.name << "\",\n";
        json << "      \"status\": \"" << r.status << "\",\n";

        if (r.status == "success") {
            json << "      \"throughput\": " << r.throughput << ",\n";
            json << "      \"throughput_unit\": \"" << r.throughput_unit << "\",\n";
            json << "      \"latency\": {\n";
            json << "        \"average\": " << r.avg_latency << ",\n";
            json << "        \"minimum\": " << r.min_latency << ",\n";
            json << "        \"maximum\": " << r.max_latency << ",\n";
            json << "        \"p50\": " << r.p50_latency << ",\n";
            json << "        \"p90\": " << r.p90_latency << ",\n";
            json << "        \"p99\": " << r.p99_latency << ",\n";
            json << "        \"unit\": \"" << r.latency_unit << "\"\n";
            json << "      }";

            bool has_numeric_metrics = !r.extra_metrics.empty();
            bool has_text_metrics = !r.extra_info.empty();
            if (has_numeric_metrics) {
                json << ",\n      \"extra_metrics\": {\n";
                bool first = true;
                for (const auto& metric : r.extra_metrics) {
                    if (!first)
                        json << ",\n";
                    json << "        \"" << metric.first << "\": " << metric.second;
                    first = false;
                }
                json << "\n      }";
            }
            if (has_text_metrics) {
                json << ",\n      \"extra_info\": {\n";
                bool first_info = true;
                for (const auto& info : r.extra_info) {
                    if (!first_info)
                        json << ",\n";
                    json << "        \"" << info.first << "\": \"" << info.second << "\"";
                    first_info = false;
                }
                json << "\n      }";
            }
        } else {
            json << "      \"error_message\": \"" << r.error_message << "\"";
        }

        json << "\n    }";
        if (i < results.size() - 1)
            json << ",";
        json << "\n";
    }

    json << "  ]\n";
    json << "}\n";

    return json.str();
}

std::string Report::toMarkdown() const
{
    std::stringstream md;

    md << "# Performance Test Report\n\n";
    md << "**Generated:** " << timestamp << "\n\n";

    md << "## System Information\n\n";
    md << "```\n"
       << system_info << "```\n\n";

    md << "## Benchmark Results\n\n";

    for (const auto& r : results) {
        md << "### " << r.name << "\n\n";

        if (r.status == "success") {
            md << "| Metric | Value | Unit |\n";
            md << "|--------|-------|------|\n";
            md << "| Throughput | " << std::fixed << std::setprecision(2)
               << r.throughput << " | " << r.throughput_unit << " |\n";
            md << "| Average Latency | " << std::fixed << std::setprecision(3)
               << r.avg_latency << " | " << r.latency_unit << " |\n";
            md << "| Min Latency | " << std::fixed << std::setprecision(3)
               << r.min_latency << " | " << r.latency_unit << " |\n";
            md << "| Max Latency | " << std::fixed << std::setprecision(3)
               << r.max_latency << " | " << r.latency_unit << " |\n";
            md << "| P50 Latency | " << std::fixed << std::setprecision(3)
               << r.p50_latency << " | " << r.latency_unit << " |\n";
            md << "| P90 Latency | " << std::fixed << std::setprecision(3)
               << r.p90_latency << " | " << r.latency_unit << " |\n";
            md << "| P99 Latency | " << std::fixed << std::setprecision(3)
               << r.p99_latency << " | " << r.latency_unit << " |\n";

            if (!r.extra_metrics.empty() || !r.extra_info.empty()) {
                md << "\n**Additional Metrics:**\n\n";
                md << "| Metric | Value |\n";
                md << "|--------|-------|\n";
                for (const auto& metric : r.extra_metrics) {
                    md << "| " << metric.first << " | "
                       << std::fixed << std::setprecision(3) << metric.second << " |\n";
                }
                for (const auto& info : r.extra_info) {
                    md << "| " << info.first << " | " << info.second << " |\n";
                }
            }
        } else {
            md << "**Status:** Error\n\n";
            md << "**Error Message:** " << r.error_message << "\n";
        }

        md << "\n";
    }

    // Summary table
    md << "## Summary\n\n";
    md << "| Benchmark | Status | Throughput | Avg Latency |\n";
    md << "|-----------|--------|------------|-------------|\n";

    for (const auto& r : results) {
        md << "| " << r.name << " | " << r.status << " | ";
        if (r.status == "success") {
            md << std::fixed << std::setprecision(2) << r.throughput
               << " " << r.throughput_unit << " | "
               << std::fixed << std::setprecision(3) << r.avg_latency
               << " " << r.latency_unit;
        } else {
            md << "N/A | N/A";
        }
        md << " |\n";
    }

    md << "\n";

    return md.str();
}

void Report::writeToFile(const std::string& filename, const std::string& format)
{
    std::ofstream file(filename);
    if (!file) {
        throw std::runtime_error("Failed to open report file: " + filename);
    }

    if (format == "json") {
        file << toJSON();
    } else if (format == "markdown") {
        file << toMarkdown();
    } else if (format == "txt") {
        file << toTXT();
    } else {
        throw std::runtime_error("Unsupported report format: " + format + " (supported: json, markdown, txt)");
    }

    file.close();
}

void Report::printToConsole(const std::string& format)
{
    if (format == "json") {
        std::cout << toJSON();
    } else if (format == "markdown") {
        std::cout << toMarkdown();
    } else if (format == "txt") {
        std::cout << toTXT();
    } else {
        throw std::runtime_error("Unsupported report format: " + format + " (supported: json, markdown, txt)");
    }
}
