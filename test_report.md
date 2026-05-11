# Test Report — smart_device

> Project: smart_device character driver
> Target : Linux 6.x, x86_64 / arm64
> Date   : __________
> Tester : __________

---

## 1. Test environment

| Field           | Value                          |
|-----------------|--------------------------------|
| Kernel version  | `uname -r` output              |
| Distro          | Ubuntu __ / Raspberry Pi OS __ |
| Architecture    | `uname -m`                     |
| GCC version     | `gcc --version` first line     |
| Hardware        | __________                     |

---

## 2. Test methodology

1. **Positive tests** — each ioctl invoked with valid input; verify
   it returns 0 and the side-effect is observable.
2. **Negative tests** — invalid magic, invalid NR, NULL pointers,
   out-of-range mode and timeout values.
3. **Boundary tests** — empty writes, writes longer than the buffer,
   reads after EOF, reads after CLEAR.
4. **Stress / concurrency** — 8 threads × N iterations, mixed
   operations. Watched with `dmesg -w` for OOPS/WARN/BUG.
5. **Lifecycle** — load / unload cycles repeated; verify no
   resource leaks, no orphan `/dev` nodes.

`./test.sh` automates phases 1–4 and the final lifecycle check.

---

## 3. Test cases

> Format reused for every case:
> **ID · objective · steps · expected · actual · status**.

---

### TC-01 — module load
- **Objective** : driver inserts cleanly and creates `/dev/smart_device`.
- **Steps**     : `./load_driver.sh`
- **Expected**  : `lsmod` shows `smart_driver`; `/dev/smart_device` is
  char-special; dmesg ends with `/dev/smart_device ready`.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-02 — open / close
- **Objective** : repeated open() / close() leaves the device usable.
- **Steps**     : run `--auto` 5×.
- **Expected**  : no error; open_count == close_count.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-03 — write then read round-trip
- **Objective** : data written via `write()` is returned by `read()`.
- **Steps**     : write 30 bytes, lseek 0, read 128 bytes.
- **Expected**  : read returns 30 bytes equal to the written buffer.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-04 — write longer than buffer (boundary)
- **Objective** : driver truncates instead of corrupting memory.
- **Steps**     : write 8192 bytes (buffer is 4096).
- **Expected**  : `write()` returns 4096; subsequent read returns 4096.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-05 — RESET_DEVICE
- **Objective** : ioctl clears buffer, mode, timeout and stats.
- **Steps**     : write data → RESET → GET_STATS → GET_MODE.
- **Expected**  : stats all zero, mode == NORMAL, timeout == 1000.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-06 — GET_STATS / GET_LAST_CMD
- **Objective** : counters increment monotonically; last_cmd is
  remembered.
- **Steps**     : write × 3, read × 2, ioctl × 4 → GET_STATS.
- **Expected**  : write_count == 3, read_count == 2,
  ioctl_count == 5 (4 + GET_STATS itself).
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-07 — CLEAR_BUFFER
- **Objective** : after CLEAR, read() returns 0 (EOF).
- **Steps**     : write data → CLEAR → read.
- **Expected**  : read == 0.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-08 — SET_MODE / GET_MODE round-trip
- **Objective** : every defined mode is settable and observable.
- **Steps**     : for m in 0..3: SET_MODE(m), GET_MODE.
- **Expected**  : GET_MODE returns the value just set.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-09 — SET_MODE rejects invalid (negative test)
- **Objective** : out-of-range mode returns -EINVAL.
- **Steps**     : SET_MODE(999), SET_MODE(-1).
- **Expected**  : both return -1, errno == EINVAL.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-10 — SET_TIMEOUT / GET_TIMEOUT
- **Objective** : timeout round-trip & range check (1…60000).
- **Steps**     : 1, 1000, 60000 — all accept. 0 and 60001 — reject.
- **Expected**  : valid values returned; invalid → EINVAL.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-11 — ENABLE_LOGGING / DISABLE_LOGGING
- **Objective** : runtime log silencing works.
- **Steps**     : DISABLE → write → check dmesg has no
  `smart_device:` line for the write.
- **Expected**  : dmesg quiet for that operation; ENABLE restores it.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-12 — invalid ioctl number
- **Objective** : unknown ioctl returns -ENOTTY.
- **Steps**     : `ioctl(fd, _IO('Z', 99))`.
- **Expected**  : return -1, errno == ENOTTY (25).
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-13 — ioctl with bad userspace pointer
- **Objective** : copy_to_user faulting returns -EFAULT, no crash.
- **Steps**     : `ioctl(fd, SMART_IOCTL_GET_STATS, (void*)1)`.
- **Expected**  : return -1, errno == EFAULT; error_count++.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-14 — repeated open / close stability
- **Objective** : 1000 open/close cycles, no leak, no crash.
- **Steps**     : shell loop `for i in $(seq 1000); do ...`.
- **Expected**  : open_count and close_count both == 1000.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-15 — concurrency stress
- **Objective** : 8 threads × 200 mixed ops produce no oops.
- **Steps**     : `sudo ./smart_app --stress 200`.
- **Expected**  : exit 0; dmesg free of OOPS / WARN / BUG.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-16 — large data write (boundary)
- **Objective** : `write(fd, buf, 1 MB)` is truncated to 4 KiB safely.
- **Steps**     : `dd if=/dev/urandom of=/dev/smart_device bs=1M count=1`.
- **Expected**  : `dd` reports 4096 bytes copied or short-write; no panic.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-17 — read past EOF
- **Objective** : read at offset > data_len returns 0.
- **Steps**     : write 10 bytes, lseek 100, read 64.
- **Expected**  : read == 0.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-18 — unload cleanup
- **Objective** : after rmmod, all resources gone.
- **Steps**     : `./unload_driver.sh`.
- **Expected**  : `lsmod` empty for our module; `/dev/smart_device` gone;
  `/proc/devices` no longer lists `smart_device`.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-19 — load/unload cycle ×10
- **Objective** : repeated insert/remove leaves no leaks.
- **Steps**     : `for i in $(seq 10); do ./load_driver.sh; ./unload_driver.sh; done`.
- **Expected**  : all succeed; `dmesg | grep -i leak` empty.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

### TC-20 — automated test exit code
- **Objective** : `--auto` returns 0 on success, non-zero otherwise.
- **Steps**     : `sudo ./smart_app --auto; echo $?`.
- **Expected**  : exit code == 0.
- **Actual**    : __________
- **Status**    : [ ] PASS  [ ] FAIL

---

## 4. Result summary

| Total | PASS | FAIL | Blocked |
|-------|------|------|---------|
|  20   |   _  |   _  |    _    |

---

## 5. Defects found

| ID | Severity | Description | Status |
|----|----------|-------------|--------|
|    |          |             |        |
|    |          |             |        |

---

## 6. Sign-off

- **Tester**:    __________  Date: ______
- **Reviewer**:  __________  Date: ______
