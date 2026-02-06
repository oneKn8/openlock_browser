#!/bin/bash
# OpenLock Lockdown Verification Script
# Tests that lockdown measures are working correctly

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}[PASS]${NC} $1"; }
fail() { echo -e "${RED}[FAIL]${NC} $1"; }
skip() { echo -e "${YELLOW}[SKIP]${NC} $1"; }

echo "OpenLock Lockdown Verification"
echo "=============================="
echo ""

# Check if OpenLock is running
if ! pgrep -x openlock > /dev/null 2>&1; then
    echo "OpenLock is not running. Start it first."
    exit 1
fi

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

run_test() {
    local name="$1"
    local cmd="$2"
    TESTS_RUN=$((TESTS_RUN + 1))

    if eval "$cmd" > /dev/null 2>&1; then
        fail "$name - should have been blocked"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    else
        pass "$name - correctly blocked"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    fi
}

# Test: Cannot open a terminal
run_test "Terminal launch blocked" "timeout 2 gnome-terminal 2>/dev/null"
run_test "Terminal launch blocked (xterm)" "timeout 2 xterm 2>/dev/null"

# Test: Cannot take screenshots
run_test "Screenshot blocked (scrot)" "timeout 2 scrot /tmp/test_screenshot.png 2>/dev/null"
run_test "Screenshot blocked (maim)" "timeout 2 maim /tmp/test_screenshot.png 2>/dev/null"

# Test: Cannot access clipboard
run_test "Clipboard blocked (xclip)" "echo test | timeout 2 xclip -selection clipboard 2>/dev/null && xclip -selection clipboard -o 2>/dev/null | grep test"

# Test: Cannot launch browser
run_test "Browser launch blocked (firefox)" "timeout 2 firefox 2>/dev/null"

echo ""
echo "=============================="
echo "Results: $TESTS_PASSED/$TESTS_RUN passed, $TESTS_FAILED failed"
