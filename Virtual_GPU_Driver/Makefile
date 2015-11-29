#
# Makefile for kyouko2_module
#

obj-m += kyouko2_module.o

default:
	$(MAKE) -C /usr/src/linux M=$(PWD) modules

clean:
	rm *.ko
	rm *.o
	rm *.mod.c

