BOARD ?= root@192.168.5.168
RDIR ?= /usr/local/bin
FWDIR ?= /lib/firmware
CC ?= cc

all: a7_manager m4/OpenAMP_TTY_echo.elf

a7_manager: a7_manager.c
	$(CC) -O2 -Wall -Wextra -o $@ $<

m4/OpenAMP_TTY_echo.elf:
	$(MAKE) -C m4

deploy: a7_manager m4/OpenAMP_TTY_echo.elf
	@ssh $(BOARD) 'mkdir -p $(RDIR)'
	@scp -q m4/OpenAMP_TTY_echo.elf $(BOARD):$(FWDIR)/
	@tar cf - a7_manager | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR) && chmod 755 $(RDIR)/a7_manager'

start:
	@ssh $(BOARD) 'echo OpenAMP_TTY_echo.elf > /sys/class/remoteproc/remoteproc0/firmware; \
	echo start > /sys/class/remoteproc/remoteproc0/state; sleep 2; \
	echo "state: $$(cat /sys/class/remoteproc/remoteproc0/state)"; ls /dev/ttyRPMSG*'

stop:
	@ssh $(BOARD) 'echo stop > /sys/class/remoteproc/remoteproc0/state'

run:
	@ssh -tt $(BOARD) '/usr/local/bin/a7_manager'

clean:
	rm -f a7_manager
	$(MAKE) -C m4 clean

.PHONY: all deploy start stop run clean
