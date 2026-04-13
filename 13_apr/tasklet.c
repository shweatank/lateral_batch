#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#define KEYBOARD_IRQ 1
 
static void keyboard_tasklet_fn(struct tasklet_struct *t);

DECLARE_TASKLET(keyboard_tasklet, keyboard_tasklet_fn);

static void keyboard_tasklet_fn(struct tasklet_struct *t)
{
    pr_info("tasklet: bottom half executed\n");
}

static irqreturn_t keyboard_irq_handler(int irq, void *dev_id){
    pr_info("irq: keyboard interrupt occurred\n");
    tasklet_schedule(&keyboard_tasklet);
    return IRQ_HANDLED;
}

static int __init tasklet_irq_init(void){
    int ret;
    ret= request_irq(KEYBOARD_IRQ, keyboard_irq_handler, IRQF_SHARED, "kbd_tasklet", (void *)keyboard_irq_handler);
    if(ret){
        pr_err("Failed to request IRQ %d\n", KEYBOARD_IRQ);
        return ret;
    }
    pr_info("tasklet module loaded\n");
    return 0;
}

static void __exit tasklet_irq_exit(void){
    free_irq(KEYBOARD_IRQ, (void *)keyboard_irq_handler);
    tasklet_kill(&keyboard_tasklet);
    pr_info("tasklet module unloaded\n");
}

module_init(tasklet_irq_init);
module_exit(tasklet_irq_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");