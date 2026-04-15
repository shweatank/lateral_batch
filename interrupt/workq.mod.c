#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

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



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0x2d88a3ab, "flush_work" },
	{ 0xaef1f20d, "system_wq" },
	{ 0x49733ad6, "queue_work_on" },
	{ 0xd272d446, "__fentry__" },
	{ 0xe8213e80, "_printk" },
	{ 0x67628f51, "msleep" },
	{ 0x4749ded2, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xd272d446,
	0x2d88a3ab,
	0xaef1f20d,
	0x49733ad6,
	0xd272d446,
	0xe8213e80,
	0x67628f51,
	0x4749ded2,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"__x86_return_thunk\0"
	"flush_work\0"
	"system_wq\0"
	"queue_work_on\0"
	"__fentry__\0"
	"_printk\0"
	"msleep\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "8F624A3B0568D2C4B3F6C2C");
