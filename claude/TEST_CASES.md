# SMART_DEVICE DRIVER - COMPREHENSIVE TEST CASES

**Document Version:** 1.0  
**Date:** May 2026  
**Platform:** Linux Kernel 6.x (ARM64/x86_64)  
**Author:** Linux Kernel Development Team

---

## Table of Contents

1. [Test Overview](#test-overview)
2. [Test Environment Setup](#test-environment-setup)
3. [Functional Test Cases](#functional-test-cases)
4. [IOCTL Command Test Cases](#ioctl-command-test-cases)
5. [Automated Mode Test Cases](#automated-mode-test-cases)
6. [Stress Test Cases](#stress-test-cases)
7. [Boundary and Edge Case Tests](#boundary-and-edge-case-tests)
8. [Negative Test Cases](#negative-test-cases)
9. [Concurrency Test Cases](#concurrency-test-cases)
10. [Debugging and Instrumentation](#debugging-and-instrumentation)

---

## Test Overview

This document defines comprehensive test cases for the smart_device Linux character device driver. The test suite covers:

- **Functional Tests:** Basic device operations (open, close, read, write)
- **IOCTL Tests:** All 10 IOCTL commands with various parameters
- **Automated Tests:** Scripted exercises of all operations
- **Stress Tests:** Multi-threaded concurrent access
- **Edge Cases:** Boundary conditions and error scenarios
- **Negative Tests:** Invalid inputs and error conditions
- **Concurrency Tests:** Race condition validation
- **Kernel Log Analysis:** Verification of kernel driver behavior

### Test Execution Procedures

All tests should be executed in the following order:

1. Load the driver: `sudo ./load_driver.sh`
2. Run functional tests: `sudo ./test.sh`
3. Run extended tests: `sudo ./test.sh --verbose --report --stress`
4. Unload the driver: `sudo ./unload_driver.sh`

---

## Test Environment Setup

### Prerequisites

```bash
# Verify kernel version
uname -r                    # Should be 6.x

# Check for required tools
which gcc                   # C compiler
which make                  # Build system
which insmod rmmod          # Kernel module tools
which dmesg                 # Kernel logging

# Verify permissions
sudo -l                     # Must have sudo access
```

### Driver Compilation

```bash
cd smart_device
make clean
make                        # Compile both kernel module and user app
```

### Module Loading

```bash
sudo ./load_driver.sh       # Load the driver with automatic verification

# Verify successful load
lsmod | grep smart_driver   # Should show module is loaded
ls -la /dev/smart_device    # Device node should exist
```

---

## Functional Test Cases

### TC-1.1: Device Open and Close

**Test ID:** TC-1.1  
**Category:** Functional / Device Management  
**Priority:** Critical  
**Objective:** Verify that the device can be opened and closed successfully

**Prerequisites:**
- Driver module is loaded
- Device node exists at /dev/smart_device
- Device node has read/write permissions

**Test Steps:**
1. Open /dev/smart_device in read-write mode using `open()`
2. Verify the file descriptor is valid (>= 0)
3. Close the device using `close()`
4. Verify close returns success (0)

**Expected Result:**
- open() call succeeds and returns valid file descriptor
- close() call succeeds without errors
- Kernel log shows: "smart_device: open() pid=XXXX" and "smart_device: release() pid=XXXX"
- Device statistics should show opens=1, closes=1

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Multiple open/close cycles should be supported
- Device should support concurrent opens from different processes
- Permissions should allow any user with read/write access

---

### TC-1.2: Write Data to Device

**Test ID:** TC-1.2  
**Category:** Functional / Data Operations  
**Priority:** Critical  
**Objective:** Verify that data can be written to the device buffer

**Prerequisites:**
- Device is open and valid
- Device buffer is empty

**Test Steps:**
1. Write test data "Hello smart_device" to the device
2. Verify write() returns the number of bytes written (19 bytes)
3. Check kernel logs for write confirmation
4. Verify internal buffer contains the written data

**Expected Result:**
- write() returns 19 (number of bytes written)
- Kernel log shows: "smart_device: write() -> 19"
- No errors are generated
- Internal buffer is updated with new data

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Device should maintain internal buffer of SMART_BUF_SIZE (4096 bytes)
- Data should be copied using copy_from_user()
- Partial writes should be handled correctly

---

### TC-1.3: Read Data from Device

**Test ID:** TC-1.3  
**Category:** Functional / Data Operations  
**Priority:** Critical  
**Objective:** Verify that previously written data can be read back

**Prerequisites:**
- Device is open and valid
- Data has been written to device (TC-1.2)

**Test Steps:**
1. Read data from the device using read()
2. Specify read buffer size of 256 bytes
3. Verify read() returns the number of bytes written previously (19 bytes)
4. Verify the data returned matches the written data
5. Check kernel logs for read confirmation

**Expected Result:**
- read() returns 19 bytes (the amount previously written)
- Data matches: "Hello smart_device"
- Kernel log shows: "smart_device: read() -> 19"
- File position is updated correctly
- Subsequent reads return EOF (0 bytes) until repositioned

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- read() should use copy_to_user() for safety
- File position should be maintained across multiple reads
- lseek() should allow repositioning to read from beginning

---

### TC-1.4: Device Reset

**Test ID:** TC-1.4  
**Category:** Functional / Device Control  
**Priority:** High  
**Objective:** Verify that device reset clears buffers and restores defaults

**Prerequisites:**
- Device has been written to (TC-1.2)
- Device has active logging and custom mode
- Device has modified statistics

**Test Steps:**
1. Write data to device and perform various operations
2. Invoke SMART_RESET_DEVICE IOCTL
3. Verify internal buffer is cleared
4. Verify device mode is reset to SMART_MODE_NORMAL
5. Verify logging is re-enabled
6. Verify timeout is reset to default (1000 ms)
7. Verify statistics are cleared (except ioctls counter)

**Expected Result:**
- IOCTL returns success (0)
- Kernel log shows reset confirmation
- Buffer contains all zeros
- Mode is SMART_MODE_NORMAL
- Timeout is 1000 ms
- Statistics show only 1 ioctl call
- Subsequent read returns 0 bytes (EOF)

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Reset should be atomic (protected by mutex)
- Command history should be cleared
- All counters should be reset except ioctls
- Device should be usable immediately after reset

---

### TC-1.5: Clear Buffer

**Test ID:** TC-1.5  
**Category:** Functional / Device Control  
**Priority:** High  
**Objective:** Verify that buffer can be cleared without affecting device state

**Prerequisites:**
- Device has data written to it
- Device is configured with custom parameters

**Test Steps:**
1. Write test data "TEST_DATA_12345" to device
2. Invoke SMART_CLEAR_BUFFER IOCTL
3. Verify buffer is cleared
4. Attempt to read from device
5. Verify device mode and timeout are unchanged
6. Verify statistics are NOT cleared (only buffer)

**Expected Result:**
- IOCTL returns success (0)
- Buffer is cleared (read returns 0 bytes)
- Device mode unchanged
- Timeout setting unchanged
- Statistics counters unchanged
- Kernel log shows: "smart_device: ioctl nr=3 ret=0"

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- CLEAR_BUFFER should only affect buffer content, not statistics or configuration
- Should be idempotent (multiple clears have same effect as one)
- Should not modify file position

---

## IOCTL Command Test Cases

### TC-2.1: Get Driver Statistics

**Test ID:** TC-2.1  
**Category:** IOCTL / Statistics  
**Priority:** High  
**Objective:** Verify that driver statistics are returned correctly

**Prerequisites:**
- Device has been used for various operations
- Driver maintains statistics for opens, reads, writes, ioctls, bytes, errors

**Test Steps:**
1. Perform 5 write operations with varying sizes
2. Perform 3 read operations
3. Invoke SMART_GET_DRIVER_STATS IOCTL
4. Verify returned structure contains expected statistics
5. Verify all fields are non-negative

**Expected Result:**
- IOCTL returns success (0)
- Returned struct smart_stats contains:
  - opens >= 1
  - closes >= 0
  - reads >= 3
  - writes >= 5
  - ioctls >= 1 (at least this call)
  - bytes_read > 0
  - bytes_written > 0
  - errors == 0 (no errors occurred)
- Snapshot is consistent (all values are non-decreasing)

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IOR macro (kernel writes to user buffer)
- Snapshot must be taken under mutex lock for consistency
- All __u64 fields should be properly formatted
- Should work correctly on both 32-bit and 64-bit systems

---

### TC-2.2: Set Device Mode

**Test ID:** TC-2.2  
**Category:** IOCTL / Configuration  
**Priority:** High  
**Objective:** Verify that device mode can be changed to all valid modes

**Prerequisites:**
- Device is open and valid

**Test Variations:**
1. Set mode to SMART_MODE_NORMAL (0)
2. Set mode to SMART_MODE_DEBUG (1)
3. Set mode to SMART_MODE_TEST (2)
4. Set mode to SMART_MODE_SILENT (3)

**Test Steps:**
1. For each valid mode value:
   a. Invoke SMART_SET_DEVICE_MODE IOCTL with mode value
   b. Verify IOCTL returns success (0)
   c. Invoke SMART_GET_DEVICE_MODE to verify the change
   d. Verify returned mode matches set mode

**Expected Result:**
- All valid mode values (0-3) are accepted
- IOCTL returns success for each valid mode
- Mode change is immediately reflected in subsequent GET_DEVICE_MODE call
- Kernel logs show mode change in debug mode
- Device behavior changes according to mode

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IOW macro (user writes to kernel)
- Invalid mode values should be rejected with -EINVAL
- Mode persistence should survive multiple operations

---

### TC-2.3: Get Device Mode

**Test ID:** TC-2.3  
**Category:** IOCTL / Configuration  
**Priority:** High  
**Objective:** Verify that current device mode can be queried

**Prerequisites:**
- Device mode has been set (TC-2.2)

**Test Steps:**
1. Invoke SMART_GET_DEVICE_MODE IOCTL
2. Verify IOCTL returns success (0)
3. Verify returned mode value matches previously set mode
4. Verify returned mode is a valid enum value (0-3)

**Expected Result:**
- IOCTL returns success (0)
- Returned mode matches current device mode
- Value is one of: SMART_MODE_NORMAL, DEBUG, TEST, or SILENT
- Multiple successive calls return same value

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IOR macro (kernel reads, user receives)
- Default mode should be SMART_MODE_NORMAL after reset
- Should return current mode without side effects

---

### TC-2.4: Enable Logging

**Test ID:** TC-2.4  
**Category:** IOCTL / Logging Control  
**Priority:** Medium  
**Objective:** Verify that kernel logging can be enabled

**Prerequisites:**
- Device is open

**Test Steps:**
1. Invoke SMART_DISABLE_LOGGING to ensure logging is off
2. Invoke SMART_ENABLE_LOGGING IOCTL
3. Perform a device operation (write)
4. Verify kernel logs contain the operation message
5. Check that logging flag is set

**Expected Result:**
- IOCTL returns success (0)
- Subsequent kernel messages appear in dmesg
- Kernel log shows operation messages when logging is enabled
- Logging can be toggled on and off

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IO macro (no data transfer)
- Should persist across multiple operations
- Should work in all device modes

---

### TC-2.5: Disable Logging

**Test ID:** TC-2.5  
**Category:** IOCTL / Logging Control  
**Priority:** Medium  
**Objective:** Verify that kernel logging can be disabled

**Prerequisites:**
- Logging is currently enabled (TC-2.4)

**Test Steps:**
1. Invoke SMART_DISABLE_LOGGING IOCTL
2. Perform multiple device operations
3. Check that kernel logs do NOT contain new operation messages
4. Verify previous logs still exist

**Expected Result:**
- IOCTL returns success (0)
- Subsequent kernel messages are NOT added to dmesg
- Existing log entries are preserved
- Logging can be re-enabled with ENABLE_LOGGING

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IO macro (no data transfer)
- Should silence all debug output without affecting device operation
- SILENT mode should also suppress output even if logging is enabled

---

### TC-2.6: Get Last Command

**Test ID:** TC-2.6  
**Category:** IOCTL / History  
**Priority:** Medium  
**Objective:** Verify that command history can be retrieved

**Prerequisites:**
- At least one IOCTL has been executed

**Test Steps:**
1. Execute any IOCTL command (e.g., SMART_GET_DRIVER_STATS)
2. Immediately invoke SMART_GET_LAST_COMMAND IOCTL
3. Verify returned structure contains:
   - cmd_nr: command number from previous command
   - result: return value (0 for success)
   - timestamp_ns: nanosecond timestamp

**Expected Result:**
- IOCTL returns success (0)
- Returned struct contains valid command history entry
- cmd_nr matches the previously executed command
- result is 0 (success)
- timestamp_ns is a non-zero value
- Multiple successive calls show different commands

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IOR macro (kernel writes to user)
- History is stored in a 16-entry ring buffer
- Timestamps use CLOCK_REALTIME nanoseconds
- Returns -ENODATA if no commands in history

---

### TC-2.7: Set Timeout

**Test ID:** TC-2.7  
**Category:** IOCTL / Configuration  
**Priority:** Medium  
**Objective:** Verify that command timeout can be configured

**Prerequisites:**
- Device is open

**Test Variations:**
1. Set timeout to 1000 ms (minimum)
2. Set timeout to 5000 ms
3. Set timeout to 60000 ms (maximum)

**Test Steps:**
1. For each valid timeout value:
   a. Invoke SMART_SET_TIMEOUT IOCTL with timeout value
   b. Verify IOCTL returns success (0)
   c. Invoke SMART_GET_TIMEOUT to verify the change
   d. Verify returned timeout matches set value

**Expected Result:**
- All valid timeout values (1-60000 ms) are accepted
- IOCTL returns success for each valid timeout
- Change is immediately reflected in GET_TIMEOUT
- Invalid values (0, > 60000) are rejected with -EINVAL
- Timeout persists across multiple operations

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IOW macro (user writes to kernel)
- Valid range: 1 ms to 60000 ms
- Zero and values over 60000 should return -EINVAL
- Used for command execution timeout (future feature)

---

### TC-2.8: Get Timeout

**Test ID:** TC-2.8  
**Category:** IOCTL / Configuration  
**Priority:** Medium  
**Objective:** Verify that current timeout can be queried

**Prerequisites:**
- Timeout has been configured (TC-2.7)

**Test Steps:**
1. Invoke SMART_GET_TIMEOUT IOCTL
2. Verify IOCTL returns success (0)
3. Verify returned timeout value matches previously set timeout
4. Verify returned value is in valid range (1-60000)

**Expected Result:**
- IOCTL returns success (0)
- Returned timeout matches current device timeout
- Value is within valid range
- Default value after reset is 1000 ms
- Multiple successive calls return same value

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses _IOR macro (kernel reads, user receives)
- Should return current timeout without side effects
- Default should be SMART_DEFAULT_TIMEOUT (1000 ms)

---

## Automated Mode Test Cases

### TC-3.1: Automated Exercise of All Operations

**Test ID:** TC-3.1  
**Category:** Automated / Integration  
**Priority:** High  
**Objective:** Verify all operations execute successfully in sequence

**Prerequisites:**
- Driver is loaded
- user_app compiled with --auto mode support

**Test Steps:**
1. Execute: `./user_app --auto`
2. Program automatically performs:
   a. Device reset
   b. Enable logging
   c. Set mode to NORMAL
   d. Get mode
   e. Set timeout to 2500 ms
   f. Get timeout
   g. Write "hello smart_device" (19 bytes)
   h. Read back data (should get 19 bytes)
   i. Clear buffer
   f. Read again (should get 0 bytes - EOF)
   g. Get last command
   h. Get driver statistics
   i. Disable logging
3. Verify program exits with status 0

**Expected Result:**
- All operations complete without errors
- Program output contains "AUTO: PASS"
- Program exit code is 0
- Kernel logs show all operations
- Statistics show all operations were recorded

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- This is an integration test covering all major functionality
- Perfect for automated CI/CD pipelines
- Should complete in under 5 seconds
- Useful for smoke testing after driver load

---

## Stress Test Cases

### TC-4.1: Multi-threaded Stress Test

**Test ID:** TC-4.1  
**Category:** Stress / Concurrency  
**Priority:** High  
**Objective:** Verify driver handles concurrent access correctly

**Prerequisites:**
- Driver is loaded
- user_app compiled with --stress mode support

**Test Configurations:**
1. 4 threads, 100 operations each
2. 8 threads, 500 operations each
3. 16 threads, 1000 operations each

**Test Steps:**
1. Execute: `./user_app --stress <threads> <ops>`
2. Program spawns N pthreads
3. Each thread performs M mixed operations:
   - Write operations (random data)
   - Read operations
   - IOCTL commands (get stats, set/get mode, set/get timeout)
4. Program measures execution time and operation rate
5. Verify all threads complete successfully
6. Verify no mutex deadlocks occur
7. Check for any kernel warnings or errors

**Expected Result (Config 1: 4 threads, 100 ops):**
- Program executes in < 2 seconds
- Completes 400 total operations
- Output shows "STRESS: PASS" with 0 errors
- Operation rate: > 150 ops/sec
- No kernel warnings in dmesg
- All threads complete

**Expected Result (Config 2: 8 threads, 500 ops):**
- Program executes in < 10 seconds
- Completes 4000 total operations
- Output shows "STRESS: PASS" with 0 errors
- No deadlocks or race conditions
- Mutex serialization working correctly

**Expected Result (Config 3: 16 threads, 1000 ops):**
- Program executes in < 30 seconds
- Completes 16000 total operations
- Output shows "STRESS: PASS" with 0 errors
- Demonstrates scalability

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Tests mutex lock implementation
- Validates race condition protection
- Should show zero errors in all configurations
- Memory should remain stable (no leaks)
- All counters should be consistent

---

## Boundary and Edge Case Tests

### TC-5.1: Large Data Write

**Test ID:** TC-5.1  
**Category:** Boundary / Data Operations  
**Priority:** High  
**Objective:** Verify behavior when writing data larger than buffer size

**Prerequisites:**
- Device buffer size is 4096 bytes
- Device is open

**Test Steps:**
1. Create test data of 8192 bytes (2x buffer size)
2. Write all 8192 bytes to device
3. Verify return value
4. Attempt to read all data back
5. Verify data integrity of returned portion

**Expected Result:**
- write() returns 4096 (maximum buffer size)
- Remaining 4096 bytes are silently discarded
- Subsequent read() returns 4096 bytes
- Data returned matches first 4096 bytes written

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Driver should silently truncate writes larger than buffer
- No error should be returned (consistent with kernel driver design)
- All 4096 bytes in buffer should be valid

---

### TC-5.2: Zero-byte Write

**Test ID:** TC-5.2  
**Category:** Boundary / Data Operations  
**Priority:** Medium  
**Objective:** Verify handling of zero-byte writes

**Prerequisites:**
- Device is open

**Test Steps:**
1. Invoke write() with count=0
2. Verify return value
3. Verify no buffer modification
4. Verify device state unchanged

**Expected Result:**
- write() returns 0
- No error is generated
- Device buffer remains unchanged
- Statistics.writes counter does not increment

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Should be handled as no-op
- Consistent with standard kernel driver behavior
- No side effects should occur

---

### TC-5.3: Read Before Write

**Test ID:** TC-5.3  
**Category:** Boundary / Data Operations  
**Priority:** Medium  
**Objective:** Verify read behavior when buffer is empty

**Prerequisites:**
- Device is open
- Buffer is empty (freshly loaded driver)

**Test Steps:**
1. Attempt to read 256 bytes from device
2. Verify return value

**Expected Result:**
- read() returns 0 (EOF)
- No error is generated
- No exception occurs
- File position remains at 0

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Reading from empty buffer should return EOF (0 bytes)
- Should not hang or block
- Should be consistent with standard behavior

---

### TC-5.4: Invalid IOCTL Number

**Test ID:** TC-5.4  
**Category:** Boundary / IOCTL  
**Priority:** High  
**Objective:** Verify handling of invalid IOCTL commands

**Prerequisites:**
- Device is open

**Test Steps:**
1. Invoke ioctl() with invalid command number (e.g., 0x12345678)
2. Verify ioctl() returns error

**Expected Result:**
- ioctl() returns -1
- errno is set to ENOTTY (25) - Not a TTY device
- No crash or kernel oops
- Device remains usable for subsequent operations

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Wrong magic number or invalid NR should be rejected
- _ENOTTY is standard response for unknown ioctl
- Device should remain stable

---

### TC-5.5: Invalid Mode Value

**Test ID:** TC-5.5  
**Category:** Boundary / IOCTL  
**Priority:** High  
**Objective:** Verify handling of invalid device mode values

**Prerequisites:**
- Device is open

**Test Steps:**
1. Invoke SMART_SET_DEVICE_MODE with mode = 99 (invalid)
2. Verify ioctl() returns error
3. Verify device mode is unchanged
4. Verify device remains usable

**Expected Result:**
- ioctl() returns -1
- errno is set to EINVAL (22)
- Device mode unchanged (still previous value)
- Subsequent GET_DEVICE_MODE returns unchanged mode

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Values >= SMART_MODE_MAX should be rejected
- Should use -EINVAL for invalid values
- No partial updates should occur

---

### TC-5.6: Invalid Timeout Value

**Test ID:** TC-5.6  
**Category:** Boundary / IOCTL  
**Priority:** High  
**Objective:** Verify handling of invalid timeout values

**Prerequisites:**
- Device is open

**Test Steps (Variation 1: Zero value):**
1. Invoke SMART_SET_TIMEOUT with timeout_ms = 0
2. Verify ioctl() returns error
3. Verify timeout unchanged

**Test Steps (Variation 2: Exceeds maximum):**
1. Invoke SMART_SET_TIMEOUT with timeout_ms = 100000 (exceeds 60000 max)
2. Verify ioctl() returns error
3. Verify timeout unchanged

**Expected Result (Both variations):**
- ioctl() returns -1
- errno is set to EINVAL (22)
- Timeout value unchanged (still previous value)
- Subsequent GET_TIMEOUT returns unchanged timeout
- Device remains usable

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Valid range: 1 <= timeout_ms <= 60000
- Zero and out-of-range values should be rejected
- No partial updates should occur

---

## Negative Test Cases

### TC-6.1: Device Not Open

**Test ID:** TC-6.1  
**Category:** Negative / Error Handling  
**Priority:** Medium  
**Objective:** Verify error when accessing non-opened device

**Prerequisites:**
- None

**Test Steps:**
1. Do NOT open device
2. Attempt to write to invalid file descriptor (-1)
3. Attempt to read from invalid file descriptor (-1)
4. Attempt ioctl on invalid file descriptor (-1)

**Expected Result:**
- All operations return -1
- errno is set to EBADF (Bad file descriptor)
- No kernel oops or crash
- No memory leaks

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Standard POSIX error behavior
- File descriptor validation is at syscall layer
- No kernel crash should occur

---

### TC-6.2: Permission Denied

**Test ID:** TC-6.2  
**Category:** Negative / Error Handling  
**Priority:** Medium  
**Objective:** Verify permission checking on device node

**Prerequisites:**
- Device file permissions are restricted

**Test Steps:**
1. Change device permissions to 600 (read/write owner only)
2. Attempt to open device as non-owner user
3. Verify operation fails

**Expected Result:**
- open() returns -1
- errno is set to EACCES (Permission denied)
- Device remains secure

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Permission checking is at filesystem layer
- Device itself doesn't enforce permissions
- Test may require multiple users

---

### TC-6.3: Memory Allocation Failure (Simulated)

**Test ID:** TC-6.3  
**Category:** Negative / Resource Management  
**Priority:** Medium  
**Objective:** Verify graceful handling of memory constraints

**Prerequisites:**
- Device is loaded

**Test Steps:**
1. Enable memory pressure (administrative command)
2. Attempt to perform operations
3. Verify graceful degradation or error reporting

**Expected Result:**
- Operations fail gracefully with appropriate errors
- No kernel oops or panic
- System remains responsive
- Device can be unloaded cleanly

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Simulated by system admin or memory pressure test tools
- Driver should handle allocation failures
- No resource leaks should occur

---

## Concurrency Test Cases

### TC-7.1: Concurrent Read/Write Operations

**Test ID:** TC-7.1  
**Category:** Concurrency / Synchronization  
**Priority:** High  
**Objective:** Verify correct serialization of concurrent operations

**Prerequisites:**
- Driver is loaded
- Mutex synchronization is implemented

**Test Steps:**
1. Open device from two separate processes
2. Process 1: Write data, read data, write new data
3. Process 2: Read data, get stats, clear buffer (concurrent with Process 1)
4. Monitor kernel logs and statistics
5. Verify all operations complete without corruption

**Expected Result:**
- All operations complete successfully
- Data is not corrupted
- Statistics reflect all operations
- No deadlocks occur
- Mutex serialization prevents race conditions
- dmesg shows no warnings or errors

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Tests mutex_lock implementation
- Data consistency must be maintained
- No mutex deadlocks should occur
- Statistics should be consistent

---

### TC-7.2: Rapid Open/Close Cycles

**Test ID:** TC-7.2  
**Category:** Concurrency / Resource Management  
**Priority:** Medium  
**Objective:** Verify stability under rapid open/close operations

**Prerequisites:**
- Driver is loaded

**Test Steps:**
1. Perform 100 rapid open/close cycles
2. Monitor for:
   - Resource leaks
   - Deadlocks
   - Permission errors
   - Device corruption
3. Verify statistics accuracy

**Expected Result:**
- All 100 cycles complete successfully
- opens counter = 100
- closes counter = 100
- No memory leaks detected
- No kernel warnings
- Device remains usable

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Tests reference counting
- Verifies cleanup code is correct
- Statistics must remain accurate

---

### TC-7.3: IOCTL Race Condition Test

**Test ID:** TC-7.3  
**Category:** Concurrency / IOCTL  
**Priority:** High  
**Objective:** Verify thread safety of IOCTL commands

**Prerequisites:**
- User_app --stress mode available

**Test Steps:**
1. Run stress test with 16 threads, 500 operations each
2. Each thread alternates between:
   - SET_DEVICE_MODE with varying modes
   - GET_DEVICE_MODE
   - SET_TIMEOUT with varying values
   - GET_TIMEOUT
3. Monitor for:
   - Inconsistent state
   - Lost updates
   - Unexpected mode/timeout combinations

**Expected Result:**
- All 8000 operations complete successfully
- Final device state is valid
- No "FAIL" status is reported
- Statistics show all operations recorded
- No kernel warnings or oops
- Mutex protection is working correctly

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Critical test for mutex correctness
- Race conditions would cause unpredictable behavior
- All operations must be atomic from user perspective

---

## Debugging and Instrumentation

### TC-8.1: Kernel Log Analysis

**Test ID:** TC-8.1  
**Category:** Debugging / Diagnostics  
**Priority:** Medium  
**Objective:** Verify kernel logging is comprehensive and accurate

**Prerequisites:**
- Driver is loaded with logging enabled
- Various operations have been performed

**Test Steps:**
1. Execute: `dmesg | grep smart_device | tail -50`
2. Verify log entries for:
   - Driver load/unload
   - Device open/release
   - Read/write operations
   - IOCTL calls
   - Error conditions
3. Check log format and content accuracy

**Expected Result:**
- Kernel logs contain clear, formatted messages
- Log format: "smart_device: <operation> <details>"
- Includes PID of calling process
- Timestamps are correct
- No duplicate or missing messages
- No kernel oops or panic messages

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses pr_info for logging (ratelimited)
- Logs should be helpful for debugging
- No sensitive information should be logged

---

### TC-8.2: Device Statistics Accuracy

**Test ID:** TC-8.2  
**Category:** Debugging / Diagnostics  
**Priority:** High  
**Objective:** Verify that device statistics are accurate and consistent

**Prerequisites:**
- Device has been used for various operations
- Stress test has completed

**Test Steps:**
1. Get device statistics: `./user_app --cmd stats`
2. Manually count operations performed
3. Verify each counter matches expected value:
   - opens = number of times device was opened
   - closes = number of times device was closed
   - reads = number of read() syscalls
   - writes = number of write() syscalls
   - ioctls = number of ioctl() syscalls
   - bytes_read = total bytes returned
   - bytes_written = total bytes accepted
   - errors = number of operations that failed

**Expected Result:**
- All counters match manually counted operations
- No overflow or underflow
- Counters are never negative
- Snapshot is internally consistent
- Statistics persist across multiple queries

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Counters should be exact
- Protected by mutex for consistency
- Should reset on SMART_RESET_DEVICE (except ioctls)

---

### TC-8.3: Memory Leak Detection

**Test ID:** TC-8.3  
**Category:** Debugging / Diagnostics  
**Priority:** High  
**Objective:** Verify no memory is leaked during driver operation

**Prerequisites:**
- Driver is loaded
- Stress test has been executed

**Test Steps:**
1. Check `/proc/meminfo` before test
2. Execute: `./user_app --stress 8 500`
3. Check `/proc/meminfo` after test
4. Verify memory usage is reasonable
5. Check kernel memory allocation counters
6. Unload driver and verify memory freed

**Expected Result:**
- Memory usage before ≈ memory usage after
- No significant memory growth (< 1 MB)
- Driver unload frees all allocated memory
- No "memory leak" warnings in dmesg
- No orphaned kmalloc'd blocks

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Uses kmalloc/kfree for buffer management
- All allocations should be freed on unload
- Pool allocations should be bounded

---

### TC-8.4: strace Command Analysis

**Test ID:** TC-8.4  
**Category:** Debugging / System Tracing  
**Priority:** Medium  
**Objective:** Verify syscall interface behavior

**Prerequisites:**
- Device is open
- strace utility is available

**Test Steps:**
1. Execute: `strace -e read,write,ioctl ./user_app --cmd write "test"`
2. Examine system call trace
3. Verify:
   - open() is called for device
   - write() syscall is used
   - ioctl() syscalls are traced
   - close() is called
   - All syscalls return appropriate values

**Expected Result:**
- strace output shows all expected syscalls
- Return values are correct
- File descriptors are valid
- No unexpected syscalls
- Trace reveals correct parameter passing

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Useful for debugging user-kernel interaction
- Shows copy_to/from_user details
- Can reveal parameter passing issues

---

### TC-8.5: GDB Debugger Session

**Test ID:** TC-8.5  
**Category:** Debugging / Advanced Diagnostics  
**Priority:** Low  
**Objective:** Verify driver can be debugged with GDB

**Prerequisites:**
- GDB and kernel debug symbols available
- User application compiled with debug symbols

**Test Steps:**
1. Compile user_app with -g flag: `gcc -g ...`
2. Run under GDB: `gdb ./user_app`
3. Set breakpoints in open(), write(), ioctl() calls
4. Step through code execution
5. Inspect variables and buffers
6. Verify data integrity

**Expected Result:**
- GDB can attach to user application
- Breakpoints function correctly
- Variables display correctly
- Data inspection shows expected values
- Stepping works as expected

**Actual Result:** ___________

**Status:** ☐ PASS  ☐ FAIL  

**Notes:**
- Useful for interactive debugging
- Requires debug symbols in binary
- Kernel module debugging requires kgdb/kdb
- Can reveal synchronization issues

---

## Test Execution Summary

### Quick Test Run (< 5 minutes)
```bash
sudo ./load_driver.sh          # 30 seconds
sudo ./test.sh                 # 2 minutes
sudo ./unload_driver.sh        # 30 seconds
```

### Full Test Run (< 15 minutes)
```bash
sudo ./load_driver.sh                           # 30 seconds
sudo ./test.sh --verbose --report --stress     # 10-12 minutes
sudo ./unload_driver.sh                         # 30 seconds
cat test_report.txt                             # Review results
```

### Extended Validation (> 1 hour)
```bash
# Run multiple stress iterations
for i in {1..5}; do
  echo "Stress iteration $i"
  sudo ./user_app --stress 16 1000
done

# Run concurrency tests
# Run custom test scripts
# Verify no memory leaks
```

---

## Test Result Documentation

For each test case executed, document:

1. **Date and Time:** When test was executed
2. **Kernel Version:** `uname -r`
3. **Test Status:** PASS / FAIL
4. **Actual Result:** What actually happened
5. **Error Messages:** Any error output
6. **Kernel Logs:** Relevant dmesg entries
7. **Notes:** Any observations or issues

### Sample Result Template

```
Test Case: TC-1.1 Device Open and Close
Date: May 11, 2026 14:30:00
Kernel: 6.1.0-generic-x86_64
Status: PASS
Result: Device successfully opened and closed
Logs: smart_device: open() pid=1234
      smart_device: release() pid=1234
Notes: None
```

---

## Conclusion

This comprehensive test suite provides complete coverage of the smart_device driver functionality, including:

- Basic device operations
- All IOCTL commands
- Edge cases and error conditions
- Concurrent access and race conditions
- Memory management and resource leaks
- Kernel logging and debugging

Regular execution of these tests ensures driver reliability, correctness, and robustness.

---

**Document End**
