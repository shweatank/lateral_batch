#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/interrupt.h>
#include<linux/io.h>

#define KBD_IRQ 1
#define KBD_DATA_PORT 0X60

static irqreturn_t keyboard_interrupt(int irq, void *dev_id)
{
	unsigned char scancode;
	//read scan code from keyboard data port
	scancode = inb(KBD_DATA_PORT);
	printk(KERN_INFO "Keyboard IRQ: Scan code = 0x%x\n", scancode);
	return IRQ_HANDLED;
}

static int __init kbd_driver_init(void)
{
	int result;
	printk(KERN_INFO "Keyboard deiver loaded\n");
	//requested IRQ 1(keyboar)
	result = request_irq(KBD_IRQ, keyboard_interrupt,IRQF_SHARED,"kbd_driver", (void *)(keyboard_interrupt));
	if(result)
	{
		printk(KERN_ERR "cannot register IRQ 1\n");
		return result;
	}
	return 0;
}
static void __exit kbd_driver_exit(void)
{
	free_irq(KBD_IRQ, (void *)(keyboard_interrupt));
	printk(KERN_INFO "Keyboard driver unloaded\n");
}
module_init(kbd_driver_init);
module_exit(kbd_driver_exit);

MODULE_LICENSE("GPL");

