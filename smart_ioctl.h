/*
 * smart_ioctl.h
 * -------------
 * Shared header between the smart_device kernel driver and the
 * user-space application. This file defines:
 *
 *   - The IOCTL magic number ('S')
 *   - All IOCTL command codes (built with _IO/_IOR/_IOW/_IOWR macros)
 *   - The data structures exchanged between user and kernel
 *
 * IOCTL command numbers are constructed by the kernel's <asm/ioctl.h>
 * macros. Each command encodes 4 fields into a single 32-bit integer:
 *
 *   +--------+--------+--------+--------+
 *   |  DIR   |  SIZE  |  TYPE  |  NR    |
 *   | 2 bits |14 bits | 8 bits | 8 bits |
 *   +--------+--------+--------+--------+
 *
 *   DIR  : direction of data transfer
 *          _IOC_NONE  (0) -> _IO()    (no data)
 *          _IOC_WRITE (1) -> _IOW()   (user -> kernel)
 *          _IOC_READ  (2) -> _IOR()   (kernel -> user)
 *          _IOC_READ|WRITE -> _IOWR() (both directions)
 *   SIZE : sizeof(arg_type) — used for sanity-checking by the kernel.
 *   TYPE : "magic number" identifying THIS driver. We pick 'S' for "Smart".
 *          Pick something not already used in Documentation/userspace-api/
 *          ioctl/ioctl-number.rst to avoid clashes in production drivers.
 *   NR   : sequence number for the command (0, 1, 2, ...).
 *
 * The kernel uses the (TYPE, NR) pair to dispatch the call; SIZE+DIR
 * are used in copy_from_user/copy_to_user safety checks. This is why
 * passing the WRONG ioctl number from user-space generates -ENOTTY.
 */

#ifndef _SMART_IOCTL_H
#define _SMART_IOCTL_H

#include <linux/ioctl.h>

/* Device name as it appears under /dev */
#define SMART_DEVICE_NAME   "smart_device"
#define SMART_CLASS_NAME    "smart_class"

/* Size of the internal kernel buffer (bytes). 4 KiB is the usual page
 * size and a comfortable starting point for character drivers. */
#define SMART_BUFFER_SIZE   4096

/* Maximum size of the "description" string returned by GET_LAST_COMMAND. */
#define SMART_DESC_LEN      64

/* ---------------------------------------------------------------------
 * Magic number for this driver. Convention: pick a single ASCII letter
 * unlikely to clash. 'S' = "Smart device".
 * ---------------------------------------------------------------------
 */
#define SMART_IOC_MAGIC     'S'

/* ---------------------------------------------------------------------
 * Device operating modes — used by SET_MODE / GET_MODE.
 * The names are documented in the user manual and the kernel log.
 * ---------------------------------------------------------------------
 */
enum smart_device_mode {
	SMART_MODE_NORMAL = 0,   /* default — balanced behaviour          */
	SMART_MODE_DEBUG  = 1,   /* extra printk traces on every op       */
	SMART_MODE_PERF   = 2,   /* skip non-essential bookkeeping        */
	SMART_MODE_SAFE   = 3,   /* extra boundary checks, slower         */
	SMART_MODE_MAX                              /* sentinel, do not use */
};

/* ---------------------------------------------------------------------
 * Data structures exchanged via IOCTL.
 *
 * IMPORTANT: these structs are visible to BOTH the kernel and user
 * space, so we must NOT use kernel-only types (e.g. u64). We use the
 * standard fixed-width ABI types from <linux/types.h> (__u32, __u64).
 * ---------------------------------------------------------------------
 */
#include <linux/types.h>

struct smart_stats {
	__u64 open_count;       /* number of successful open() calls     */
	__u64 close_count;      /* number of release() calls             */
	__u64 read_count;       /* number of read() calls                */
	__u64 write_count;      /* number of write() calls               */
	__u64 ioctl_count;      /* number of ioctl() calls               */
	__u64 bytes_read;       /* total bytes delivered to user         */
	__u64 bytes_written;    /* total bytes received from user        */
	__u64 error_count;      /* number of errors returned             */
};

struct smart_cmd_info {
	__s32 last_cmd;                          /* numeric ioctl code   */
	__u32 timestamp;                         /* jiffies snapshot     */
	char  description[SMART_DESC_LEN];       /* human-readable name  */
};

/* ---------------------------------------------------------------------
 * IOCTL command codes.
 *
 *   _IO   : command without data argument
 *   _IOR  : command that READS  data from kernel  (kernel writes  -> user)
 *   _IOW  : command that WRITES data to   kernel  (user   writes  -> kernel)
 *   _IOWR : bidirectional (rarely needed; kept for completeness)
 *
 * NR (the third macro argument) must be unique per command in this driver.
 * ---------------------------------------------------------------------
 */
#define SMART_IOCTL_RESET_DEVICE     _IO(SMART_IOC_MAGIC, 0)
#define SMART_IOCTL_GET_STATS        _IOR(SMART_IOC_MAGIC, 1, struct smart_stats)
#define SMART_IOCTL_CLEAR_BUFFER     _IO(SMART_IOC_MAGIC, 2)
#define SMART_IOCTL_SET_MODE         _IOW(SMART_IOC_MAGIC, 3, __s32)
#define SMART_IOCTL_GET_MODE         _IOR(SMART_IOC_MAGIC, 4, __s32)
#define SMART_IOCTL_ENABLE_LOGGING   _IO(SMART_IOC_MAGIC, 5)
#define SMART_IOCTL_DISABLE_LOGGING  _IO(SMART_IOC_MAGIC, 6)
#define SMART_IOCTL_GET_LAST_CMD     _IOR(SMART_IOC_MAGIC, 7, struct smart_cmd_info)
#define SMART_IOCTL_SET_TIMEOUT      _IOW(SMART_IOC_MAGIC, 8, __u32)
#define SMART_IOCTL_GET_TIMEOUT      _IOR(SMART_IOC_MAGIC, 9, __u32)

/* Highest NR used — for range-checking inside the driver's
 * unlocked_ioctl handler. Keep in sync if you add new commands. */
#define SMART_IOCTL_MAX_NR           9

#endif /* _SMART_IOCTL_H */
