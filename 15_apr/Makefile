obj-m += sysfs.o
obj-m += calculator_using_sysfs.o
obj-m += procfs.o
obj-m += calculator_procfs.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
