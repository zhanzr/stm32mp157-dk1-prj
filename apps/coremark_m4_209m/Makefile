BOARD ?= root@192.168.5.168
RDIR ?= /usr/local/bin
FWDIR ?= /lib/firmware
FW_NAME ?= coremark_m4.elf
CC ?= cc

all: a7_coremark_manager m4/coremark_m4.elf

a7_coremark_manager: a7_coremark_manager.c
	$(CC) -O2 -Wall -Wextra -o $@ $<

m4/coremark_m4.elf:
	$(MAKE) -C m4

deploy: a7_coremark_manager m4/coremark_m4.elf
	@scp -q m4/coremark_m4.elf $(BOARD):$(FWDIR)/
	@tar cf - a7_coremark_manager | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR) && chmod 755 $(RDIR)/a7_coremark_manager'

start:
	@ssh $(BOARD) 'mount -t debugfs none /sys/kernel/debug 2>/dev/null; \
	echo stop > /sys/class/remoteproc/remoteproc0/state 2>/dev/null; \
	echo $(FW_NAME) > /sys/class/remoteproc/remoteproc0/firmware; \
	echo start > /sys/class/remoteproc/remoteproc0/state; sleep 2; \
	echo "state: $$(cat /sys/class/remoteproc/remoteproc0/state)"'

run:
	@ssh $(BOARD) '/usr/local/bin/a7_coremark_manager'

stop:
	@ssh $(BOARD) 'echo stop > /sys/class/remoteproc/remoteproc0/state'

clean: clean-m4
	rm -f a7_coremark_manager

clean-m4:
	$(MAKE) -C m4 clean

.PHONY: all deploy start run stop clean clean-m4
