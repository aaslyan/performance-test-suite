# Performance Testing Tutorial

This tutorial demonstrates how to use the automated testing script (`test.sh`) to perform comprehensive performance testing and comparison analysis.

## Overview

The `test.sh` script automates the process of:
1. Running a baseline performance test
2. Running two additional tests for comparison
3. Performing three comparisons between all test combinations
4. Generating detailed reports with color-coded results

## Prerequisites

1. **Build the project:**
   ```bash
   make clean && make
   ```

2. **Verify the binary exists:**
   ```bash
   ls -la perf_test
   ```

## Quick Start

### Basic Usage
```bash
# Run with default settings (10 seconds, cpu+mem modules)
./test.sh

# Run with custom duration (30 seconds)
./test.sh 30

# Run with custom duration and modules
./test.sh 15 "cpu,mem,disk"
```

### Advanced Usage
```bash
# Full comprehensive test (all modules, longer duration)
./test.sh 60 "all"

# Quick smoke test (minimal modules, short duration)
./test.sh 5 "cpu"

# Network and IPC focused test
./test.sh 20 "net,ipc"
```

## Understanding the Output

### Test Execution Flow
```
┌─────────────────┐
│  TEST 1         │
│  (Baseline)     │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│  TEST 2         │
│  (Current 1)    │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│  TEST 3         │
│  (Current 2)    │
└─────────────────┘
         │
         ▼
┌─────────────────┐
│  COMPARISONS    │
│  1. Base vs T1  │
│  2. Base vs T2  │
│  3. T1 vs T2    │
└─────────────────┘
```

### Exit Codes
- **0 (HEALTHY):** All comparisons show acceptable performance
- **1 (WARNING):** Some performance degradation detected
- **2 (CRITICAL):** Significant performance issues found

### Status Indicators
- 🟢 **[IMPROVED]** - Performance better than baseline
- ⚪ **[UNCHANGED]** - Performance within acceptable thresholds
- 🟡 **[DEGRADED]** - Performance degraded but acceptable (warning)
- 🔴 **[CRITICAL]** - Performance critically degraded

## Example Test Run

```bash
$ ./test.sh 15 "cpu,mem"

=====================================
Performance Test Suite - Automated Testing
=====================================

ℹ Test Configuration:
  Duration per test: 15 seconds
  Modules to test: cpu,mem
  Output directory: test_results
  Timestamp: 20240120_143022

=====================================
TEST 1: Creating Baseline
=====================================

ℹ Running baseline test...
Performance Test Suite v1.0
===========================

Running CPU benchmark...
Running Memory benchmark...

✓ Baseline test completed: test_results/baseline_20240120_143022.json

ℹ Waiting 5 seconds before next test...

=====================================
TEST 2: First Comparison Test
=====================================

ℹ Running first comparison test...
Performance Test Suite v1.0
===========================

Running CPU benchmark...
Running Memory benchmark...

✓ First comparison test completed: test_results/current1_20240120_143022.json

=====================================
COMPARISON 1: Baseline vs Test 1
=====================================

ℹ Comparing baseline with first test...

=====================================
    PERFORMANCE COMPARISON REPORT
=====================================

System Information
------------------
Baseline System:
  OS: Darwin 24.6.0
  CPU: Apple M2
  Memory: 16 GB

Current System:
  OS: Darwin 24.6.0
  CPU: Apple M2  
  Memory: 16 GB

Benchmark Comparisons
====================

CPU Benchmark
-------------
  [UNCHANGED] Throughput: 1245.30 -> 1251.45 MFLOPS (+0.49%)
  [UNCHANGED] Avg Latency: 0.85 -> 0.83 ms (-2.35%)
  Overall: PASSED

Memory Benchmark
----------------
  [IMPROVED] Throughput: 8456.20 -> 8523.10 MB/s (+0.79%)
  [UNCHANGED] Avg Latency: 125.30 -> 123.45 ns (-1.48%)
  Overall: PASSED

=====================================
OVERALL STATUS: HEALTHY - All benchmarks within acceptable thresholds
=====================================

✓ Comparison 1: HEALTHY - Performance within acceptable thresholds
```

## File Organization

After running tests, you'll find these files in `test_results/`:

```
test_results/
├── baseline_20240120_143022.json      # Baseline test results
├── current1_20240120_143022.json      # First comparison test
├── current2_20240120_143022.json      # Second comparison test
└── summary_20240120_143022.md         # Markdown summary report
```

### File Contents
- **JSON files:** Complete benchmark results in JSON format
- **Markdown report:** Comprehensive summary with all comparisons

## Practical Use Cases

### 1. System Performance Validation
```bash
# Test after system changes
./test.sh 30 "all"
```
Use this when you've made system changes (OS updates, hardware changes, configuration tweaks) and want to validate performance impact.

### 2. Performance Regression Testing
```bash
# Before code deployment
./test.sh 60 "cpu,mem,disk"
```
Run before deploying new software to establish baseline, then run again after deployment to detect regressions.

### 3. Hardware Comparison
```bash
# Compare different servers
./test.sh 45 "all"
```
Run on different hardware configurations to compare performance characteristics.

### 4. Load Testing Preparation
```bash
# Quick consistency check
./test.sh 10 "cpu,mem"
```
Verify system performance consistency before running extensive load tests.

## Interpreting Results

### Healthy Results
```
✓ Comparison 1: HEALTHY - Performance within acceptable thresholds
✓ Comparison 2: HEALTHY - Performance within acceptable thresholds  
✓ Comparison 3: HEALTHY - Performance consistent between tests
```
**Interpretation:** System performance is stable and consistent.

### Warning Results
```
⚠ Comparison 1: WARNING - Some performance degradation detected
✓ Comparison 2: HEALTHY - Performance within acceptable thresholds
⚠ Comparison 3: WARNING - Some variation between tests
```
**Interpretation:** Performance is mostly acceptable but shows some inconsistency. Investigate potential causes.

### Critical Results
```
✗ Comparison 1: CRITICAL - Significant performance degradation detected
✗ Comparison 2: CRITICAL - Significant performance degradation detected
✓ Comparison 3: HEALTHY - Performance consistent between tests
```
**Interpretation:** Consistent performance degradation from baseline. Immediate investigation required.

## Troubleshooting

### Common Issues

1. **"perf_test binary not found"**
   ```bash
   make clean && make
   ```

2. **Permission denied**
   ```bash
   chmod +x test.sh
   ```

3. **Tests hanging**
   - Reduce duration: `./test.sh 5`
   - Use fewer modules: `./test.sh 10 "cpu"`

4. **High performance variation**
   - Ensure system is idle during tests
   - Close unnecessary applications
   - Disable CPU frequency scaling
   - Use longer test duration for stable results

### Best Practices

1. **Consistent Environment:**
   - Run tests on idle system
   - Close unnecessary applications
   - Use consistent power settings

2. **Appropriate Duration:**
   - Short tests (5-10s): Quick validation
   - Medium tests (15-30s): Regular monitoring
   - Long tests (60s+): Detailed analysis

3. **Module Selection:**
   - `"cpu"`: Fast, CPU-only testing
   - `"cpu,mem"`: Balanced performance test
   - `"all"`: Comprehensive system test

4. **Threshold Tuning:**
   ```bash
   # Custom thresholds for sensitive systems
   ./perf_test --compare --baseline=baseline.json --current=current.json \
               --warning=5.0 --critical=15.0
   ```

## CI/CD Integration

### Example Jenkins Pipeline
```groovy
pipeline {
    agent any
    stages {
        stage('Performance Test') {
            steps {
                sh './test.sh 30 "cpu,mem,disk"'
            }
            post {
                always {
                    archiveArtifacts 'test_results/*'
                }
                failure {
                    emailext (
                        subject: "Performance Test Failed",
                        body: "Performance degradation detected. Check artifacts.",
                        to: "${env.CHANGE_AUTHOR_EMAIL}"
                    )
                }
            }
        }
    }
}
```

### GitHub Actions Example
```yaml
name: Performance Test
on: [push, pull_request]
jobs:
  performance:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Build
      run: make
    - name: Run Performance Tests
      run: ./test.sh 20 "cpu,mem"
    - name: Upload Results
      uses: actions/upload-artifact@v2
      if: always()
      with:
        name: performance-results
        path: test_results/
```

## Advanced Features

### Custom Comparison Scripts
Create your own comparison workflows:
```bash
#!/bin/bash
# Custom comparison between different configurations

# Test configuration A
export CONFIG_A=true
./perf_test --modules="all" --duration=30 --format=json --report=config_a.json

# Test configuration B  
export CONFIG_A=false
export CONFIG_B=true
./perf_test --modules="all" --duration=30 --format=json --report=config_b.json

# Compare
./perf_test --compare --baseline=config_a.json --current=config_b.json
```

This tutorial provides comprehensive guidance for using the automated testing system to ensure consistent and reliable performance monitoring.