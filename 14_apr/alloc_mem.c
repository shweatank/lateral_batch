#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>    // For kmalloc/kfree
#include <linux/vmalloc.h> // For vmalloc/vfree

#define KMALLOC_SIZE 1024
#define VMALLOC_SIZE (4 * PAGE_SIZE) 

static char *kmalloc_ptr;
static char *vmalloc_ptr;

static int __init alloc_mem_init(void)
{
    pr_info("alloc_mem: Module initialized\n");

    kmalloc_ptr = kmalloc(KMALLOC_SIZE, GFP_KERNEL);
    if (!kmalloc_ptr) {
        pr_err("alloc_mem: kmalloc failed to allocate memory!\n");
        return -ENOMEM;
    }
    pr_info("alloc_mem: kmalloc successfully allocated %d bytes at %p\n", KMALLOC_SIZE, kmalloc_ptr);


    vmalloc_ptr = vmalloc(VMALLOC_SIZE);
    if (!vmalloc_ptr) {
        pr_err("alloc_mem: vmalloc failed to allocate memory!\n");
        kfree(kmalloc_ptr); 
        return -ENOMEM;
    }
    pr_info("alloc_mem: vmalloc successfully allocated %lu bytes at %p\n", VMALLOC_SIZE, vmalloc_ptr);

    return 0;
}

static void __exit alloc_mem_exit(void)
{
    pr_info("alloc_mem: Module exiting, freeing memory...\n");

    if (kmalloc_ptr) {
        kfree(kmalloc_ptr);
        pr_info("alloc_mem: kmalloc memory freed\n");
    }

    if (vmalloc_ptr) {
        vfree(vmalloc_ptr);
        pr_info("alloc_mem: vmalloc memory freed\n");
    }
}

module_init(alloc_mem_init);
module_exit(alloc_mem_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("udayabhaskar");
MODULE_DESCRIPTION("Simple kmalloc and vmalloc demonstration");
