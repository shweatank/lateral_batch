/*
 * smart_ioctl.h - Shared kernel/user definitions for /dev/smart_device
 *
 * This header is included verbatim by BOTH:
 *   - the kernel module  (smart_driver.c)
 *   - the user app       (user_app.c, test programs)
 *
 * Because both sides must agree on:
 *   - the ioctl magic byte
 *   - the ioctl command numbers
 *   - the structures passed across the boundary
 *
 * Only fixed-width types (__u32, __u64, __s32) are used so the binary
 * layout is identical regardless of whether the consumer is 32-bit or
 * 64-bit.
 */
#ifndef SMART_IOCTL_H
#define SMART_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

/* ---------------------------------------------------------------- */
/* IOCTL magic and number assignment                                 */
/* ---------------------------------------------------------------- */
/*
 * Every ioctl number is built from four fields packed into 32 bits:
 *
 *   bits 31..30  direction  (_IOC_NONE / _IOC_READ / _IOC_WRITE)
 *   bits 29..16  size       (sizeof of the argument struct)
 *   bits 15..8   type       (the "magic" byte)
 *   bits  7..0   nr         (sequential command number)
 *
 * The macros _IO, _IOR, _IOW, _IOWR assemble these fields for us.
 * The "magic" byte should be unique enough not to collide with other
 * drivers on the system - the canonical list lives in the kernel tree
 * under Documentation/userspace-api/ioctl/ioctl-number.rst.
 * We chose 'S' for "Smart".
 */
#define SMART_IOC_MAGIC      'S'
#define SMART_IOC_MAXNR      10

/* ---------------------------------------------------------------- */
/* Device operating modes                                            */
/* ---------------------------------------------------------------- */
enum smart_mode {
	SMART_MODE_NORMAL = 0,  /* Default behaviour                  */
	SMART_MODE_DEBUG  = 1,  /* Verbose printk on each op          */
	SMART_MODE_TEST   = 2,  /* Reserved for self-test routines    */
	SMART_MODE_SILENT = 3,  /* Suppress all printk even if logging on */
	SMART_MODE_MAX,         /* Sentinel - keep last               */
};

/* ---------------------------------------------------------------- */
/* Statistics snapshot returned by SMART_GET_DRIVER_STATS            */
/* ---------------------------------------------------------------- */
struct smart_stats {
	__u64 opens;          /* successful open() calls                */
	__u64 closes;         /* successful release() calls             */
	__u64 reads;          /* successful read() calls                */
	__u64 writes;         /* successful write() calls               */
	__u64 ioctls;         /* total ioctl() calls (success or not)   */
	__u64 bytes_read;     /* total bytes returned to user via read  */
	__u64 bytes_written;  /* total bytes accepted from user         */
	__u64 errors;         /* any operation that returned < 0        */
};

/* ---------------------------------------------------------------- */
/* Single command-history record returned by SMART_GET_LAST_COMMAND  */
/* ---------------------------------------------------------------- */
struct smart_cmd_entry {
	__u32 cmd_nr;         /* _IOC_NR(cmd) of the recorded ioctl     */
	__s32 result;         /* return value driver gave (0 or -errno) */
	__u64 timestamp_ns;   /* CLOCK_REALTIME nanoseconds             */
};

/* ---------------------------------------------------------------- */
/* IOCTL command numbers                                             */
/* ---------------------------------------------------------------- */
/*
 * Direction is from the application's point of view:
 *   _IO   : no payload
 *   _IOR  : application READS from driver  (driver writes back)
 *   _IOW  : application WRITES to   driver (driver reads in)
 *   _IOWR : both directions
 */
#define SMART_RESET_DEVICE       _IO  (SMART_IOC_MAGIC,  1)
#define SMART_GET_DRIVER_STATS   _IOR (SMART_IOC_MAGIC,  2, struct smart_stats)
#define SMART_CLEAR_BUFFER       _IO  (SMART_IOC_MAGIC,  3)
#define SMART_SET_DEVICE_MODE    _IOW (SMART_IOC_MAGIC,  4, __u32)
#define SMART_GET_DEVICE_MODE    _IOR (SMART_IOC_MAGIC,  5, __u32)
#define SMART_ENABLE_LOGGING     _IO  (SMART_IOC_MAGIC,  6)
#define SMART_DISABLE_LOGGING    _IO  (SMART_IOC_MAGIC,  7)
#define SMART_GET_LAST_COMMAND   _IOR (SMART_IOC_MAGIC,  8, struct smart_cmd_entry)
#define SMART_SET_TIMEOUT        _IOW (SMART_IOC_MAGIC,  9, __u32)
#define SMART_GET_TIMEOUT        _IOR (SMART_IOC_MAGIC, 10, __u32)

#endif /* SMART_IOCTL_H */
