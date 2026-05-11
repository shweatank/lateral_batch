#!/bin/bash
# Basic test cases for calc_driver. Run as root after `make`.

set -u
MOD=calc_driver.ko
APP=./calc_user
PASS=0
FAIL=0

check() {
	local desc="$1" expected="$2" got="$3"
	if [ "$expected" = "$got" ]; then
		echo "PASS: $desc  (got: $got)"
		PASS=$((PASS+1))
	else
		echo "FAIL: $desc  (expected: $expected, got: $got)"
		FAIL=$((FAIL+1))
	fi
}

if [ "$(id -u)" -ne 0 ]; then
	echo "Run as root (needs insmod / /dev access)."
	exit 1
fi

echo "== Loading module =="
lsmod | grep -q '^calc_driver' && rmmod calc_driver
insmod "$MOD" || { echo "insmod failed"; exit 1; }
sleep 1
[ -e /dev/calc_dev ] || { echo "/dev/calc_dev missing"; exit 1; }
chmod 666 /dev/calc_dev

echo "== Functional tests =="
check "add positives"     "5 + 3 = 8"     "$($APP 5 3 add)"
check "add with zero"     "0 + 7 = 7"     "$($APP 0 7 add)"
check "add negatives"     "-4 + -6 = -10" "$($APP -4 -6 add)"
check "sub positives"     "10 - 4 = 6"    "$($APP 10 4 sub)"
check "sub to negative"   "3 - 9 = -6"    "$($APP 3 9 sub)"
check "sub equal"         "8 - 8 = 0"     "$($APP 8 8 sub)"

echo "== Negative tests =="
if $APP 1 2 mul 2>/dev/null; then
	echo "FAIL: invalid op should error"; FAIL=$((FAIL+1))
else
	echo "PASS: invalid op rejected"; PASS=$((PASS+1))
fi

if $APP 1 2 2>/dev/null; then
	echo "FAIL: missing arg should error"; FAIL=$((FAIL+1))
else
	echo "PASS: missing arg rejected"; PASS=$((PASS+1))
fi

echo "== Unloading module =="
rmmod calc_driver

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
