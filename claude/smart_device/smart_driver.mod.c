#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_MITIGATION_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x89940875, "mutex_lock_interruptible" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0x1d24c881, "___ratelimit" },
	{ 0x54b1fac6, "__ubsan_handle_load_invalid_value" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0xc4f0da12, "ktime_get_with_offset" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x7ab1f0cb, "pcpu_hot" },
	{ 0x4c03a563, "random_kmalloc_seed" },
	{ 0x1004e946, "kmalloc_caches" },
	{ 0xbf55f104, "kmalloc_trace" },
	{ 0xcefb0c9f, "__mutex_init" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x81daace6, "cdev_init" },
	{ 0x6a7b86fa, "cdev_add" },
	{ 0xa4bf0f83, "class_create" },
	{ 0x31ba63ba, "device_create" },
	{ 0xe8075284, "default_llseek" },
	{ 0xe9b868f, "device_destroy" },
	{ 0xeea0e0d, "class_destroy" },
	{ 0x67d01ca4, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x37a0cba, "kfree" },
	{ 0x122c3a7e, "_printk" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x73776b79, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "FE0E53A27862334FB16A41F");
