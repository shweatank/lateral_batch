#!/bin/bash
#
# test.sh - Comprehensive test suite for the smart_device driver
#
# This script automates all functional, stress, and validation tests
# for the smart_device driver. It executes each test case and collects
# results for validation and debugging.
#
# Usage: sudo ./test.sh [--verbose] [--report] [--stress]
# Note: Must be run with root privileges (sudo)
# Note: Driver must already be loaded (see load_driver.sh)
#
# Options:
#   --verbose   Enable verbose output for each test
#   --report    Generate a test report file
#   --stress    Run stress tests (default: quick tests only)
#
# Author: Linux Kernel Developer
# Date: May 2026

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
DEVICE_PATH="/dev/smart_device"
USER_APP="./smart_device/user_app"
TEST_REPORT="test_report.txt"
VERBOSE=0
GENERATE_REPORT=0
RUN_STRESS=0

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# ============================================================================
# Parse command line arguments
# ============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --verbose)
            VERBOSE=1
            shift
            ;;
        --report)
            GENERATE_REPORT=1
            shift
            ;;
        --stress)
            RUN_STRESS=1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# ============================================================================
# Helper functions
# ============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

report_line() {
    if [[ $GENERATE_REPORT -eq 1 ]]; then
        echo "$1" >> "$TEST_REPORT"
    fi
}

test_case() {
    ((TESTS_RUN++))
    local test_id=$1
    local description=$2
    log_info "[$test_id] $description"
    report_line "TEST: $test_id - $description"
}

# ============================================================================
# Prerequisite checks
# ============================================================================

echo ""
echo "=========================================="
echo "SMART_DEVICE COMPREHENSIVE TEST SUITE"
echo "=========================================="
echo ""

log_info "Performing prerequisite checks..."

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    log_fail "Must run as root (use: sudo $0)"
    exit 1
fi

# Check if device exists
if [[ ! -e "$DEVICE_PATH" ]]; then
    log_fail "Device $DEVICE_PATH not found"
    log_info "Load the driver first: sudo ./load_driver.sh"
    exit 1
fi

log_pass "Device node exists: $DEVICE_PATH"

# Check if user_app exists
if [[ ! -f "$USER_APP" ]]; then
    log_fail "User application not found: $USER_APP"
    log_info "Build the application: make -C smart_device user"
    exit 1
fi

log_pass "User application found: $USER_APP"

# Check if module is loaded
if ! lsmod | grep -q "smart_driver"; then
    log_fail "Driver module not loaded"
    exit 1
fi

log_pass "Driver module is loaded"

# Initialize report file
if [[ $GENERATE_REPORT -eq 1 ]]; then
    > "$TEST_REPORT"  # Clear the file
    report_line "=========================================="
    report_line "SMART_DEVICE TEST REPORT"
    report_line "Generated: $(date)"
    report_line "=========================================="
    report_line ""
fi

echo ""
log_info "All prerequisites passed. Starting test execution..."
echo ""

# ============================================================================
# FUNCTIONAL TESTS - Basic device operations
# ============================================================================

echo "=========================================="
echo "FUNCTIONAL TESTS"
echo "=========================================="
echo ""

# Test 1: Device Open/Close
test_case "TC-1.1" "Device open and close"
if $USER_APP --cmd reset &>/dev/null; then
    log_pass "Device open/close successful"
    report_line "  Status: PASS"
else
    log_fail "Device open/close failed"
    report_line "  Status: FAIL"
fi
echo ""

# Test 2: Write operation
test_case "TC-1.2" "Write data to device"
if output=$($USER_APP --cmd write "test_data_12345" 2>&1); then
    log_pass "Write operation successful"
    report_line "  Status: PASS"
else
    log_fail "Write operation failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# Test 3: Read operation
test_case "TC-1.3" "Read data from device"
if output=$($USER_APP --cmd read 256 2>&1); then
    log_pass "Read operation successful"
    report_line "  Status: PASS"
else
    log_fail "Read operation failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# Test 4: Reset device
test_case "TC-1.4" "Reset device to default state"
if $USER_APP --cmd reset &>/dev/null; then
    log_pass "Device reset successful"
    report_line "  Status: PASS"
else
    log_fail "Device reset failed"
    report_line "  Status: FAIL"
fi
echo ""

# Test 5: Clear buffer
test_case "TC-1.5" "Clear internal device buffer"
if $USER_APP --cmd clear &>/dev/null; then
    log_pass "Buffer clear successful"
    report_line "  Status: PASS"
else
    log_fail "Buffer clear failed"
    report_line "  Status: FAIL"
fi
echo ""

# ============================================================================
# IOCTL FUNCTIONAL TESTS
# ============================================================================

echo "=========================================="
echo "IOCTL COMMAND TESTS"
echo "=========================================="
echo ""

# Test 6: Get driver stats
test_case "TC-2.1" "Get driver statistics"
if output=$($USER_APP --cmd stats 2>&1); then
    if echo "$output" | grep -q "driver stats"; then
        log_pass "Get stats IOCTL successful"
        report_line "  Status: PASS"
    else
        log_fail "Stats output invalid"
        report_line "  Status: FAIL"
    fi
else
    log_fail "Get stats IOCTL failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# Test 7: Set device mode
test_case "TC-2.2" "Set device to DEBUG mode"
if $USER_APP --cmd set-mode 1 &>/dev/null; then
    log_pass "Set mode IOCTL successful"
    report_line "  Status: PASS"
else
    log_fail "Set mode IOCTL failed"
    report_line "  Status: FAIL"
fi
echo ""

# Test 8: Get device mode
test_case "TC-2.3" "Get current device mode"
if output=$($USER_APP --cmd get-mode 2>&1); then
    if echo "$output" | grep -q "mode"; then
        log_pass "Get mode IOCTL successful"
        report_line "  Status: PASS"
    else
        log_fail "Mode output invalid"
        report_line "  Status: FAIL"
    fi
else
    log_fail "Get mode IOCTL failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# Test 9: Enable logging
test_case "TC-2.4" "Enable kernel logging"
if $USER_APP --cmd log-on &>/dev/null; then
    log_pass "Enable logging IOCTL successful"
    report_line "  Status: PASS"
else
    log_fail "Enable logging IOCTL failed"
    report_line "  Status: FAIL"
fi
echo ""

# Test 10: Disable logging
test_case "TC-2.5" "Disable kernel logging"
if $USER_APP --cmd log-off &>/dev/null; then
    log_pass "Disable logging IOCTL successful"
    report_line "  Status: PASS"
else
    log_fail "Disable logging IOCTL failed"
    report_line "  Status: FAIL"
fi
echo ""

# Test 11: Get last command
test_case "TC-2.6" "Get last command from history"
if output=$($USER_APP --cmd last 2>&1); then
    if echo "$output" | grep -q "last cmd"; then
        log_pass "Get last command IOCTL successful"
        report_line "  Status: PASS"
    else
        log_fail "Last command output invalid"
        report_line "  Status: FAIL"
    fi
else
    log_fail "Get last command IOCTL failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# Test 12: Set timeout
test_case "TC-2.7" "Set command timeout"
if $USER_APP --cmd set-timeout 5000 &>/dev/null; then
    log_pass "Set timeout IOCTL successful"
    report_line "  Status: PASS"
else
    log_fail "Set timeout IOCTL failed"
    report_line "  Status: FAIL"
fi
echo ""

# Test 13: Get timeout
test_case "TC-2.8" "Get current timeout value"
if output=$($USER_APP --cmd get-timeout 2>&1); then
    if echo "$output" | grep -q "timeout"; then
        log_pass "Get timeout IOCTL successful"
        report_line "  Status: PASS"
    else
        log_fail "Timeout output invalid"
        report_line "  Status: FAIL"
    fi
else
    log_fail "Get timeout IOCTL failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# ============================================================================
# AUTOMATED MODE TEST
# ============================================================================

echo "=========================================="
echo "AUTOMATED MODE TEST"
echo "=========================================="
echo ""

test_case "TC-3.1" "Automated exercise of all operations"
if output=$($USER_APP --auto 2>&1); then
    if echo "$output" | grep -q "AUTO: PASS"; then
        log_pass "Automated test sequence successful"
        report_line "  Status: PASS"
    else
        log_fail "Automated test sequence failed"
        report_line "  Status: FAIL - Output:"
        report_line "  $output"
    fi
else
    log_fail "Automated test execution failed"
    report_line "  Status: FAIL"
fi
[[ $VERBOSE -eq 1 ]] && echo "Output: $output"
echo ""

# ============================================================================
# STRESS TEST (optional)
# ============================================================================

if [[ $RUN_STRESS -eq 1 ]]; then
    echo "=========================================="
    echo "STRESS TESTS"
    echo "=========================================="
    echo ""
    
    test_case "TC-4.1" "Stress test with 4 threads, 100 ops each"
    if output=$($USER_APP --stress 4 100 2>&1); then
        if echo "$output" | grep -q "STRESS: PASS"; then
            log_pass "Stress test successful"
            report_line "  Status: PASS"
        else
            log_fail "Stress test failed"
            report_line "  Status: FAIL - Output:"
            report_line "  $output"
        fi
    else
        log_fail "Stress test execution failed"
        report_line "  Status: FAIL"
    fi
    [[ $VERBOSE -eq 1 ]] && echo "Output: $output"
    echo ""
fi

# ============================================================================
# KERNEL LOG VALIDATION
# ============================================================================

echo "=========================================="
echo "KERNEL LOG VALIDATION"
echo "=========================================="
echo ""

test_case "TC-5.1" "Verify kernel logs for errors"
kernel_errors=$(dmesg | tail -50 | grep -i "error\|bug\|oops\|panic" | grep -v "Permission denied" || true)

if [[ -z "$kernel_errors" ]]; then
    log_pass "No kernel errors detected"
    report_line "  Status: PASS"
else
    log_warn "Potential kernel errors detected:"
    echo "$kernel_errors" | sed 's/^/    /'
    report_line "  Status: WARNING - Errors detected:"
    report_line "  $kernel_errors"
fi
echo ""

# ============================================================================
# TEST SUMMARY
# ============================================================================

echo "=========================================="
echo "TEST SUMMARY"
echo "=========================================="
echo ""

total_tests=$TESTS_RUN
pass_rate=$((TESTS_PASSED * 100 / total_tests))

echo "Total Tests Run:    $TESTS_RUN"
echo "Tests Passed:       $TESTS_PASSED"
echo "Tests Failed:       $TESTS_FAILED"
echo "Pass Rate:          $pass_rate%"
echo ""

if [[ $TESTS_FAILED -eq 0 ]]; then
    log_pass "ALL TESTS PASSED!"
    test_status="PASS"
else
    log_fail "$TESTS_FAILED TEST(S) FAILED"
    test_status="FAIL"
fi

# ============================================================================
# GENERATE FINAL REPORT
# ============================================================================

if [[ $GENERATE_REPORT -eq 1 ]]; then
    report_line ""
    report_line "=========================================="
    report_line "TEST SUMMARY"
    report_line "=========================================="
    report_line "Total Tests:       $TESTS_RUN"
    report_line "Passed:            $TESTS_PASSED"
    report_line "Failed:            $TESTS_FAILED"
    report_line "Pass Rate:         $pass_rate%"
    report_line "Final Status:      $test_status"
    report_line ""
    
    log_info "Test report generated: $TEST_REPORT"
fi

echo ""
echo "=========================================="
echo ""

if [[ $TESTS_FAILED -eq 0 ]]; then
    exit 0
else
    exit 1
fi
