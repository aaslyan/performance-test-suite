#ifndef REPORT_H
#define REPORT_H

#include "benchmark.h"
#include <vector>
#include <string>
#include <fstream>

class Report {
private:
    std::vector<BenchmarkResult> results;
    std::string system_info;
    std::string timestamp;
    
    std::string toJSON() const;
    std::string toMarkdown() const;
    std::string toTXT() const;  // Added TXT format
    std::string getCurrentTimestamp() const;
    
public:
    Report();
    void addResult(const BenchmarkResult& result);
    void setSystemInfo(const std::string& info);
    void writeToFile(const std::string& filename, const std::string& format = "json");
    void printToConsole(const std::string& format = "txt");  // Changed default to txt
};

#endif