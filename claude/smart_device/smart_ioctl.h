#ifndef SMART_IOCTL_H
#define SMART_IOCTL_H

#include <linux/ioctl.h>

/*
 * Magic Number Definition:
 * A magic number is a unique 8-bit character identifying the driver.
 * It prevents conflicts if our IOCTL commands accidentally match another driver's commands.
 */
#define SMART_MAGIC 'S'

/*
 * Data Structures for IOCTLs
 */
struct smart_stats {
    unsigned long bytes_read;
    unsigned long bytes_written;
    unsigned long ioctl_count;
    unsigned long open_count;
};

/* Device Modes */
#define MODE_NORMAL     0
#define MODE_STRICT     1
#define MODE_LOOPBACK   2

/*
 * IOCTL Command Definitions using Kernel Macros:
 * _IO(magic, num)        : No data transfer.
 * _IOR(magic, num, type) : Read data from kernel to user space.
 * _IOW(magic, num, type) : Write data from user space to kernel.
 * _IOWR(magic, num, type): Read and write data.
 */

// 1. Reset device state to defaults
#define RESET_DEVICE      _IO(SMART_MAGIC, 0)

// 2. Get device operational statistics
#define GET_DRIVER_STATS  _IOR(SMART_MAGIC, 1, struct smart_stats)

// 3. Clear the internal kernel buffer
#define CLEAR_BUFFER      _IO(SMART_MAGIC, 2)

// 4. Set device operation mode (Normal, Strict, Loopback)
#define SET_DEVICE_MODE   _IOW(SMART_MAGIC, 3, int)

// 5. Get current device operation mode
#define GET_DEVICE_MODE   _IOR(SMART_MAGIC, 4, int)

// 6. Enable extra printk logging
#define ENABLE_LOGGING    _IO(SMART_MAGIC, 5)

// 7. Disable extra printk logging
#define DISABLE_LOGGING   _IO(SMART_MAGIC, 6)

// 8. Get the last command/string that was written
#define GET_LAST_COMMAND  _IOR(SMART_MAGIC, 7, char[256])

// 9. Set a mock timeout value
#define SET_TIMEOUT       _IOW(SMART_MAGIC, 8, int)

// 10. Get the mock timeout value
#define GET_TIMEOUT       _IOR(SMART_MAGIC, 9, int)

#endif /* SMART_IOCTL_H */
