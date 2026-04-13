#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/interrupt.h>
#include<linux/io.h>
#include<linux/delay.h>

#define DRIVER_NAME     "irq_calc"
#define IRQ_NUM           1           



static void __exit irq_calc_exit(void)
{
    free_irq(IRQ_NUM, (void *)irq_calc_thread);
    pr_info("%s: IRQ %d Unloaded\n", DRIVER_NAME, IRQ_NUM);
}

static int __init irq_calc_init(void)
{
    int ret;

    pr_info("%s: Loaded \n", DRIVER_NAME);

    ret = request_threaded_irq(
            IRQ_NUM,
            irq_calc_top,     
            irq_calc_thread, 
            IRQF_SHARED,     
            DRIVER_NAME,
            (void *)irq_calc_thread   
    );

    if (ret) {
        pr_err("%s: request_threaded_irq failed: %d\n", DRIVER_NAME, ret);
        return ret;
    }

    pr_info("%s: registered on IRQ %d\n", DRIVER_NAME, IRQ_NUM);
    pr_info("%s: press A=add  S=sub  M=mul  D=div \n",
            DRIVER_NAME);
    return 0;
}

static irqreturn_t irq_calc_thread(int irq, void *dev_id)
{
    u8 sc = last_scancode;   

    do_calc(sc);
    return IRQ_HANDLED;
}
