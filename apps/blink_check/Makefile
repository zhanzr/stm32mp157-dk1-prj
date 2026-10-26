BOARD ?= root@192.168.5.168
RDIR ?= /usr/local/bin
CC ?= cc

all: blink_check

blink_check: blink_check.c
	$(CC) -O2 -Wall -Wextra -o $@ $<

i2cpin_probe: i2cpin_probe.c
	$(CC) -O2 -Wall -Wextra -o $@ $<

deploy: blink_check
	@ssh $(BOARD) 'mkdir -p $(RDIR)'
	@if ssh $(BOARD) 'command -v rsync >/dev/null 2>&1'; then \
		rsync -avz --chmod=D755,F755 blink_check $(BOARD):$(RDIR)/; \
	else \
		tar cf - blink_check | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR)'; \
	fi

clean:
	rm -f blink_check i2cpin_probe

.PHONY: all deploy clean
