obj-m += interrupt.o
obj-m += keyboard_interrupt.o
obj-m += keyboard_thread.o
obj-m += tasklet.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
