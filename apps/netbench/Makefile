BOARD ?= root@192.168.5.165
RDIR ?= /usr/local/bin
CC ?= cc

all: netbench

netbench: netbench.c
	$(CC) -O2 -Wall -Wextra -o $@ $<

deploy: netbench
	@ssh $(BOARD) 'pkill -x netbench 2>/dev/null; mkdir -p $(RDIR)'
	@if ssh $(BOARD) 'command -v rsync >/dev/null 2>&1'; then \
		rsync -avz --chmod=D755,F755 netbench $(BOARD):$(RDIR)/; \
	else \
		tar cf - netbench | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR)'; \
	fi

serve:
	@ssh $(BOARD) '/usr/local/bin/netbench'

clean:
	rm -f netbench

.PHONY: all deploy serve clean
