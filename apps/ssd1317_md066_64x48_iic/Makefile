BOARD ?= root@192.168.5.168
RDIR ?= /usr/local/bin
CC ?= cc

all: ssd1317_demo

ssd1317_demo: ssd1317_demo.c oledfont.c
	$(CC) -O2 -Wall -Wextra -o $@ ssd1317_demo.c oledfont.c

deploy: ssd1317_demo
	@ssh $(BOARD) 'mkdir -p $(RDIR)'
	@if ssh $(BOARD) 'command -v rsync >/dev/null 2>&1'; then \
		rsync -avz --chmod=D755,F755 ssd1317_demo $(BOARD):$(RDIR)/; \
	else \
		tar cf - ssd1317_demo | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR)'; \
	fi

clean:
	rm -f ssd1317_demo

.PHONY: all deploy clean
