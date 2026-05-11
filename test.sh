#!/usr/bin/env bash
# test.sh — end-to-end test orchestration.
#
# Phases:
#   1. Build everything
#   2. (Re)load the driver
#   3. Run user_app --auto    (positive, negative, boundary)
#   4. Run user_app --stress  (concurrency stress)
#   5. Scrape dmesg for OOPS / WARN / BUG
#   6. Unload, verify clean state
#
# Returns 0 on success, non-zero on any failure.

set -uo pipefail   # NOT -e: we want to capture failures and continue.

PASS=0
FAIL=0
record() {
    if [[ $1 -eq 0 ]]; then
        echo -e "  \e[32m[PASS]\e[0m $2";  PASS=$((PASS+1))
    else
        echo -e "  \e[31m[FAIL]\e[0m $2";  FAIL=$((FAIL+1))
    fi
}

banner() { echo -e "\n\e[36m==== $* ====\e[0m"; }

banner "PHASE 1: build"
make >/dev/null 2>&1; record $? "make"

banner "PHASE 2: load driver"
./unload_driver.sh 2>&1 | sed 's/^/    /' || true
./load_driver.sh   2>&1 | sed 's/^/    /'
record ${PIPESTATUS[0]} "load_driver.sh"

banner "PHASE 3: automated functional tests"
sudo ./smart_app --auto
record $? "smart_app --auto"

banner "PHASE 4: stress test (8 threads × 200 iters)"
sudo ./smart_app --stress 200
record $? "smart_app --stress 200"

banner "PHASE 5: dmesg for OOPS/WARN/BUG"
if sudo dmesg | tail -n 200 | grep -E "Oops|WARNING|BUG:"; then
    record 1 "dmesg has Oops/WARN/BUG"
else
    record 0 "dmesg is clean"
fi

banner "PHASE 6: unload"
./unload_driver.sh >/dev/null 2>&1; record $? "unload_driver.sh"

banner "RESULTS"
echo "  PASS: $PASS"
echo "  FAIL: $FAIL"
[[ $FAIL -eq 0 ]]
