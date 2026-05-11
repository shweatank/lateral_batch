# ============================================================================
# Makefile for smart_device project
# ----------------------------------------------------------------------------
# Builds:
#   * smart_driver.ko  — loadable kernel module
#   * smart_app        — user-space companion binary
#
# Usage:
#   make            -> build everything
#   make module     -> build only the kernel module
#   make app        -> build only the user-space app
#   make clean      -> remove all build artefacts
#   make load       -> insmod the driver (sudo required)
#   make unload     -> rmmod the driver
#   make test       -> run the automated test script
# ============================================================================

# --- Kernel module --------------------------------------------------------
# obj-m tells kbuild "produce smart_driver.ko from smart_driver.o".
obj-m := smart_driver.o

# Path to the running kernel's build tree. Required by kbuild.
KDIR  ?= /lib/modules/$(shell uname -r)/build

# Current directory (where this Makefile lives).
PWD   := $(shell pwd)

# kbuild's M= argument cannot tolerate spaces in the path. If the
# project lives under a directory like "/home/hp/Mamatha/claude folder",
# build through a symlink in /tmp instead. The .ko is copied back.
BUILDDIR := $(if $(findstring $() ,$(PWD)),/tmp/smart_device_build,$(PWD))

# --- Compiler -------------------------------------------------------------
# Kernel 6.x needs gcc-12+ (uses -ftrivial-auto-var-init=zero). On Ubuntu
# 22.04 the default cc/gcc is still 11, so pick gcc-12 if it exists.
# Override on the command line with `make CC=...`.
ifneq ($(shell command -v gcc-12 2>/dev/null),)
CC      := gcc-12
else
CC      := gcc
endif

# --- User-space app -------------------------------------------------------
CFLAGS  := -Wall -Wextra -O2 -pthread
APP     := smart_app
APP_SRC := user_app.c

# --- Phony targets --------------------------------------------------------
.PHONY: all module app clean load unload reload test help

all: module app

# Set up the symlink (only if the real path has spaces).
$(BUILDDIR):
	ln -sfn "$(PWD)" $(BUILDDIR)

# Kernel module: delegate to the kernel's build system, via the
# space-free build directory.
module: $(BUILDDIR)
	$(MAKE) -C $(KDIR) M=$(BUILDDIR) modules
	@if [ "$(BUILDDIR)" != "$(PWD)" ]; then \
	    cp -f $(BUILDDIR)/smart_driver.ko "$(PWD)/" 2>/dev/null || true; \
	fi

# User app: a single gcc invocation. We include "." so smart_ioctl.h
# is found without -I tricks.
app: $(APP)

$(APP): $(APP_SRC) smart_ioctl.h
	$(CC) $(CFLAGS) $(APP_SRC) -o $(APP)

# Clean both halves.
clean: $(BUILDDIR)
	$(MAKE) -C $(KDIR) M=$(BUILDDIR) clean
	rm -f $(APP) smart_driver.ko
	@if [ "$(BUILDDIR)" != "$(PWD)" ]; then rm -f $(BUILDDIR); fi

# Convenience wrappers — these need root.
load:
	@if lsmod | grep -q smart_driver; then \
	    echo "smart_driver already loaded"; \
	else \
	    sudo insmod smart_driver.ko && echo "loaded"; \
	fi
	@sudo dmesg | tail -n 5

unload:
	@if lsmod | grep -q smart_driver; then \
	    sudo rmmod smart_driver && echo "removed"; \
	else \
	    echo "smart_driver not loaded"; \
	fi

reload: unload load

test: all
	@./test.sh

help:
	@echo "Targets: all module app clean load unload reload test"
