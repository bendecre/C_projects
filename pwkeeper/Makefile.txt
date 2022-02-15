# Basic Makefile to pull in kernel's KBuild to build an out-of-tree
# kernel module

KDIR ?= /lib/modules/$(shell uname -r)/build
MODNAME = pwkeeper

all: modules $(MODNAME)-test

$(MODNAME)-test: $(MODNAME)-test.o cs421net.o
	gcc --std=c99 -Wall -O2 -pthread -o $@ $^ -lm

$(MODNAME)-test.o: $(MODNAME)-test.c cs421net.h
cs421net.o: cs421net.c cs421net.h

%.o: %.c
	gcc --std=c99 -Wall -O2 -c -o $@ $<

modules xt_cs421net.ko:
	$(MAKE) -C $(KDIR) M=$$PWD $@

clean:
	$(MAKE) -C $(KDIR) M=$$PWD $@
	-rm $(MODNAME)-test

