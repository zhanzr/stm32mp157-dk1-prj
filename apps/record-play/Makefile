BOARD ?= root@192.168.5.201
RDIR ?= /usr/local/bin
CC ?= cc

all: record-play

record-play: record_play.c
	$(CC) -O2 -Wall -Wextra -o $@ $< -lasound

deploy: record-play
	@ssh $(BOARD) 'mkdir -p $(RDIR)'
	@if ssh $(BOARD) 'command -v rsync >/dev/null 2>&1'; then \
		rsync -avz --chmod=D755,F755 record-play $(BOARD):$(RDIR)/; \
	else \
		tar cf - record-play | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR)'; \
	fi

clean:
	rm -f record-play

.PHONY: all deploy clean
