# Smart Device Driver Project

## 1. Project Overview
This project is an industry-grade Linux character device driver designed for Kernel 6.x architectures (x86_64 and ARM64). The driver demonstrates standard operations (`open`, `read`, `write`, `release`), concurrency handling via `mutex`, internal state management, and highly detailed communication using multiple `ioctl` commands. It includes a user-space application, automated shell scripts, and complete documentation.

## 2. Driver Architecture
The architecture is divided into two spaces:
- **User Space:** Contains `user_app.c` which interfaces with the driver via system calls.
- **Kernel Space:** Contains `smart_driver.c`, structured using the Linux VFS (Virtual File System) to map file operations.
  
**Communication Flow:**
1. User application issues a `write()` system call.
2. VFS maps this to the driver's `smart_write()` function.
3. The kernel acquires a mutex lock.
4. Data is safely moved from user space using `copy_from_user()`.
5. The lock is released, and stats are updated.

## 3. Build Instructions
To compile the kernel module and the user-space application, you will need the `build-essential` and Linux headers packages for your current kernel.

```bash
# Compile both the kernel driver (smart_driver.ko) and the user application (user_app)
make

# Clean up build artifacts
make clean
```

## 4. How to Test
A complete automated test suite is provided. You must have root privileges to insert the module and create device nodes.

1. **Load the driver:**
   ```bash
   sudo ./load_driver.sh
   ```
   *This script automatically checks for existing instances, inserts the module (`insmod`), and uses `mknod` to create `/dev/smart_device` with the correct major number and permissions.*

2. **Run the automated test suite:**
   ```bash
   sudo ./test.sh
   ```
   *This script executes the `user_app` binary, testing basic I/O, advanced IOCTLs (configuration, status polling, clearing buffers, triggering actions), and concurrency testing.*

3. **Unload the driver:**
   ```bash
   sudo ./unload_driver.sh
   ```
   *This script removes the device node and unloads the kernel module (`rmmod`).*

## 9. Debugging Guide
When developing this driver, standard kernel debugging tools are essential:

- **dmesg:** Run `dmesg -w` to see real-time `printk` logs. Check for "smart_device" logs.
- **strace:** Run `strace ./user_app` to trace every system call made and the exact arguments sent.
- **lsmod:** Verify the module is loaded via `lsmod | grep smart_driver`.
- **Race conditions:** Always lock state and buffer using `mutex_lock` and `mutex_unlock`. Forgeting unlocking leads to process hanging (D state).
- **Memory leaks:** Verify all `kzalloc` allocations have a matching `kfree` in the `__exit` function.

## 11. Future Enhancements
- **Interrupt Handling:** Connect the driver to physical hardware interrupts rather than mock states.
- **Workqueues:** Move heavy logging or processing to kernel workqueues.
- **mmap() Support:** Allow user space to map the kernel buffer directly to memory for zero-copy read/writes.
- **Multiple Instances:** Modify the `dev_num` allocation to support 10+ devices (smart_device0, smart_device1, etc.).
