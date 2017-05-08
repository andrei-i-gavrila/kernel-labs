# Two passes are done on this file: one by the manually invoked 'make', other
# by the kbuild system started by $(MAKE) -C $(KERNELDIR) M=...

ifneq ($(KERNELRELEASE),)

# This is the kbuild part

$(info Building with KERNELRELEASE = ${KERNELRELEASE})
obj-m := morse.o

else

# These targets are used by the initial 'make' command

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	 rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c modules.order Module.symvers

endif
