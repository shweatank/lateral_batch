#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

static struct work_struct my_work;

static void my_work_handler(struct work_struct *work)
{
    pr_info("Workqueue: Handler started\n");
    msleep(2000);
    pr_info("Workqueue: Handler finished\n");
}

static int __init workq_init(void){
    pr_info("Workqueue module loaded\n");
    INIT_WORK(&my_work, my_work_handler);
    pr_info("Workqueue: Scheduling Work\n");
    schedule_work(&my_work);
    return 0;
}

static void __exit workq_exit(void){
    pr_info("Workqueue module exiting\n");
    flush_work(&my_work);
    pr_info("Workqueue module unloaded\n");
}


module_init(workq_init);
module_exit(workq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Sample Workqueue Program");