obj-m += smart_driver.o

# Fetch kernel build path
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: kernel_module user_app

kernel_module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user_app: user_app.c smart_ioctl.h
	gcc -Wall -Wextra -O2 -pthread -o user_app user_app.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f user_app
