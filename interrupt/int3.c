#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/interrupt.h>
#include<linux/io.h>
#include<linux/delay.h>

#define KBD_IRQ 1
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

static unsigned char scancode;

/* Top Half */
 static irqreturn_t irq_top(int irq, void *dev_id)
{
	unsigned char status = inb(KBD_STATUS_PORT);
	if(!(status & 0x01))
	return IRQ_NONE;
	scancode = inb(KBD_DATA_PORT);
	return IRQ_WAKE_THREAD;
}
/* thread handler*/

static irqreturn_t irq_thread(int irq, void *dev_id)
{
	int a=10,b=5;
	/*ignore key release (break code)*/
	if(scancode & 0x80)
		return IRQ_HANDLED;
	switch(scancode)
	{
		case 0x1E: //'A' pressed
			   printk(KERN_INFO "A pressed -> addition: %d + %d =%d\n",a,b,a+b);
			   break;

		case 0x30: //'B' pressed
			   printk(KERN_INFO "b prerssed-> sub : %d - %d=%d\n",a,b,a-b);
		           break;
		 default:
			   printk(KERN_INFO "other key (sacn=0x%x)\n", scancode);
			   break;
	}
       return IRQ_HANDLED;
}
/* init */
static int __init kbd_init(void)
{
	printk(KERN_INFO "Driver loaded\n");
	return request_threaded_irq(KBD_IRQ,irq_top,irq_thread,IRQF_SHARED,"kbd_threaded",(void *)irq_thread);
}
/*Exit*/
static void __exit kbd_exit(void)
{
	free_irq(KBD_IRQ, (void *)irq_thread);
	printk(KERN_INFO "driver unloaded");
}
module_init(kbd_init);
module_exit(kbd_exit);

MODULE_LICENSE("GPL");
