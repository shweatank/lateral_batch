#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define TIMER_INTERVEL_MS 1000 // 1 sec

static struct timer_list my_timer;
/* timer callback function */

static void my_timer_callback(struct timer_list *t)
{
	printk(KERN_INFO "timer interrupt occured:\n");
	/*restart the timer (periodic behaviour)*/
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMER_INTERVEL_MS));
}
/*MODULE INITIALIXATION*/
static int __init timer_driver_init(void)
{
	printk(KERN_INFO "timer driver loaded\n");
	/* initializing timer*/
	timer_setup(&my_timer, my_timer_callback, 0);
	/*start timer */
	mod_timer(&my_timer, jiffies + msecs_to_jiffies(TIMER_INTERVEL_MS));
	return 0;
}
/* module exit*/
static void __exit timer_driver_exit(void)
{
	del_timer_sync(&my_timer);
	printk(KERN_INFO "Timer driver unloaded\n");
}
module_init(timer_driver_init);
module_exit(timer_driver_exit);

MODULE_LICENSE("GPL");


