#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#define DRIVER_NAME "irq_driver"
#define IRQ_NUM 1 //keyboard irq on x86

static int irq_counter=0;
static irqreturn_t irq_demo_isr(int irq, void *dev_id){
    irq_counter++;
    pr_info("%s: Interrupt received! IRQ=%d Count=%d\n", DRIVER_NAME, irq, irq_counter);
    return IRQ_HANDLED;
}

static int __init irq_demo_init(void){
    int ret;
    pr_info("%s: Initializing\n",DRIVER_NAME);
    ret =request_irq(IRQ_NUM, irq_demo_isr,IRQF_SHARED, DRIVER_NAME,(void *)irq_demo_isr);
    if(ret){
        pr_err("%s: Failed to request IRQ %d\n", DRIVER_NAME,IRQ_NUM);
        return ret;
    }
    pr_info("%s: IRQ %d registered successfully\n", DRIVER_NAME, IRQ_NUM);
    return 0;
}

static void __exit irq_demo_exit(void){
    pr_info("%s: Cleaning up\n",DRIVER_NAME);
    free_irq(IRQ_NUM, (void *)irq_demo_isr);
    pr_info("%s: IRQ freed\n",DRIVER_NAME);
}

module_init(irq_demo_init);
module_exit(irq_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Interrupt Demo Driver");