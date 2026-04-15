#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/interrupt.h>

#define KEYBOARD_IRQ 1

MODULE_LICENSE("GPL");
//forward declaration -new signature
static void keyboard_tasklet_fn(struct tasklet_struct *t);
//tasklet object
DECLARE_TASKLET(keyboard_tasklet, keyboard_tasklet_fn);
//tasklet (bottom half)
static void keyboard_tasklet_fn(struct tasklet_struct *t)
{
	pr_info("tasklet: bottom half executed\n");
}
//IRQ handler (top half)
static irqreturn_t keyboard_irq_handler(int irq, void *dev_id)
{
	pr_info("irq keyboard interrupt occured\n");
	tasklet_schedule(&keyboard_tasklet);
	return IRQ_HANDLED;
}
//module init
static int __init tasklet_irq_init(void)
{
	int ret;
	ret = request_irq(KEYBOARD_IRQ,keyboard_irq_handler,IRQF_SHARED,"kbd_tasklet",(void *)keyboard_irq_handler);
	if(ret)
	{
		pr_err("failedto request IRQ %d\n",KEYBOARD_IRQ);
		return ret;
	}
	pr_info("tasklet module loaded\n");
	return 0;
}

//module exit
static void __exit tasklet_irq_exit(void)
{
	tasklet_kill(&keyboard_tasklet);
	free_irq(KEYBOARD_IRQ, (void *)keyboard_irq_handler);
	pr_info("tasklet module unloaded\n");
}

module_init(tasklet_irq_init);
module_exit(tasklet_irq_exit);


