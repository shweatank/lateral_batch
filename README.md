# smart_device — A Linux Character Driver with IOCTL

An industry-style teaching project for Linux kernel **6.x** on x86_64 / arm64.
It combines a character driver, IOCTL communication, synchronization, a
user-space app (menu / automated / stress modes), build / load / test
scripts, and a debugging guide.

---

## 1. Project layout

```
smart_device_driver/
├── smart_driver.c      kernel module (the driver)
├── smart_ioctl.h       shared header (ioctl numbers + structs)
├── user_app.c          user-space companion app
├── Makefile            builds module + app
├── load_driver.sh      build → insmod → verify
├── unload_driver.sh    rmmod + verify cleanup
├── test.sh             end-to-end test orchestrator
├── README.md           you are here
└── test_report.md      test cases + result template
```

---

## 2. Build & run (quick start)

```bash
# 1. Install kernel headers (one-time)
sudo apt install build-essential linux-headers-$(uname -r)

# 2. Build the module and the app
make

# 3. Load the driver
./load_driver.sh

# 4. Try it
sudo ./smart_app                 # interactive menu
sudo ./smart_app --auto          # automated test
sudo ./smart_app --stress 500    # 8 threads × 500 iterations

# 5. Unload when done
./unload_driver.sh
```

---

## 3. What this driver does

`/dev/smart_device` is a 4 KiB byte buffer that supports `read()`, `write()`,
and ten IOCTL commands. It keeps:

* **statistics** — counters for open/close/read/write/ioctl/errors and total
  bytes;
* **command history** — the last IOCTL number, name and timestamp;
* **configurable parameters** — operating mode, timeout (ms), logging on/off.

All shared state is protected by a single mutex.

---

## 4. IOCTL design

### 4.1 How an ioctl number is built

The kernel encodes 4 fields into one 32-bit integer using
`<asm/ioctl.h>` macros:

```
+--------+--------+--------+--------+
|  DIR   |  SIZE  |  TYPE  |  NR    |
| 2 bits |14 bits | 8 bits | 8 bits |
+--------+--------+--------+--------+
```

| Field | Meaning                                              |
|-------|------------------------------------------------------|
| DIR   | `_IOC_NONE / _WRITE / _READ / _READ\|_WRITE`         |
| SIZE  | `sizeof(arg_type)` — sanity-checked by the kernel    |
| TYPE  | Magic byte unique to this driver (here `'S'`)        |
| NR    | Per-driver sequence number 0,1,2,…                   |

The macros pick the right `DIR`:

```
_IO  (type, nr)            no data
_IOR (type, nr, T)         kernel WRITES T to user
_IOW (type, nr, T)         user   WRITES T to kernel
_IOWR(type, nr, T)         both
```

### 4.2 Commands

| Macro                       | Direction      | Argument               |
|-----------------------------|----------------|------------------------|
| `SMART_IOCTL_RESET_DEVICE`  | _IO            | —                      |
| `SMART_IOCTL_GET_STATS`     | _IOR           | `struct smart_stats`   |
| `SMART_IOCTL_CLEAR_BUFFER`  | _IO            | —                      |
| `SMART_IOCTL_SET_MODE`      | _IOW           | `int32_t`              |
| `SMART_IOCTL_GET_MODE`      | _IOR           | `int32_t`              |
| `SMART_IOCTL_ENABLE_LOGGING`| _IO            | —                      |
| `SMART_IOCTL_DISABLE_LOGGING`| _IO           | —                      |
| `SMART_IOCTL_GET_LAST_CMD`  | _IOR           | `struct smart_cmd_info`|
| `SMART_IOCTL_SET_TIMEOUT`   | _IOW           | `uint32_t` (ms)        |
| `SMART_IOCTL_GET_TIMEOUT`   | _IOR           | `uint32_t` (ms)        |

### 4.3 Dispatch flow

```
        user space                       kernel space
        ----------                       ------------
  ioctl(fd, CMD, &arg)
        │
        ▼  syscall
              ──►   sys_ioctl()
                        │
                        ▼
                   file_operations.unlocked_ioctl  ─►  smart_ioctl()
                                                      │
                              ┌───────────────────────┤
                              │  validate magic       │
                              │  validate NR          │
                              │  mutex_lock           │
                              │  switch(cmd)          │
                              │     copy_from_user    │
                              │     update state      │
                              │     copy_to_user      │
                              │  mutex_unlock         │
                              └───────────────────────┘
```

---

## 5. Kernel APIs and why they're used

| API                          | Why                                                      |
|------------------------------|----------------------------------------------------------|
| `alloc_chrdev_region`        | Reserve a free `<major, minor>` dynamically              |
| `cdev_init / cdev_add`       | Register the file_operations with the VFS                |
| `class_create / device_create` | udev sees these and creates `/dev/smart_device`        |
| `kzalloc / kfree`            | Allocate / free the 4 KiB internal buffer                |
| `copy_from_user / copy_to_user` | Safely cross the kernel/user boundary                  |
| `mutex_lock_interruptible`   | Sleep-safe lock that responds to signals (Ctrl-C)        |
| `pr_info / pr_err`           | Structured logging into the kernel ring buffer           |
| `_IOC_TYPE / _IOC_NR`        | Decode magic + sequence from an ioctl number             |

`class_create` lost its `THIS_MODULE` first argument in kernel 6.4; the driver
guards this with `LINUX_VERSION_CODE` so it builds on both 6.3- and 6.4+.

---

## 6. Synchronization

* **One mutex** (`smart_dev.lock`) protects every field of `smart_dev_state`.
* **Why mutex, not spinlock?** Every path can sleep — `copy_to_user` and
  `copy_from_user` may take page faults and block. Spinlocks must NEVER
  sleep; using one here would `BUG_ON` under load.
* **No global state outside the struct** — easier to reason about and to
  port to a multi-instance driver later.

---

## 7. Error handling pattern

The init function uses Linux's classic goto-cleanup ladder. If step *N*
fails, jump to `err_<N-1>` which undoes step *N-1*, falls through to
`err_<N-2>`, … all the way down. This avoids deep nesting and guarantees
no leak on partial init.

---

## 8. User-space app modes

* `--menu` (default) — interactive menu, one operation at a time.
* `--auto` — runs every IOCTL once, prints PASS/FAIL with colour, returns
  non-zero exit on any failure (CI-friendly).
* `--stress N` — 8 pthreads × N iterations of mixed read/write/ioctl. Used
  to flush out locking bugs.

---

## 9. Common debugging recipes

| Problem                          | Tool / command                                  |
|----------------------------------|-------------------------------------------------|
| Driver doesn't load              | `sudo dmesg`, `modinfo smart_driver.ko`         |
| `/dev/smart_device` missing      | `lsmod`, `cat /proc/devices`, `ls /sys/class/`  |
| `ioctl returned -1, errno 25 ENOTTY` | wrong magic / wrong NR — recompile both sides |
| User app reads zeros             | no write yet, or `lseek` to 0 missing           |
| Random Oops under load           | likely a missed `mutex_unlock` — see `dmesg`    |
| Want a syscall trace             | `sudo strace -e ioctl,read,write ./smart_app --auto` |
| Want to step into the user app   | `sudo gdb --args ./smart_app --auto`            |
| Want to see only our printk      | `sudo dmesg -w \| grep smart_device`             |

### Debugging in depth

* **printk levels** — use `pr_info` for normal events, `pr_err` for
  failures, `pr_debug` for verbose traces (enable with
  `echo 8 > /proc/sys/kernel/printk`).
* **dmesg ring buffer** — `sudo dmesg -wH` watches in real time.
* **strace** — every syscall the user app makes; very useful for
  spotting the wrong ioctl number.
* **gdb** — debug only the user-space side; the kernel side needs
  `KGDB` or QEMU.
* **Race conditions** — run `--stress` with `lockdep` enabled in the
  kernel; any deadlock prints a clear stack trace.
* **Memory leaks** — `kmemleak` (`echo scan > /sys/kernel/debug/kmemleak`)
  after load → unload cycles.

---

## 10. Troubleshooting cheat-sheet

```bash
# headers missing
sudo apt install linux-headers-$(uname -r)

# permission denied opening /dev/smart_device
sudo ./smart_app             # or write a udev rule

# module won't unload, "in use"
sudo lsof /dev/smart_device  # find who has it open
sudo fuser -k /dev/smart_device

# clean dmesg before re-running tests
sudo dmesg -C
```

---

## 11. Future enhancements (ideas)

* `/proc/smart_device` or `sysfs` knobs for stats (debugfs is even nicer).
* `poll()` / `wait_event_interruptible_timeout` driven by a kernel timer
  so the configurable timeout actually does something.
* Multiple device nodes (`/dev/smart_device0`, `…1`) sharing the same
  major. Move `smart_dev_state` into `file->private_data`.
* Replace the linear buffer with a `kfifo` so reads don't shrink the
  available data the way they do today.
* Add `mmap()` support so user space can avoid the
  `copy_to_user / copy_from_user` overhead.
