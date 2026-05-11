#!/usr/bin/env bash
# load_driver.sh — build (if needed), insmod, and verify.
#
# Steps:
#   1. Ensure smart_driver.ko exists, otherwise run `make`.
#   2. insmod the module.
#   3. Verify with lsmod, /dev/<name>, /proc/devices, dmesg.
#
# Run as a normal user; sudo is invoked only where it's needed.

set -euo pipefail

MODULE=smart_driver
KO=${MODULE}.ko
DEV=/dev/smart_device

echo "==> [1/4] checking build"
if [[ ! -f "$KO" ]]; then
    echo "    $KO not found — running make"
    make
fi

if lsmod | grep -q "^${MODULE}\b"; then
    echo "==> module already loaded"
else
    echo "==> [2/4] insmod $KO"
    sudo insmod "$KO"
fi

echo "==> [3/4] verifying"
lsmod | grep -E "^${MODULE}\b" >/dev/null \
    || { echo "ERR: not in lsmod"; exit 1; }

# udev creates the device node asynchronously after insmod — wait up to
# 3 seconds for it to appear.
for _ in 1 2 3 4 5 6; do
    [[ -c "$DEV" ]] && break
    sleep 0.5
done
[[ -c "$DEV" ]] || { echo "ERR: $DEV missing after wait"; exit 1; }

ls -l "$DEV"
grep smart_device /proc/devices || true

echo "==> [4/4] recent dmesg lines"
sudo dmesg | tail -n 10

echo "OK — driver loaded and ready"
