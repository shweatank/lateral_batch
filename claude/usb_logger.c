/*
 * Sample High-Speed USB Data Logger - Linux host driver
 *
 * Architecture:
 *   - Probes a USB device matching VID/PID and grabs its bulk IN endpoint.
 *   - Submits a pool of URBs for continuous bulk IN streaming.
 *   - URB completion callback pushes data into a circular buffer and
 *     immediately resubmits the URB to keep the pipe saturated.
 *   - User space reads from /dev/usb_logger0 (blocking or non-blocking).
 *   - Tracks dropped bytes (buffer overrun) and per-URB timestamps.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>

#define USB_LOGGER_VENDOR_ID    0x1234
#define USB_LOGGER_PRODUCT_ID   0x5678

#define USB_LOGGER_MINOR_BASE   192
#define USB_LOGGER_NUM_URBS     8
#define USB_LOGGER_URB_SIZE     (16 * 1024)   /* 16 KB per URB */
#define USB_LOGGER_RING_SIZE    (1 << 20)     /* 1 MiB ring buffer */

struct usb_logger {
    struct usb_device       *udev;
    struct usb_interface    *intf;

    __u8                     bulk_in_ep;
    size_t                   bulk_in_mps;

    struct urb              *urbs[USB_LOGGER_NUM_URBS];
    unsigned char           *urb_buf[USB_LOGGER_NUM_URBS];

    /* Host-side circular buffer */
    unsigned char           *ring;
    size_t                   ring_head;   /* producer: completion ctx */
    size_t                   ring_tail;   /* consumer: read() ctx */
    size_t                   ring_count;
    spinlock_t               ring_lock;

    u64                      bytes_received;
    u64                      bytes_dropped;
    ktime_t                  last_packet_ts;

    wait_queue_head_t        read_wq;
    struct mutex             io_mutex;     /* serializes open/close vs disconnect */
    struct kref              kref;
    bool                     disconnected;
};

#define to_logger(d) container_of(d, struct usb_logger, kref)

static struct usb_driver usb_logger_driver;

/* ------------------------------------------------------------------ */
/* Ring buffer helpers (called with ring_lock held)                    */
/* ------------------------------------------------------------------ */

static size_t ring_space(struct usb_logger *dev)
{
    return USB_LOGGER_RING_SIZE - dev->ring_count;
}

static void ring_push(struct usb_logger *dev, const unsigned char *src, size_t len)
{
    size_t space = ring_space(dev);
    size_t to_copy, first;

    if (len > space) {
        /* Overrun: drop oldest by advancing tail */
        size_t drop = len - space;
        dev->ring_tail = (dev->ring_tail + drop) % USB_LOGGER_RING_SIZE;
        dev->ring_count -= drop;
        dev->bytes_dropped += drop;
    }

    to_copy = len;
    first = min(to_copy, USB_LOGGER_RING_SIZE - dev->ring_head);
    memcpy(dev->ring + dev->ring_head, src, first);
    if (to_copy > first)
        memcpy(dev->ring, src + first, to_copy - first);

    dev->ring_head = (dev->ring_head + to_copy) % USB_LOGGER_RING_SIZE;
    dev->ring_count += to_copy;
}

static size_t ring_pop_to_user(struct usb_logger *dev, char __user *ubuf, size_t len)
{
    size_t avail, to_copy, first;
    unsigned long flags;
    size_t copied = 0;
    unsigned char *bounce;

    spin_lock_irqsave(&dev->ring_lock, flags);
    avail = dev->ring_count;
    to_copy = min(len, avail);
    if (to_copy == 0) {
        spin_unlock_irqrestore(&dev->ring_lock, flags);
        return 0;
    }

    /* Bounce to a kernel buffer first so we don't hold the spinlock
     * across copy_to_user (which may sleep / fault).
     */
    bounce = kmalloc(to_copy, GFP_ATOMIC);
    if (!bounce) {
        spin_unlock_irqrestore(&dev->ring_lock, flags);
        return -ENOMEM;
    }

    first = min(to_copy, USB_LOGGER_RING_SIZE - dev->ring_tail);
    memcpy(bounce, dev->ring + dev->ring_tail, first);
    if (to_copy > first)
        memcpy(bounce + first, dev->ring, to_copy - first);

    dev->ring_tail = (dev->ring_tail + to_copy) % USB_LOGGER_RING_SIZE;
    dev->ring_count -= to_copy;
    spin_unlock_irqrestore(&dev->ring_lock, flags);

    if (copy_to_user(ubuf, bounce, to_copy))
        copied = -EFAULT;
    else
        copied = to_copy;

    kfree(bounce);
    return copied;
}

/* ------------------------------------------------------------------ */
/* URB completion: producer side                                       */
/* ------------------------------------------------------------------ */

static void usb_logger_read_complete(struct urb *urb)
{
    struct usb_logger *dev = urb->context;
    unsigned long flags;
    int ret;

    switch (urb->status) {
    case 0:
        break;
    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
        /* Unlinked or device gone — do not resubmit */
        return;
    default:
        dev_warn(&dev->intf->dev, "urb status %d, resubmitting\n", urb->status);
        goto resubmit;
    }

    if (urb->actual_length) {
        spin_lock_irqsave(&dev->ring_lock, flags);
        ring_push(dev, urb->transfer_buffer, urb->actual_length);
        dev->bytes_received += urb->actual_length;
        dev->last_packet_ts = ktime_get();
        spin_unlock_irqrestore(&dev->ring_lock, flags);
        wake_up_interruptible(&dev->read_wq);
    }

resubmit:
    if (dev->disconnected)
        return;

    ret = usb_submit_urb(urb, GFP_ATOMIC);
    if (ret && ret != -EPERM)
        dev_err(&dev->intf->dev, "resubmit failed: %d\n", ret);
}

/* ------------------------------------------------------------------ */
/* kref-based lifetime                                                 */
/* ------------------------------------------------------------------ */

static void usb_logger_delete(struct kref *kref)
{
    struct usb_logger *dev = to_logger(kref);
    int i;

    for (i = 0; i < USB_LOGGER_NUM_URBS; i++) {
        if (dev->urbs[i]) {
            usb_free_coherent(dev->udev, USB_LOGGER_URB_SIZE,
                              dev->urb_buf[i], dev->urbs[i]->transfer_dma);
            usb_free_urb(dev->urbs[i]);
        }
    }
    usb_put_dev(dev->udev);
    kfree(dev->ring);
    kfree(dev);
}

/* ------------------------------------------------------------------ */
/* File operations                                                     */
/* ------------------------------------------------------------------ */

static int usb_logger_open(struct inode *inode, struct file *file)
{
    struct usb_interface *intf;
    struct usb_logger *dev;
    int subminor = iminor(inode);
    int i, ret;

    intf = usb_find_interface(&usb_logger_driver, subminor);
    if (!intf)
        return -ENODEV;

    dev = usb_get_intfdata(intf);
    if (!dev)
        return -ENODEV;

    kref_get(&dev->kref);
    file->private_data = dev;

    /* Submit the URB pool on first open. Already-submitted URBs return
     * -EBUSY which we treat as success.
     */
    for (i = 0; i < USB_LOGGER_NUM_URBS; i++) {
        ret = usb_submit_urb(dev->urbs[i], GFP_KERNEL);
        if (ret && ret != -EBUSY) {
            dev_err(&intf->dev, "submit urb %d failed: %d\n", i, ret);
            kref_put(&dev->kref, usb_logger_delete);
            return ret;
        }
    }

    return 0;
}

static int usb_logger_release(struct inode *inode, struct file *file)
{
    struct usb_logger *dev = file->private_data;

    if (!dev)
        return -ENODEV;

    kref_put(&dev->kref, usb_logger_delete);
    return 0;
}

static ssize_t usb_logger_read(struct file *file, char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct usb_logger *dev = file->private_data;
    ssize_t ret;

    if (dev->disconnected)
        return -ENODEV;

    while (true) {
        unsigned long flags;
        size_t avail;

        spin_lock_irqsave(&dev->ring_lock, flags);
        avail = dev->ring_count;
        spin_unlock_irqrestore(&dev->ring_lock, flags);

        if (avail)
            break;

        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;

        if (wait_event_interruptible(dev->read_wq,
                                     dev->ring_count > 0 || dev->disconnected))
            return -ERESTARTSYS;

        if (dev->disconnected)
            return -ENODEV;
    }

    ret = ring_pop_to_user(dev, buf, count);
    return ret;
}

static const struct file_operations usb_logger_fops = {
    .owner    = THIS_MODULE,
    .open     = usb_logger_open,
    .release  = usb_logger_release,
    .read     = usb_logger_read,
    .llseek   = no_llseek,
};

static struct usb_class_driver usb_logger_class = {
    .name       = "usb_logger%d",
    .fops       = &usb_logger_fops,
    .minor_base = USB_LOGGER_MINOR_BASE,
};

/* ------------------------------------------------------------------ */
/* probe / disconnect                                                  */
/* ------------------------------------------------------------------ */

static int usb_logger_probe(struct usb_interface *intf,
                            const struct usb_device_id *id)
{
    struct usb_logger *dev;
    struct usb_endpoint_descriptor *bulk_in = NULL;
    int ret, i;

    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    kref_init(&dev->kref);
    mutex_init(&dev->io_mutex);
    spin_lock_init(&dev->ring_lock);
    init_waitqueue_head(&dev->read_wq);

    dev->udev = usb_get_dev(interface_to_usbdev(intf));
    dev->intf = intf;

    ret = usb_find_common_endpoints(intf->cur_altsetting,
                                    &bulk_in, NULL, NULL, NULL);
    if (ret) {
        dev_err(&intf->dev, "no bulk IN endpoint found\n");
        goto err_free;
    }

    dev->bulk_in_ep  = bulk_in->bEndpointAddress;
    dev->bulk_in_mps = usb_endpoint_maxp(bulk_in);

    dev->ring = kmalloc(USB_LOGGER_RING_SIZE, GFP_KERNEL);
    if (!dev->ring) {
        ret = -ENOMEM;
        goto err_free;
    }

    for (i = 0; i < USB_LOGGER_NUM_URBS; i++) {
        struct urb *urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb) {
            ret = -ENOMEM;
            goto err_urbs;
        }

        dev->urb_buf[i] = usb_alloc_coherent(dev->udev, USB_LOGGER_URB_SIZE,
                                             GFP_KERNEL, &urb->transfer_dma);
        if (!dev->urb_buf[i]) {
            usb_free_urb(urb);
            ret = -ENOMEM;
            goto err_urbs;
        }

        usb_fill_bulk_urb(urb, dev->udev,
                          usb_rcvbulkpipe(dev->udev, dev->bulk_in_ep),
                          dev->urb_buf[i], USB_LOGGER_URB_SIZE,
                          usb_logger_read_complete, dev);
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

        dev->urbs[i] = urb;
    }

    usb_set_intfdata(intf, dev);

    ret = usb_register_dev(intf, &usb_logger_class);
    if (ret) {
        dev_err(&intf->dev, "usb_register_dev failed: %d\n", ret);
        usb_set_intfdata(intf, NULL);
        goto err_urbs;
    }

    dev_info(&intf->dev,
             "usb_logger attached: ep 0x%02x, mps %zu, minor %d\n",
             dev->bulk_in_ep, dev->bulk_in_mps, intf->minor);
    return 0;

err_urbs:
    for (i = 0; i < USB_LOGGER_NUM_URBS; i++) {
        if (dev->urbs[i]) {
            usb_free_coherent(dev->udev, USB_LOGGER_URB_SIZE,
                              dev->urb_buf[i], dev->urbs[i]->transfer_dma);
            usb_free_urb(dev->urbs[i]);
            dev->urbs[i] = NULL;
        }
    }
    kfree(dev->ring);
err_free:
    usb_put_dev(dev->udev);
    kfree(dev);
    return ret;
}

static void usb_logger_disconnect(struct usb_interface *intf)
{
    struct usb_logger *dev = usb_get_intfdata(intf);
    int i;

    usb_deregister_dev(intf, &usb_logger_class);
    usb_set_intfdata(intf, NULL);

    dev->disconnected = true;

    /* Stop the streaming pipeline */
    for (i = 0; i < USB_LOGGER_NUM_URBS; i++)
        usb_kill_urb(dev->urbs[i]);

    wake_up_interruptible(&dev->read_wq);

    dev_info(&intf->dev,
             "usb_logger disconnected: rx=%llu dropped=%llu\n",
             dev->bytes_received, dev->bytes_dropped);

    kref_put(&dev->kref, usb_logger_delete);
}

static const struct usb_device_id usb_logger_table[] = {
    { USB_DEVICE(USB_LOGGER_VENDOR_ID, USB_LOGGER_PRODUCT_ID) },
    { }
};
MODULE_DEVICE_TABLE(usb, usb_logger_table);

static struct usb_driver usb_logger_driver = {
    .name       = "usb_logger",
    .id_table   = usb_logger_table,
    .probe      = usb_logger_probe,
    .disconnect = usb_logger_disconnect,
};

module_usb_driver(usb_logger_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("usb_logger sample");
MODULE_DESCRIPTION("Sample high-speed USB bulk-IN data logger driver");
