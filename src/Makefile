# obj-m is a list of what kernel modules to build.  The .o and other
# objects will be automatically built from the corresponding .c file -
# no need to list the source files explicitly.

# toto-objs := hello_printk.o lib_printk.o
# tata-objs := hello_printk2.o lib_printk.o

# obj-m :=  toto.o
# obj-m +=  tata.o

#unable yet
#snd-lx6464es-objs := lx6464es.o lx_core.o lxcommon.o
#obj-m = snd-lx6464es.o

snd-lxmadi-objs := lxmadi.o  lxcommon.o lx_core.o
obj-m += snd-lxmadi.o

snd-lxip-objs := lxip.o  lxcommon.o lx_core.o
obj-m += snd-lxip.o

KVERSION ?= $(shell uname -r)

# KDIR is the location of the kernel source.  The current standard is
# to link to the associated source tree from the directory containing
# the compiled modules.
KDIR  := /lib/modules/$(KVERSION)/build

# PWD is the current working directory and the location of our module
# source files.
PWD   := $(shell pwd)

MODULES_DIR := /lib/modules/$(KVERSION)/digigram
# default is the default make target.  The rule here says to run make
# with a working directory of the directory containing the kernel
# source and compile only the modules in the PWD (local) directory.
all: clean default
default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	


install:
	test -d $(MODULES_DIR) || mkdir $(MODULES_DIR)
	cp *.ko $(MODULES_DIR)
	depmod -ae

	
