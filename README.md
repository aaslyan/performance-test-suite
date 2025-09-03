# Performance Test Suite

A comprehensive C++ performance benchmarking tool for production servers running CentOS and Ubuntu. This tool measures CPU, memory, disk I/O, network, and inter-process communication (IPC) performance with detailed throughput and latency metrics.

## Features

- **CPU Benchmarks**: Multi-threaded throughput, cache latency measurements
- **Memory Benchmarks**: Sequential/random access, bandwidth testing  
- **Disk I/O Benchmarks**: Sequential/random read/write performance, IOPS
- **Network Benchmarks**: TCP/UDP performance using standard sockets
- **IPC Benchmarks**: Shared memory communication performance
- **Multiple Output Formats**: TXT, JSON, and Markdown reports
- **Production Safe**: Non-destructive tests using temporary files and loopback networking
- **Cross-Platform**: Works on Linux (CentOS/Ubuntu) and macOS

## Requirements

- C++17 compatible compiler (g++, clang++)
- CMake 3.10+
- POSIX-compliant system
- pthread support

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

### Basic Usage

```bash
# Run all benchmarks with TXT output (default)
./perf_test --modules=all --duration=30

# Run specific benchmark
./perf_test --modules=cpu --duration=60 --verbose

# Save results to file
./perf_test --modules=cpu,mem --report=results.json --format=json
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--modules=LIST` | Comma-separated modules: cpu,mem,disk,net,ipc,integrated,all | cpu |
| `--duration=SEC` | Duration in seconds per test | 30 |
| `--iterations=N` | Number of iterations for averaging | 10 |
| `--report=FILE` | Output report file | stdout |
| `--format=FORMAT` | Report format: txt, json, markdown | txt |
| `--verbose` | Enable verbose output | false |
| `--help` | Show help message | - |

### Output Formats

#### TXT Format (Human Readable)
```
=====================================
    PERFORMANCE TEST REPORT
=====================================

+-- CPU --------------------------------------------+
| Status:          SUCCESS                              |
| Throughput:      2.22       GOPS               |
| Avg Latency:     0.002      us/op              |
+------------------------------------------------------+
```

#### JSON Format (Machine Readable)
```json
{
  "timestamp": "2025-09-03 14:40:44",
  "system_info": "OS: Linux...",
  "benchmarks": [
    {
      "name": "CPU",
      "status": "success", 
      "throughput": 2.22,
      "throughput_unit": "GOPS"
    }
  ]
}
```

#### Markdown Format (Documentation)
```markdown
# Performance Test Report

| Metric | Value | Unit |
|--------|-------|------|
| Throughput | 2.22 | GOPS |
```

## Benchmark Modules

### CPU (`--modules=cpu`)
- **Throughput**: GFLOPS, integer operations per second
- **Latency**: Cache access times (L1/L2/L3/Memory)  
- **Multi-threading**: Utilizes all available CPU cores

### Memory (`--modules=mem`) 
- **Bandwidth**: Sequential and random access patterns
- **Latency**: Memory access latency distribution
- **Contention**: Multi-threaded memory access performance

### Disk I/O (`--modules=disk`)
- **Sequential**: Large block read/write performance
- **Random**: Small block IOPS measurements
- **Sync**: Durability testing with fsync

### Network (`--modules=net`)
- **TCP**: Connection-oriented throughput and latency
- **UDP**: Packet-based performance and loss measurements
- **Loopback**: Safe testing using 127.0.0.1

### IPC (`--modules=ipc`)
- **Shared Memory**: POSIX shared memory performance
- **Synchronization**: Semaphore coordination overhead
- **Message Sizes**: Testing various data transfer sizes

## Architecture

The tool is designed with modularity and safety in mind:

- **Modular Design**: Each benchmark is an independent module
- **Production Safe**: Uses temporary files, loopback networking, configurable limits
- **Error Handling**: Graceful failure handling and cleanup
- **Resource Management**: RAII principles for automatic cleanup
- **Portable**: Standard C++17 and POSIX APIs only

## Examples

```bash
# Quick CPU test
./perf_test --modules=cpu --duration=10 --verbose

# Comprehensive system benchmark  
./perf_test --modules=all --duration=60 --report=system_bench.json

# Memory-focused analysis
./perf_test --modules=mem --iterations=20 --format=markdown --report=memory_analysis.md

# Compare different durations
./perf_test --modules=cpu --duration=5 --report=short.json
./perf_test --modules=cpu --duration=60 --report=long.json
```

## Development Status

- ✅ **CPU Benchmarks** - Fully implemented
- ✅ **Reporting System** - TXT/JSON/Markdown formats  
- ✅ **Command Line Interface** - Complete argument parsing
- ⏳ **Memory Benchmarks** - In development
- ⏳ **Disk I/O Benchmarks** - In development
- ⏳ **Network Benchmarks** - In development
- ⏳ **IPC Benchmarks** - In development

## Contributing

1. Follow existing code style and conventions
2. Add comprehensive error handling
3. Include appropriate unit tests
4. Update documentation for new features
5. Ensure cross-platform compatibility

## License

[License information to be added]

## System Requirements

### Minimum Requirements
- 1GB RAM
- 1GB free disk space  
- Network interface (for network tests)

### Tested Platforms
- CentOS 7/8
- Ubuntu 20.04/22.04
- macOS (development)

For detailed implementation information, see `plan.txt`.