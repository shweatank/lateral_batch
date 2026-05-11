#!/usr/bin/env bash
# unload_driver.sh — rmmod + verify clean state.

set -euo pipefail

MODULE=smart_driver
DEV=/dev/smart_device

if ! lsmod | grep -q "^${MODULE}\b"; then
    echo "==> $MODULE not loaded — nothing to do"
    exit 0
fi

echo "==> rmmod $MODULE"
sudo rmmod "$MODULE"

# After unload these resources should disappear:
echo "==> verifying cleanup"
if lsmod | grep -q "^${MODULE}\b"; then
    echo "ERR: module still loaded"; exit 1
fi
if [[ -e "$DEV" ]]; then
    echo "ERR: $DEV still present"; exit 1
fi
if grep -q smart_device /proc/devices; then
    echo "ERR: still in /proc/devices"; exit 1
fi

echo "==> recent dmesg lines"
sudo dmesg | tail -n 5

echo "OK — driver unloaded cleanly"
