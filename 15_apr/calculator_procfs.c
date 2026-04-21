#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#define PROC_NAME "proc_demo"
#define BUF_SIZE 128

static int a, b, result;
static char op;

static ssize_t proc_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
    char buf[BUF_SIZE];
    int len;

    len = snprintf(buf, BUF_SIZE, "%d\n", result);

    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t proc_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
    char buf[BUF_SIZE] = {0};
    int ret;

    if (count >= BUF_SIZE)
        return -EINVAL;

    if (copy_from_user(buf, user_buf, count))
        return -EFAULT;

    ret = sscanf(buf, "%d %d %c", &a, &b, &op);
    if (ret != 3)
        return -EINVAL;

    switch (op) {
    case '+':
        result = a + b;
        break;
    case '-':
        result = a - b;
        break;
    case '*':
        result = a * b;
        break;
    case '/':
        if (b == 0)
            return -EINVAL;
        result = a / b;
        break;
    default:
        return -EINVAL;
    }

    return count;
}

static const struct proc_ops proc_file_ops = {
    .proc_read = proc_read,
    .proc_write = proc_write,
};

static int __init proc_demo_init(void)
{
    if (!proc_create(PROC_NAME, 0666, NULL, &proc_file_ops)) {
        pr_err("Failed to create proc entry\n");
        return -ENOMEM;
    }

    pr_info("proc_demo loaded\n");
    return 0;
}

static void __exit proc_demo_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    pr_info("proc_demo unloaded\n");
}

module_init(proc_demo_init);
module_exit(proc_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Procfs calculator");