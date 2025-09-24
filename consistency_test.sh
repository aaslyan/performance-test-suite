#!/bin/bash

# Performance Consistency Test Script
# Tests benchmark repeatability and identifies variance sources

set -e

# Configuration
RUNS=${1:-5}                    # Number of test runs (default: 5)
MODULES=${2:-"cpu,mem,disk"}    # Modules to test (default: cpu,mem,disk)
DURATION=${3:-30}                # Duration per test in seconds (default: 30)
ITERATIONS=${4:-3}               # Iterations per run (default: 3)
OUTPUT_DIR="consistency_results_$(date +%Y%m%d_%H%M%S)"
COOLDOWN_TIME=10                 # Seconds between runs
WARMUP_TIME=5                    # Warmup before each run

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Log function
log() {
    echo -e "${GREEN}[$(date '+%H:%M:%S')]${NC} $1" | tee -a "$OUTPUT_DIR/test.log"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$OUTPUT_DIR/test.log"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$OUTPUT_DIR/test.log"
}

info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$OUTPUT_DIR/test.log"
}

# System preparation function
prepare_system() {
    log "Preparing system for consistent benchmarking..."

    # Skip both system-check and platform-info as they may require sudo
    # ./perf_test --system-check > "$OUTPUT_DIR/system_check.txt" 2>&1
    # ./perf_test --platform-info > "$OUTPUT_DIR/platform_info.txt" 2>&1

    # Just record basic system info without using perf_test
    echo "Test started at: $(date)" > "$OUTPUT_DIR/platform_info.txt"
    echo "OS: $(uname -a)" >> "$OUTPUT_DIR/platform_info.txt"

    # Get baseline system metrics
    log "Capturing baseline system metrics..."
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        top -l 1 -n 0 > "$OUTPUT_DIR/baseline_top.txt" 2>&1
        vm_stat > "$OUTPUT_DIR/baseline_memory.txt" 2>&1
    else
        # Linux
        top -bn1 > "$OUTPUT_DIR/baseline_top.txt" 2>&1
        free -h > "$OUTPUT_DIR/baseline_memory.txt" 2>&1
        cat /proc/loadavg > "$OUTPUT_DIR/baseline_load.txt" 2>&1
    fi
}

# Monitor system during run
monitor_system() {
    local run_id=$1
    local monitor_file="$OUTPUT_DIR/monitor_run${run_id}.txt"

    while [ -f "$OUTPUT_DIR/.monitoring" ]; do
        if [[ "$OSTYPE" == "darwin"* ]]; then
            # macOS monitoring
            echo "=== $(date '+%H:%M:%S') ===" >> "$monitor_file"
            top -l 1 -n 5 | grep -E "CPU|PhysMem" >> "$monitor_file" 2>&1
        else
            # Linux monitoring
            echo "=== $(date '+%H:%M:%S') ===" >> "$monitor_file"
            top -bn1 | head -5 >> "$monitor_file" 2>&1
            free -h | grep Mem >> "$monitor_file" 2>&1
        fi
        sleep 2
    done
}

# Run single benchmark
run_benchmark() {
    local run_id=$1
    local output_file="$OUTPUT_DIR/run${run_id}.json"

    log "Starting run $run_id/$RUNS..."

    # Start monitoring in background
    touch "$OUTPUT_DIR/.monitoring"
    monitor_system $run_id &
    local monitor_pid=$!

    # Warmup
    info "Warming up system for $WARMUP_TIME seconds..."
    sleep $WARMUP_TIME

    # Run benchmark
    log "Executing benchmark (modules: $MODULES, duration: ${DURATION}s, iterations: $ITERATIONS)..."
    ./perf_test \
        --modules="$MODULES" \
        --duration="$DURATION" \
        --iterations="$ITERATIONS" \
        --report="$output_file" \
        --format=json \
        --verbose > "$OUTPUT_DIR/run${run_id}_output.txt" 2>&1

    local exit_code=$?

    # Stop monitoring
    rm -f "$OUTPUT_DIR/.monitoring"
    wait $monitor_pid 2>/dev/null

    if [ $exit_code -eq 0 ]; then
        log "Run $run_id completed successfully"
    else
        error "Run $run_id failed with exit code $exit_code"
        return $exit_code
    fi

    # Cooldown
    if [ $run_id -lt $RUNS ]; then
        info "Cooling down for $COOLDOWN_TIME seconds..."
        sleep $COOLDOWN_TIME
    fi

    return 0
}

# Parse JSON and extract metrics
extract_metrics() {
    local json_file=$1
    local module=$2
    local metric=$3

    # Using Python for reliable JSON parsing
    python3 -c "
import json
import sys

try:
    with open('$json_file', 'r') as f:
        data = json.load(f)

    # Navigate through the benchmark results structure
    for benchmark in data.get('benchmarks', []):
        if '$module' in benchmark.get('name', '').lower():
            metrics = benchmark.get('metrics', {})
            if '$metric' in metrics:
                print(metrics['$metric'])
                sys.exit(0)

    # If not found in standard structure, try alternative paths
    if 'results' in data:
        for result in data['results']:
            if '$module' in result.get('benchmark', '').lower():
                if '$metric' in result:
                    print(result['$metric'])
                    sys.exit(0)

except Exception as e:
    sys.stderr.write(f'Error parsing JSON: {e}\n')
    sys.exit(1)
" 2>/dev/null || echo "N/A"
}

# Calculate statistics
calculate_stats() {
    local values=("$@")
    local count=${#values[@]}

    if [ $count -eq 0 ]; then
        echo "N/A N/A N/A N/A"
        return
    fi

    # Calculate using Python for accuracy
    python3 -c "
import sys
import math

values = [float(x) for x in '$*'.split() if x != 'N/A']
if not values:
    print('N/A N/A N/A N/A')
    sys.exit(0)

mean = sum(values) / len(values)
variance = sum((x - mean) ** 2 for x in values) / len(values)
std_dev = math.sqrt(variance)
cv = (std_dev / mean * 100) if mean != 0 else 0
min_val = min(values)
max_val = max(values)

print(f'{mean:.2f} {std_dev:.2f} {cv:.2f} {min_val:.2f} {max_val:.2f}')
"
}

# Analyze consistency
analyze_consistency() {
    log "Analyzing consistency across $RUNS runs..."

    echo "======================================" > "$OUTPUT_DIR/consistency_report.txt"
    echo "Performance Consistency Analysis" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "======================================" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "Test Configuration:" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "  Runs: $RUNS" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "  Modules: $MODULES" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "  Duration: ${DURATION}s per test" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "  Iterations: $ITERATIONS per run" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "  Timestamp: $(date)" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "" >> "$OUTPUT_DIR/consistency_report.txt"

    # Define metrics to analyze for each module
    # Using case statement instead of associative array for compatibility
    get_module_metrics() {
        case $1 in
            cpu) echo "throughput_ops latency_ns";;
            mem) echo "bandwidth_mbps latency_ns";;
            disk) echo "read_mbps write_mbps";;
            net) echo "throughput_mbps latency_ms";;
            *) echo "throughput latency";;
        esac
    }

    echo "Metric Analysis:" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "----------------" >> "$OUTPUT_DIR/consistency_report.txt"

    # Analyze each module
    for module in $(echo $MODULES | tr ',' ' '); do
        echo "" >> "$OUTPUT_DIR/consistency_report.txt"
        echo "[$module benchmark]" >> "$OUTPUT_DIR/consistency_report.txt"

        # Get metrics for this module
        metrics=$(get_module_metrics $module)

        for metric in $metrics; do
            values=()

            # Extract metric values from all runs
            for ((i=1; i<=RUNS; i++)); do
                value=$(extract_metrics "$OUTPUT_DIR/run${i}.json" "$module" "$metric")
                if [ "$value" != "N/A" ]; then
                    values+=("$value")
                fi
            done

            if [ ${#values[@]} -gt 0 ]; then
                # Calculate statistics
                stats=$(calculate_stats "${values[@]}")
                read mean std_dev cv min_val max_val <<< "$stats"

                # Determine consistency level
                consistency="EXCELLENT"
                if (( $(echo "$cv > 15" | bc -l) )); then
                    consistency="POOR"
                elif (( $(echo "$cv > 10" | bc -l) )); then
                    consistency="FAIR"
                elif (( $(echo "$cv > 5" | bc -l) )); then
                    consistency="GOOD"
                fi

                echo "  $metric:" >> "$OUTPUT_DIR/consistency_report.txt"
                echo "    Mean: $mean" >> "$OUTPUT_DIR/consistency_report.txt"
                echo "    Std Dev: $std_dev" >> "$OUTPUT_DIR/consistency_report.txt"
                echo "    CV: ${cv}%" >> "$OUTPUT_DIR/consistency_report.txt"
                echo "    Range: [$min_val - $max_val]" >> "$OUTPUT_DIR/consistency_report.txt"
                echo "    Consistency: $consistency" >> "$OUTPUT_DIR/consistency_report.txt"
                echo "    Raw values: ${values[*]}" >> "$OUTPUT_DIR/consistency_report.txt"

                # Print to console with color coding
                if [ "$consistency" == "EXCELLENT" ]; then
                    info "  $module/$metric: CV=${cv}% - ${GREEN}EXCELLENT${NC} consistency"
                elif [ "$consistency" == "GOOD" ]; then
                    info "  $module/$metric: CV=${cv}% - ${GREEN}GOOD${NC} consistency"
                elif [ "$consistency" == "FAIR" ]; then
                    warning "  $module/$metric: CV=${cv}% - ${YELLOW}FAIR${NC} consistency"
                else
                    error "  $module/$metric: CV=${cv}% - ${RED}POOR${NC} consistency"
                fi
            fi
        done
    done

    # Generate comparison between runs
    echo "" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "Run-to-Run Comparison:" >> "$OUTPUT_DIR/consistency_report.txt"
    echo "---------------------" >> "$OUTPUT_DIR/consistency_report.txt"

    # Compare first and last run
    if [ -f "$OUTPUT_DIR/run1.json" ] && [ -f "$OUTPUT_DIR/run${RUNS}.json" ]; then
        ./perf_test \
            --compare \
            --baseline="$OUTPUT_DIR/run1.json" \
            --current="$OUTPUT_DIR/run${RUNS}.json" \
            --warning=5 \
            --critical=10 >> "$OUTPUT_DIR/consistency_report.txt" 2>&1
    fi

    log "Consistency analysis complete. Report saved to $OUTPUT_DIR/consistency_report.txt"
}

# Generate summary
generate_summary() {
    echo ""
    echo "========================================"
    echo "         CONSISTENCY TEST SUMMARY       "
    echo "========================================"
    echo ""
    cat "$OUTPUT_DIR/consistency_report.txt" | grep -E "Consistency:|CV:" | while read line; do
        echo "  $line"
    done
    echo ""
    echo "Full report: $OUTPUT_DIR/consistency_report.txt"
    echo "Raw data: $OUTPUT_DIR/run*.json"
    echo ""

    # Determine overall consistency
    if grep -q "POOR" "$OUTPUT_DIR/consistency_report.txt"; then
        error "Overall consistency: POOR - Results vary significantly between runs"
        echo "Recommendations:"
        echo "  - Check for background processes"
        echo "  - Ensure thermal throttling is not occurring"
        echo "  - Increase test duration or iterations"
        echo "  - Review system monitoring logs in $OUTPUT_DIR/monitor_run*.txt"
    elif grep -q "FAIR" "$OUTPUT_DIR/consistency_report.txt"; then
        warning "Overall consistency: FAIR - Some variance detected"
        echo "Recommendations:"
        echo "  - Consider increasing iterations"
        echo "  - Review specific metrics with high variance"
    else
        log "Overall consistency: GOOD - Results are reliable"
    fi
}

# Main execution
main() {
    log "Starting consistency test with $RUNS runs"
    log "Configuration: modules=$MODULES, duration=${DURATION}s, iterations=$ITERATIONS"

    # Prepare system
    prepare_system

    # Run benchmarks
    failed_runs=0
    for ((i=1; i<=RUNS; i++)); do
        if ! run_benchmark $i; then
            ((failed_runs++))
            warning "Run $i failed, continuing with remaining runs..."
        fi
    done

    if [ $failed_runs -eq $RUNS ]; then
        error "All runs failed. Cannot perform consistency analysis."
        exit 1
    fi

    if [ $failed_runs -gt 0 ]; then
        warning "$failed_runs out of $RUNS runs failed"
    fi

    # Analyze results
    analyze_consistency

    # Generate summary
    generate_summary
}

# Trap to cleanup on exit
trap 'rm -f "$OUTPUT_DIR/.monitoring" 2>/dev/null' EXIT

# Run main function
main