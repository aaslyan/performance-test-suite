#!/bin/bash

# Performance Test Suite - Automated Testing Script
# This script runs 3 consecutive tests and compares results
# Usage: ./test.sh [duration] [modules]

set -e  # Exit on any error

# Configuration
DURATION=${1:-10}           # Test duration in seconds (default: 10)
MODULES=${2:-"cpu,mem"}     # Modules to test (default: cpu,mem for quick testing)
OUTPUT_DIR="test_results"   # Directory for test outputs
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors for output
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BLUE='\033[34m'
CYAN='\033[36m'
NC='\033[0m' # No Color

# Helper functions
print_header() {
    echo -e "\n${CYAN}=====================================\n$1\n=====================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# Check if perf_test binary exists
if [ ! -f "./perf_test" ]; then
    print_error "perf_test binary not found. Please run 'make' first."
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

print_header "Performance Test Suite - Automated Testing"
print_info "Test Configuration:"
echo "  Duration per test: ${DURATION} seconds"
echo "  Modules to test: ${MODULES}"
echo "  Output directory: ${OUTPUT_DIR}"
echo "  Timestamp: ${TIMESTAMP}"

# Test 1: Baseline
print_header "TEST 1: Creating Baseline"
BASELINE_FILE="${OUTPUT_DIR}/baseline_${TIMESTAMP}.json"

print_info "Running baseline test..."
./perf_test --modules="$MODULES" --duration="$DURATION" --format=json --report="$BASELINE_FILE" --verbose

if [ $? -eq 0 ]; then
    print_success "Baseline test completed: $BASELINE_FILE"
else
    print_error "Baseline test failed"
    exit 1
fi

# Wait a moment between tests
print_info "Waiting 5 seconds before next test..."
sleep 5

# Test 2: First Comparison
print_header "TEST 2: First Comparison Test"
CURRENT1_FILE="${OUTPUT_DIR}/current1_${TIMESTAMP}.json"

print_info "Running first comparison test..."
./perf_test --modules="$MODULES" --duration="$DURATION" --format=json --report="$CURRENT1_FILE" --verbose

if [ $? -eq 0 ]; then
    print_success "First comparison test completed: $CURRENT1_FILE"
else
    print_error "First comparison test failed"
    exit 1
fi

# Compare Test 1 vs Baseline
print_header "COMPARISON 1: Baseline vs Test 1"
print_info "Comparing baseline with first test..."
echo ""

./perf_test --compare --baseline="$BASELINE_FILE" --current="$CURRENT1_FILE" --compare-format=text
COMPARISON1_EXIT=$?

case $COMPARISON1_EXIT in
    0)
        print_success "Comparison 1: HEALTHY - Performance within acceptable thresholds"
        ;;
    1)
        print_warning "Comparison 1: WARNING - Some performance degradation detected"
        ;;
    2)
        print_error "Comparison 1: CRITICAL - Significant performance degradation detected"
        ;;
esac

# Wait a moment between tests
print_info "Waiting 5 seconds before next test..."
sleep 5

# Test 3: Second Comparison
print_header "TEST 3: Second Comparison Test"
CURRENT2_FILE="${OUTPUT_DIR}/current2_${TIMESTAMP}.json"

print_info "Running second comparison test..."
./perf_test --modules="$MODULES" --duration="$DURATION" --format=json --report="$CURRENT2_FILE" --verbose

if [ $? -eq 0 ]; then
    print_success "Second comparison test completed: $CURRENT2_FILE"
else
    print_error "Second comparison test failed"
    exit 1
fi

# Compare Test 2 vs Baseline
print_header "COMPARISON 2: Baseline vs Test 2"
print_info "Comparing baseline with second test..."
echo ""

./perf_test --compare --baseline="$BASELINE_FILE" --current="$CURRENT2_FILE" --compare-format=text
COMPARISON2_EXIT=$?

case $COMPARISON2_EXIT in
    0)
        print_success "Comparison 2: HEALTHY - Performance within acceptable thresholds"
        ;;
    1)
        print_warning "Comparison 2: WARNING - Some performance degradation detected"
        ;;
    2)
        print_error "Comparison 2: CRITICAL - Significant performance degradation detected"
        ;;
esac

# Compare Test 1 vs Test 2
print_header "COMPARISON 3: Test 1 vs Test 2"
print_info "Comparing first and second tests..."
echo ""

./perf_test --compare --baseline="$CURRENT1_FILE" --current="$CURRENT2_FILE" --compare-format=text
COMPARISON3_EXIT=$?

case $COMPARISON3_EXIT in
    0)
        print_success "Comparison 3: HEALTHY - Performance consistent between tests"
        ;;
    1)
        print_warning "Comparison 3: WARNING - Some variation between tests"
        ;;
    2)
        print_error "Comparison 3: CRITICAL - Significant variation between tests"
        ;;
esac

# Summary
print_header "TEST SUMMARY"
echo "Files generated:"
echo "  Baseline:     $BASELINE_FILE"
echo "  Test 1:       $CURRENT1_FILE"
echo "  Test 2:       $CURRENT2_FILE"
echo ""
echo "Comparison Results:"

case $COMPARISON1_EXIT in
    0) echo -e "  Baseline vs Test 1: ${GREEN}HEALTHY${NC}" ;;
    1) echo -e "  Baseline vs Test 1: ${YELLOW}WARNING${NC}" ;;
    2) echo -e "  Baseline vs Test 1: ${RED}CRITICAL${NC}" ;;
esac

case $COMPARISON2_EXIT in
    0) echo -e "  Baseline vs Test 2: ${GREEN}HEALTHY${NC}" ;;
    1) echo -e "  Baseline vs Test 2: ${YELLOW}WARNING${NC}" ;;
    2) echo -e "  Baseline vs Test 2: ${RED}CRITICAL${NC}" ;;
esac

case $COMPARISON3_EXIT in
    0) echo -e "  Test 1 vs Test 2:   ${GREEN}HEALTHY${NC}" ;;
    1) echo -e "  Test 1 vs Test 2:   ${YELLOW}WARNING${NC}" ;;
    2) echo -e "  Test 1 vs Test 2:   ${RED}CRITICAL${NC}" ;;
esac

echo ""

# Generate markdown report
MARKDOWN_REPORT="${OUTPUT_DIR}/summary_${TIMESTAMP}.md"
print_info "Generating markdown summary report: $MARKDOWN_REPORT"

cat > "$MARKDOWN_REPORT" << EOF
# Performance Test Summary

**Test Run:** $(date)  
**Duration:** ${DURATION} seconds per test  
**Modules:** ${MODULES}  

## Test Files
- **Baseline:** \`$(basename "$BASELINE_FILE")\`
- **Test 1:** \`$(basename "$CURRENT1_FILE")\`  
- **Test 2:** \`$(basename "$CURRENT2_FILE")\`

## Comparison Results

### Baseline vs Test 1
\`\`\`
$(./perf_test --compare --baseline="$BASELINE_FILE" --current="$CURRENT1_FILE" --compare-format=markdown 2>/dev/null | grep -A 50 "Benchmark Comparisons")
\`\`\`

### Baseline vs Test 2
\`\`\`
$(./perf_test --compare --baseline="$BASELINE_FILE" --current="$CURRENT2_FILE" --compare-format=markdown 2>/dev/null | grep -A 50 "Benchmark Comparisons")
\`\`\`

### Test 1 vs Test 2
\`\`\`
$(./perf_test --compare --baseline="$CURRENT1_FILE" --current="$CURRENT2_FILE" --compare-format=markdown 2>/dev/null | grep -A 50 "Benchmark Comparisons")
\`\`\`
EOF

print_success "Test sequence completed successfully!"
print_info "All results saved in: $OUTPUT_DIR"
print_info "Markdown report: $MARKDOWN_REPORT"

# Overall exit code based on worst comparison result
WORST_EXIT=$(( COMPARISON1_EXIT > COMPARISON2_EXIT ? COMPARISON1_EXIT : COMPARISON2_EXIT ))
WORST_EXIT=$(( WORST_EXIT > COMPARISON3_EXIT ? WORST_EXIT : COMPARISON3_EXIT ))

if [ $WORST_EXIT -eq 2 ]; then
    print_error "Overall result: CRITICAL - Review performance degradation"
    exit 2
elif [ $WORST_EXIT -eq 1 ]; then
    print_warning "Overall result: WARNING - Some performance concerns detected"
    exit 1
else
    print_success "Overall result: HEALTHY - All tests within acceptable thresholds"
    exit 0
fi