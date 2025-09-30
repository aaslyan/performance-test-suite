# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

Build using CMake (minimum version 2.8):
```bash
cmake .
make
```

The build outputs a single executable `perf_test`.

## Running Tests

### Basic benchmark execution:
```bash
# Run all benchmarks with default settings
./perf_test --modules=all --duration=30

# Run specific module
./perf_test --modules=cpu --duration=60 --verbose

# Save results to JSON
./perf_test --modules=cpu,mem --report=results.json --format=json
```

### Comparison mode:
```bash
# Compare two benchmark runs
./perf_test --compare --baseline=old.json --current=new.json

# With ASCII charts
./perf_test --compare --baseline=old.json --current=new.json --chart
```

### Performance context mode:
```bash
# Run with system monitoring and analysis
./perf_test --context --modules=cpu --verbose

# Check system readiness for benchmarking
./perf_test --system-check

# Show platform information
./perf_test --platform-info
```

### Consistency testing:
```bash
# Run consistency tests (5 runs by default)
./consistency_test.sh

# Custom configuration
./consistency_test.sh <runs> <modules> <duration> <iterations>
./consistency_test.sh 10 cpu,mem 60 5
```

## Architecture

### Core Abstractions

**Benchmark interface** (`benchmark.h`): All benchmarks implement the `Benchmark` abstract class with `run()` and `getName()` methods. Results are returned in `BenchmarkResult` structs containing throughput, latency, and metadata.

**Reporting system** (`report.h`): The `Report` class aggregates benchmark results and exports to TXT, JSON, or Markdown formats via `writeToFile()` and `printToConsole()`.

**Comparison engine** (`comparison.h`): `ComparisonEngine` loads two JSON reports, calculates metric deltas and percent changes, determines status (IMPROVED/DEGRADED/CRITICAL) based on configurable thresholds, and generates comparison reports with optional ASCII charts.

**Performance context** (`performance_context.h`): `PerformanceContextAnalyzer` wraps benchmark execution with system monitoring via `SystemMonitor`, platform detection via `PlatformDetector`, calculates reliability scores, and provides contextual warnings about interference or suboptimal conditions.

### Benchmark Modules

Each benchmark module (cpu_bench.cpp, mem_bench.cpp, disk_bench.cpp, net_bench.cpp, ipc_bench.cpp, integrated_bench.cpp) implements the `Benchmark` interface. Modules are instantiated based on CLI arguments in `main.cpp:createBenchmarks()`.

### System Monitoring

`SystemMonitor` (`system_monitor.h`) captures resource metrics (CPU, memory, I/O) during benchmark execution. Used in context mode to detect interference and calculate reliability scores.

`PlatformDetector` (`platform_detector.h`) identifies hardware characteristics, performance capabilities, and potential bottlenecks. Used for cross-platform result normalization and optimization recommendations.

### Command Line Interface

`main.cpp` handles argument parsing with `getopt_long`, supports three modes:
1. **Benchmark mode**: Run performance tests
2. **Comparison mode**: Compare two JSON reports (triggered by `--compare`)
3. **Context mode**: Enhanced benchmarking with system analysis (triggered by `--context`)

The `Config` struct holds all parsed options and drives execution flow in `main()`.

## Key Design Patterns

- **Modular benchmarks**: Each benchmark is self-contained and implements the same interface
- **RAII resource management**: Temporary files, network sockets, shared memory cleaned up automatically
- **Production-safe defaults**: Loopback networking, temp files, no sudo required (though perf counters may need permissions)
- **Multiple output formats**: Programmatic (JSON), documentation (Markdown), human-readable (TXT)
- **Threshold-based comparison**: Warning/critical thresholds for automated regression detection

## Important Implementation Notes

- **Metric directionality**: Some metrics are "lower is better" (latency), others are "higher is better" (throughput). The comparison engine uses `higher_is_better` flags to correctly interpret deltas.
- **Hardware performance counters**: Perf counters (CPU cycles, cache misses, etc.) are collected via `perf_event_open` when permissions allow. Can be disabled with `--no-perf`.
- **Telemetry export**: System metrics during benchmark runs can be exported to files with `--telemetry=file.json`.
- **Build metadata**: Compiler info, build type, and CMake version are embedded in results via compile-time defines.

## Testing Consistency

The `consistency_test.sh` script runs multiple benchmark iterations with system monitoring, calculates coefficient of variation (CV) for metrics, and categorizes consistency as EXCELLENT/GOOD/FAIR/POOR. Lower CV indicates more repeatable results.

## Platform Compatibility

Supports Linux (CentOS/Ubuntu) and macOS. Linux-specific features:
- CPU affinity (`_GNU_SOURCE` define)
- RT library for POSIX shared memory
- More comprehensive perf counter support

macOS limitations:
- Perf counters may be unavailable
- Different system monitoring APIs (vm_stat vs /proc)
