obj-m += calc_driver.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: calc_user.c calc_ioctl.h
	$(CC) -Wall -O2 calc_user.c -o calc_user

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f calc_user

.PHONY: all module user clean
