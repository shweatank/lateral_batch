# ---------------------------------------------------------------------
# Makefile for the smart_device project
#
# Targets:
#   make            - builds the kernel module AND the user app
#   make module     - kernel module only (smart_driver.ko)
#   make user       - user-space app only (user_app)
#   make clean      - remove all build artefacts
#   make load       - shorthand for ./load_driver.sh
#   make unload     - shorthand for ./unload_driver.sh
#   make test       - run test.sh (must be root)
#
# Kernel-side build uses Kbuild. The `obj-m += smart_driver.o` line is
# read by the kernel build system when we recurse into /lib/modules.
# ---------------------------------------------------------------------

obj-m += smart_driver.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)
CC   ?= cc
CFLAGS_USER := -Wall -Wextra -O2 -pthread

.PHONY: all module user clean load unload test

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: user_app

user_app: user_app.c smart_ioctl.h
	$(CC) $(CFLAGS_USER) -o $@ user_app.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f user_app

load:
	sudo ./load_driver.sh

unload:
	sudo ./unload_driver.sh

test:
	sudo ./test.sh
