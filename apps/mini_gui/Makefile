BOARD ?= root@192.168.5.168
RDIR ?= /usr/local/bin
CC ?= cc
PKGS := gtk+-3.0 gstreamer-1.0 gstreamer-video-1.0

CFLAGS := -O2 -Wall $(shell pkg-config --cflags $(PKGS))
LIBS := $(shell pkg-config --libs $(PKGS))

all: mini_gui

mini_gui: mini_gui.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

deploy: mini_gui
	@tar cf - mini_gui | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR) && chmod 755 $(RDIR)/mini_gui'

kill-cam-holders:
	@ssh $(BOARD) 'for pid in $$(ls /proc | grep -E "^[0-9]+$$"); do \
	  for l in /proc/$$pid/fd/*; do \
	    t=$$(readlink $$l 2>/dev/null); \
	    case "$$t" in /dev/video*) echo "killing holder $$pid ($$(cat /proc/$$pid/comm 2>/dev/null)) holding $$t"; kill -9 $$pid;; esac; \
	    break; \
	  done; \
	done; exit 0'

run: kill-cam-holders
	@ssh $(BOARD) 'pkill -9 mini_gui 2>/dev/null; sleep 0.3; \
	usermod -aG video,render weston 2>/dev/null; \
	runuser -u weston -- sh -c "export XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0; nohup $(RDIR)/mini_gui >/tmp/mini_gui.log 2>&1 &"; \
	sleep 2; pgrep -a mini_gui || tail -6 /tmp/mini_gui.log'

stop:
	@ssh $(BOARD) 'pkill mini_gui'

clean:
	rm -f mini_gui

.PHONY: all deploy kill-cam-holders run stop clean
