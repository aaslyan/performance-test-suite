#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include <string>
#include <vector>

// ASCII chart visualization for performance comparisons
class ASCIIChart {
public:
    struct DataPoint {
        std::string label;
        double value;
        std::string unit;
        std::string status; // "IMPROVED", "UNCHANGED", "DEGRADED", "CRITICAL"
    };

    struct ChartConfig {
        int width;           // Chart width in characters
        int height;          // Chart height in characters
        bool show_values;    // Show numeric values
        bool use_colors;     // Use ANSI colors
        char bar_char;       // Character for bars
        char empty_char;     // Character for empty space
        
        ChartConfig() 
            : width(60), height(20), show_values(true), use_colors(true), 
              bar_char('#'), empty_char('-') {}
    };

    // Generate horizontal bar chart
    static std::string generateBarChart(
        const std::vector<DataPoint>& data,
        const std::string& title,
        const ChartConfig& config = ChartConfig()
    );

    // Generate comparison chart (baseline vs current)
    static std::string generateComparisonChart(
        const std::vector<std::pair<DataPoint, DataPoint>>& comparisons,
        const std::string& title,
        const ChartConfig& config = ChartConfig()
    );

    // Generate trend chart (multiple data points over time)
    static std::string generateTrendChart(
        const std::vector<std::vector<DataPoint>>& time_series,
        const std::vector<std::string>& time_labels,
        const std::string& title,
        const ChartConfig& config = ChartConfig()
    );

    // Generate percentage change chart
    static std::string generateChangeChart(
        const std::vector<std::pair<std::string, double>>& changes,
        const std::string& title,
        double warning_threshold = 10.0,
        double critical_threshold = 25.0,
        const ChartConfig& config = ChartConfig()
    );

private:
    // Helper functions
    static std::string getColorCode(const std::string& status, bool use_colors);
    static std::string formatValue(double value, const std::string& unit, int precision = 2);
    static double normalizeValue(double value, double min_val, double max_val);
    static std::string createBar(int length, char fill_char, char empty_char, int total_width);
    static std::string padString(const std::string& str, int width, bool right_align = false);
};

// Integration with comparison engine
class ComparisonVisualizer {
public:
    static std::string generateComparisonCharts(
        const std::vector<struct BenchmarkComparison>& comparisons,
        const ASCIIChart::ChartConfig& config = ASCIIChart::ChartConfig()
    );

    static std::string generateMetricChart(
        const std::string& metric_name,
        const std::vector<struct MetricComparison>& metrics,
        const ASCIIChart::ChartConfig& config = ASCIIChart::ChartConfig()
    );
};

#endif // VISUALIZATION_H