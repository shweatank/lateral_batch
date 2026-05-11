obj-m += calc_driver.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all: module user test

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: calc_user.c calc_ioctl.h
	$(CC) -o calc_user calc_user.c

test: calc_test.c calc_ioctl.h
	$(CC) -Wall -O2 -o calc_test calc_test.c

# Convenience: load module, fix perms, run tests, unload.
# Run with: sudo make run-test
run-test: module test
	-sudo rmmod calc_driver 2>/dev/null
	sudo insmod calc_driver.ko
	sudo chmod a+rw /dev/calc
	./calc_test ; rc=$$? ; sudo rmmod calc_driver ; exit $$rc

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f calc_user calc_test
