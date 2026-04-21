#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static struct kobject *demo_kobj;
static int demo_value;

static ssize_t value_show(struct  kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", demo_value);
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    sscanf(buf, "%d",&demo_value);
    return count;
}

static struct kobj_attribute value_attr = __ATTR(value, 0664, value_show, value_store);

static int __init sysfs_demo_init(void){
    demo_kobj = kobject_create_and_add("sysfs_demo", kernel_kobj);
    if(!demo_kobj){
        return -ENOMEM;
    }
    if (sysfs_create_file(demo_kobj, &value_attr.attr)) {
        kobject_put(demo_kobj);
        return -ENOMEM;
    }
    pr_info("sysfs_demo loaded\n");
    return 0;
}

static void __exit sysfs_demo_exit(void){
    sysfs_remove_file(demo_kobj, &value_attr.attr);
    kobject_put(demo_kobj);
    pr_info("sysfs_demo unloaded\n");
}

module_init(sysfs_demo_init);
module_exit(sysfs_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Simple sysfs demo program");