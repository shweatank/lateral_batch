#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/spinlock.h>



#define BLINK_COUNT  5
#define ON_MS        100
#define OFF_MS       100
#define POLL_MS      2

struct td_ctx {
    struct device *dev;
    struct gpio_desc *in;
    struct gpio_desc *out;

    struct task_struct *poll_th;

    struct hrtimer blink_timer;
    ktime_t on_period;
    ktime_t off_period;

    spinlock_t lock;
    int blinks_left;     /* number of ON transitions remaining */
    bool led_on;
};

static enum hrtimer_restart td_blink_timer_fn(struct hrtimer *t)
{
    struct td_ctx *ctx = container_of(t, struct td_ctx, blink_timer);
    unsigned long flags;
    ktime_t next;

    spin_lock_irqsave(&ctx->lock, flags);

    if (ctx->blinks_left <= 0 && !ctx->led_on) {
        /* done: ensure LED is OFF */
        gpiod_set_value(ctx->out, 0);
        spin_unlock_irqrestore(&ctx->lock, flags);
        return HRTIMER_NORESTART;
    }

    if (!ctx->led_on) {
        /* turn ON */
        ctx->led_on = true;
        gpiod_set_value(ctx->out, 1);
        next = ctx->on_period;
    } else {
        /* turn OFF and count one blink completed */
        ctx->led_on = false;
        gpiod_set_value(ctx->out, 0);
        ctx->blinks_left--;
        next = ctx->off_period;
    }

    spin_unlock_irqrestore(&ctx->lock, flags);

    hrtimer_forward_now(&ctx->blink_timer, next);
    return HRTIMER_RESTART;
}

static void td_start_blink(struct td_ctx *ctx)
{
    unsigned long flags;

    spin_lock_irqsave(&ctx->lock, flags);

    /* if already blinking, restart sequence */
    ctx->blinks_left = BLINK_COUNT;
    ctx->led_on = false;
    gpiod_set_value(ctx->out, 0);

    hrtimer_start(&ctx->blink_timer, ctx->off_period, HRTIMER_MODE_REL);

    spin_unlock_irqrestore(&ctx->lock, flags);
}

static int td_poll_thread(void *arg)
{
    struct td_ctx *ctx = arg;
    int prev = 0;

    dev_info(ctx->dev, "polling started: trigger on GPIO input rising edge\n");

    while (!kthread_should_stop()) {
        int cur = gpiod_get_value(ctx->in);

        if (!prev && cur) {
            dev_info(ctx->dev, "input rising edge -> blink\n");
            td_start_blink(ctx);
        }

        prev = cur;
        msleep(POLL_MS);
    }

    dev_info(ctx->dev, "polling stopped\n");
    return 0;
}

static int td_probe(struct platform_device *pdev)
{
    struct td_ctx *ctx;

    ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    ctx->dev = &pdev->dev;
    platform_set_drvdata(pdev, ctx);

    /*
     * DT properties:
     *   in-gpios  (input)
     *   out-gpios (output)
     */
    ctx->in = devm_gpiod_get(&pdev->dev, "in", GPIOD_IN);
    if (IS_ERR(ctx->in))
        return dev_err_probe(&pdev->dev, PTR_ERR(ctx->in), "failed to get in-gpios\n");

    ctx->out = devm_gpiod_get(&pdev->dev, "out", GPIOD_OUT_LOW);
    if (IS_ERR(ctx->out))
        return dev_err_probe(&pdev->dev, PTR_ERR(ctx->out), "failed to get out-gpios\n");

    spin_lock_init(&ctx->lock);

    /* Setup hrtimer */
    ctx->on_period  = ktime_set(0, ON_MS  * 1000 * 1000ULL);
    ctx->off_period = ktime_set(0, OFF_MS * 1000 * 1000ULL);
    hrtimer_init(&ctx->blink_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    ctx->blink_timer.function = td_blink_timer_fn;

    /* Start polling thread */
    ctx->poll_th = kthread_run(td_poll_thread, ctx, "td_gpio_poll");
    if (IS_ERR(ctx->poll_th))
        return dev_err_probe(&pdev->dev, PTR_ERR(ctx->poll_th), "kthread_run failed\n");

    dev_info(&pdev->dev, "probed: DT GPIO input->blink output ready\n");
    return 0;
}

static int td_remove(struct platform_device *pdev)
{
    struct td_ctx *ctx = platform_get_drvdata(pdev);

    if (ctx->poll_th)
        kthread_stop(ctx->poll_th);

    hrtimer_cancel(&ctx->blink_timer);

    /* LED off */
    gpiod_set_value(ctx->out, 0);

    dev_info(&pdev->dev, "removed\n");
    return 0;
}

static const struct of_device_id td_of_match[] = {
    { .compatible = "udaybhaskar,td-gpio-blink" },
    { }
};
MODULE_DEVICE_TABLE(of, td_of_match);

static struct platform_driver td_driver = {
    .probe  = td_probe,
    .remove = td_remove,
    .driver = {
        .name           = "td-gpio-blink",
        .of_match_table = td_of_match,
    },
};

module_platform_driver(td_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("DT GPIO17 input -> blink GPIO27 output");
MODULE_VERSION("1.0");