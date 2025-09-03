#include "visualization.h"
#include "comparison.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

std::string ASCIIChart::getColorCode(const std::string& status, bool use_colors)
{
    if (!use_colors)
        return "";

    if (status == "IMPROVED")
        return "\033[32m"; // Green
    else if (status == "UNCHANGED")
        return "\033[37m"; // White
    else if (status == "DEGRADED")
        return "\033[33m"; // Yellow
    else if (status == "CRITICAL")
        return "\033[31m"; // Red
    else
        return "\033[0m"; // Reset
}

std::string ASCIIChart::formatValue(double value, const std::string& unit, int precision)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    if (!unit.empty()) {
        ss << " " << unit;
    }
    return ss.str();
}

double ASCIIChart::normalizeValue(double value, double min_val, double max_val)
{
    if (max_val == min_val)
        return 0.5;
    return (value - min_val) / (max_val - min_val);
}

std::string ASCIIChart::createBar(int length, char fill_char, char empty_char, int total_width)
{
    std::string bar;
    for (int i = 0; i < total_width; ++i) {
        bar += (i < length) ? fill_char : empty_char;
    }
    return bar;
}

std::string ASCIIChart::padString(const std::string& str, int width, bool right_align)
{
    if (str.length() >= static_cast<size_t>(width)) {
        return str.substr(0, width);
    }

    int padding = width - str.length();
    if (right_align) {
        return std::string(padding, ' ') + str;
    } else {
        return str + std::string(padding, ' ');
    }
}

std::string ASCIIChart::generateBarChart(
    const std::vector<DataPoint>& data,
    const std::string& title,
    const ChartConfig& config)
{
    std::stringstream chart;

    // Title
    chart << "\n+" << std::string(config.width + 2, '-') << "+\n";
    chart << "| " << padString(title, config.width) << " |\n";
    chart << "+" << std::string(config.width + 2, '-') << "+\n";

    if (data.empty()) {
        chart << "| " << padString("No data available", config.width) << " |\n";
        chart << "+" << std::string(config.width + 2, '-') << "+\n";
        return chart.str();
    }

    // Find min and max values for normalization
    double min_val = data[0].value;
    double max_val = data[0].value;
    for (const auto& point : data) {
        min_val = std::min(min_val, point.value);
        max_val = std::max(max_val, point.value);
    }

    // Ensure we have some range
    if (max_val == min_val) {
        max_val += 1.0;
    }

    // Calculate label width
    int max_label_width = 0;
    for (const auto& point : data) {
        max_label_width = std::max(max_label_width, static_cast<int>(point.label.length()));
    }
    max_label_width = std::min(max_label_width, config.width / 3);

    int bar_width = config.width - max_label_width - 15; // Space for value display

    // Generate bars
    for (const auto& point : data) {
        double normalized = normalizeValue(point.value, min_val, max_val);
        int bar_length = static_cast<int>(normalized * bar_width);

        std::string color = getColorCode(point.status, config.use_colors);
        std::string reset = config.use_colors ? "\033[0m" : "";

        chart << "| " << padString(point.label, max_label_width) << " ";
        chart << color << createBar(bar_length, config.bar_char, config.empty_char, bar_width) << reset;

        if (config.show_values) {
            std::string value_str = formatValue(point.value, point.unit);
            chart << " " << padString(value_str, 12, true);
        }
        chart << " |\n";
    }

    chart << "+" << std::string(config.width + 2, '-') << "+\n";
    return chart.str();
}

std::string ASCIIChart::generateChangeChart(
    const std::vector<std::pair<std::string, double>>& changes,
    const std::string& title,
    double warning_threshold,
    double critical_threshold,
    const ChartConfig& config)
{
    std::stringstream chart;

    // Title
    chart << "\n+" << std::string(config.width + 2, '-') << "+\n";
    chart << "| " << padString(title + " (% Change)", config.width) << " |\n";
    chart << "+" << std::string(config.width + 2, '-') << "+\n";

    if (changes.empty()) {
        chart << "| " << padString("No change data available", config.width) << " |\n";
        chart << "+" << std::string(config.width + 2, '-') << "+\n";
        return chart.str();
    }

    // Find max absolute change for scaling
    double max_abs_change = 0;
    for (const auto& change : changes) {
        max_abs_change = std::max(max_abs_change, std::abs(change.second));
    }

    if (max_abs_change == 0) {
        max_abs_change = 1.0;
    }

    // Calculate layout
    int max_label_width = 0;
    for (const auto& change : changes) {
        max_label_width = std::max(max_label_width, static_cast<int>(change.first.length()));
    }
    max_label_width = std::min(max_label_width, config.width / 3);

    int bar_area_width = config.width - max_label_width - 15;
    int center = bar_area_width / 2;

    // Generate change bars
    for (const auto& change : changes) {
        double normalized_change = change.second / max_abs_change;
        int bar_length = static_cast<int>(std::abs(normalized_change) * center);

        std::string status;
        if (std::abs(change.second) <= 5.0) {
            status = "UNCHANGED";
        } else if (std::abs(change.second) <= warning_threshold) {
            status = change.second > 0 ? "IMPROVED" : "DEGRADED";
        } else if (std::abs(change.second) <= critical_threshold) {
            status = "DEGRADED";
        } else {
            status = "CRITICAL";
        }

        std::string color = getColorCode(status, config.use_colors);
        std::string reset = config.use_colors ? "\033[0m" : "";

        chart << "| " << padString(change.first, max_label_width) << " ";

        // Create the bar
        std::string bar_display(bar_area_width, ' ');
        char bar_char = (change.second >= 0) ? '>' : '<';

        if (change.second >= 0) {
            // Positive change - bar goes right from center
            for (int i = 0; i < bar_length; ++i) {
                if (center + i < bar_area_width) {
                    bar_display[center + i] = bar_char;
                }
            }
        } else {
            // Negative change - bar goes left from center
            for (int i = 0; i < bar_length; ++i) {
                if (center - i - 1 >= 0) {
                    bar_display[center - i - 1] = bar_char;
                }
            }
        }

        // Add center line
        bar_display[center] = '|';

        chart << color << bar_display << reset;

        if (config.show_values) {
            std::string change_str = (change.second >= 0 ? "+" : "") + formatValue(change.second, "%", 1);
            chart << " " << padString(change_str, 8, true);
        }

        chart << " |\n";
    }

    // Add threshold indicators
    chart << "+" << std::string(config.width + 2, '-') << "+\n";
    chart << "| Thresholds: ";
    chart << "\033[33mWarning " << warning_threshold << "%\033[0m, ";
    chart << "\033[31mCritical " << critical_threshold << "%\033[0m";
    chart << std::string(config.width - 35, ' ') << " |\n";

    chart << "+" << std::string(config.width + 2, '-') << "+\n";
    return chart.str();
}

std::string ASCIIChart::generateComparisonChart(
    const std::vector<std::pair<DataPoint, DataPoint>>& comparisons,
    const std::string& title,
    const ChartConfig& config)
{
    std::stringstream chart;

    // Title
    chart << "\n+" << std::string(config.width + 2, '-') << "+\n";
    chart << "| " << padString(title + " (Baseline vs Current)", config.width) << " |\n";
    chart << "+" << std::string(config.width + 2, '-') << "+\n";

    if (comparisons.empty()) {
        chart << "| " << padString("No comparison data available", config.width) << " |\n";
        chart << "+" << std::string(config.width + 2, '-') << "+\n";
        return chart.str();
    }

    // Simple comparison display
    for (const auto& pair : comparisons) {
        const auto& baseline = pair.first;
        const auto& current = pair.second;

        double change = ((current.value - baseline.value) / baseline.value) * 100.0;
        std::string change_str = (change >= 0 ? "+" : "") + formatValue(change, "%", 1);
        
        std::string color = getColorCode(current.status, config.use_colors);
        std::string reset = config.use_colors ? "\033[0m" : "";

        chart << "| " << padString(baseline.label, 20) << " ";
        chart << formatValue(baseline.value, baseline.unit, 2) << " -> ";
        chart << formatValue(current.value, current.unit, 2) << " ";
        chart << color << "(" << change_str << ")" << reset;
        chart << " |\n";
    }

    chart << "+" << std::string(config.width + 2, '-') << "+\n";
    return chart.str();
}

std::string ASCIIChart::generateTrendChart(
    const std::vector<std::vector<DataPoint>>& time_series,
    const std::vector<std::string>& time_labels,
    const std::string& title,
    const ChartConfig& config)
{
    std::stringstream chart;

    chart << "\n+" << std::string(config.width + 2, '-') << "+\n";
    chart << "| " << padString(title + " (Trend Over Time)", config.width) << " |\n";
    chart << "+" << std::string(config.width + 2, '-') << "+\n";

    // Placeholder for future implementation
    chart << "| " << padString("Trend charts coming in future update!", config.width) << " |\n";
    chart << "+" << std::string(config.width + 2, '-') << "+\n";

    return chart.str();
}

// ComparisonVisualizer implementation
std::string ComparisonVisualizer::generateComparisonCharts(
    const std::vector<BenchmarkComparison>& comparisons,
    const ASCIIChart::ChartConfig& config)
{
    std::stringstream result;

    // Generate overall performance change chart
    std::vector<std::pair<std::string, double>> changes;
    std::vector<std::pair<ASCIIChart::DataPoint, ASCIIChart::DataPoint>> comparison_data;

    for (const auto& bench_comp : comparisons) {
        for (const auto& metric : bench_comp.metrics) {
            // Add to change chart
            std::string label = bench_comp.benchmark_name + " " + metric.metric_name;
            changes.push_back({ label, metric.percent_change });

            // Add to comparison chart
            ASCIIChart::DataPoint baseline;
            baseline.label = label;
            baseline.value = metric.baseline_value;
            baseline.unit = metric.unit;
            baseline.status = "UNCHANGED";

            ASCIIChart::DataPoint current;
            current.label = label;
            current.value = metric.current_value;
            current.unit = metric.unit;
            
            switch (metric.status) {
                case MetricComparison::IMPROVED: current.status = "IMPROVED"; break;
                case MetricComparison::UNCHANGED: current.status = "UNCHANGED"; break;
                case MetricComparison::DEGRADED: current.status = "DEGRADED"; break;
                case MetricComparison::CRITICAL: current.status = "CRITICAL"; break;
            }

            comparison_data.push_back({ baseline, current });
        }
    }

    // Generate percentage change chart
    result << ASCIIChart::generateChangeChart(changes, "Performance Changes", 10.0, 25.0, config);

    // Generate comparison chart
    result << ASCIIChart::generateComparisonChart(comparison_data, "Baseline vs Current", config);

    return result.str();
}

std::string ComparisonVisualizer::generateMetricChart(
    const std::string& metric_name,
    const std::vector<MetricComparison>& metrics,
    const ASCIIChart::ChartConfig& config)
{
    std::vector<ASCIIChart::DataPoint> data;

    for (const auto& metric : metrics) {
        ASCIIChart::DataPoint current_point;
        current_point.label = "Current";
        current_point.value = metric.current_value;
        current_point.unit = metric.unit;
        
        switch (metric.status) {
            case MetricComparison::IMPROVED: current_point.status = "IMPROVED"; break;
            case MetricComparison::UNCHANGED: current_point.status = "UNCHANGED"; break;
            case MetricComparison::DEGRADED: current_point.status = "DEGRADED"; break;
            case MetricComparison::CRITICAL: current_point.status = "CRITICAL"; break;
        }
        data.push_back(current_point);

        // Add baseline for comparison
        ASCIIChart::DataPoint baseline_point;
        baseline_point.label = "Baseline";
        baseline_point.value = metric.baseline_value;
        baseline_point.unit = metric.unit;
        baseline_point.status = "UNCHANGED";
        data.push_back(baseline_point);
    }

    return ASCIIChart::generateBarChart(data, metric_name + " Comparison", config);
}