#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/interrupt.h>
 
#define DRIVER_NAME "irq_demo_driver"
#define IRQ_NUM 1

static int irq_counter = 0;
//this runs in interrrupt context(top half)
static irqreturn_t irq_demo_isr(int irq, void *dev_id)
{
	irq_counter++;
	pr_info("%s,interrrupt received! IRQ=%D count=%d\n",DRIVER_NAME,irq, irq_counter);

	//this interrupt was meant for us
	return IRQ_HANDLED;
}
static int __init irq_demo_init(void)
{
	int ret;
	pr_info("%s:Initializing\n", DRIVER_NAME);
	/* 
	 * request_irq argumennts:
	 * irq   ->IRQ number
	 * handler  -> ISR FUNCTION
	 * FLAGS  ->IRQF_SHARED ALLOWS SHARING
	 * NAME  ->visible in /proc/interrups
	 * dev_id ->uniq identifier
	 */
	ret = request_irq(IRQ_NUM,irq_demo_isr,IRQF_SHARED,DRIVER_NAME,(void *)irq_demo_isr);
	if(ret)
	{
		pr_err("%s: failed to request IRQ %d\n", DRIVER_NAME, IRQ_NUM);
		return ret;
	}
	pr_info("%s: IRQ %d registered successfully\n",
			DRIVER_NAME, IRQ_NUM);
	return 0;
}
static void __exit irq_demo_exit(void)
{
	pr_info("%s: cleaning up\n", DRIVER_NAME);
	/*
	 * free_irq must match
	 * same IRQ number
	 * same dev_id pointer
	 */
	free_irq(IRQ_NUM,(void *)irq_demo_isr);
	pr_info("%s: IRQ freed\n", DRIVER_NAME);
}

module_init(irq_demo_init);
module_exit(irq_demo_exit);

MODULE_LICENSE("GPL");

	
