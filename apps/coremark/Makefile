BOARD ?= root@192.168.5.201
RDIR ?= /usr/local/bin
ITERATIONS ?= 25000
CC ?= cc

SRCS = core_list_join.c core_main.c core_matrix.c core_state.c core_util.c linux/core_portme.c
FLAGS = -O3 -funroll-loops -flto -I. -Ilinux \
        -DFLAGS_STR=\"\" \
        -DSEED_METHOD=SEED_VOLATILE -DMEM_METHOD=MEM_MALLOC \
        -DPERFORMANCE_RUN=1 -DITERATIONS=$(ITERATIONS)

all: coremark-arm coremark-thumb

coremark-arm: $(SRCS)
	$(CC) $(FLAGS) -marm $(SRCS) -o $@

coremark-thumb: $(SRCS)
	$(CC) $(FLAGS) -mthumb $(SRCS) -o $@

deploy: all
	@ssh $(BOARD) 'mkdir -p $(RDIR)'
	@if ssh $(BOARD) 'command -v rsync >/dev/null 2>&1'; then \
		rsync -avz --chmod=D755,F755 coremark-arm coremark-thumb $(BOARD):$(RDIR)/; \
	else \
		tar cf - coremark-arm coremark-thumb | ssh $(BOARD) 'tar --no-same-owner -xf - -C $(RDIR)'; \
	fi

clean:
	rm -f coremark-arm coremark-thumb

.PHONY: all deploy clean
