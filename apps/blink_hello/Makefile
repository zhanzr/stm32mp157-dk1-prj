BOARD ?= root@192.168.5.201
RDIR ?= /usr/local/bin

CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra
PREFIX ?= /usr/bin

all: blink_hello

blink_hello: blink_hello.c
	$(CC) $(CFLAGS) -o $@ $<

install: blink_hello
	install -D -m 755 blink_hello $(DESTDIR)$(PREFIX)/blink_hello

clean:
	rm -f blink_hello

deploy: blink_hello
	@ssh $(BOARD) 'mkdir -p $(RDIR)'
	@if ssh $(BOARD) 'command -v rsync >/dev/null 2>&1'; then \
		echo ">> deploying via rsync (delta)"; \
		rsync -avz --chmod=D755,F755 blink_hello $(BOARD):$(RDIR)/; \
	else \
		echo ">> board has no rsync, deploying via tar over ssh"; \
		tar cf - blink_hello | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR)'; \
	fi

.PHONY: all install clean deploy
