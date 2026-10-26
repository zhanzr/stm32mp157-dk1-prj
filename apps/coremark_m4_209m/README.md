# coremark_m4_209m

CoreMark 1.0.1 running on the **Cortex-M4 @ 208.878 MHz** (measured
`ck_mcu`) of the STM32MP157, with results reported to the A7/Linux side over
the remoteproc trace channel. Ported from
`D:\f4-demo\nano-f411\app\coremark_100m` (same optimization flags and 10000
iteration runs).

**Status: verified on hardware.**

## Results (10000 iterations, PERFORMANCE_RUN, MEM_STATIC)

| Run | CoreMark/s |
|-----|------------|
| 1   | 516.289    |
| 2   | 516.316    |
| 3   | 516.316    |
| **avg / best** | **516.30 / 516.32** |

- 516.3 / 208.878 MHz = **2.47 CoreMark/MHz** (SRAM execution, no flash wait
  states)
- A7 comparison (`apps/coremark`): 1966.7 @ 650 MHz = 3.03 CoreMark/MHz —
  the A7's NEON-less in-order A7 core is ~23 % more CoreMark-efficient per
  MHz than the M4, as expected for the integer benchmark.
- Per-run spread is ≤ 0.01 %: the bare-metal M4 has no OS jitter.

## Toolchain decision

The reference project supports gcc / Keil armclang / ST starm-clang:

| Toolchain | Available on this machine? | Used? |
|-----------|---------------------------|-------|
| arm-none-eabi-gcc 15.3.1 (`D:\arm-none-eabi-tc`) | yes | **yes** (kept, per plan) |
| Keil armclang AC6 (`D:\Keil_v5\ARM\ARMCLANG`) | yes (installed) | not integrated — build system kept GCC-only for now |
| ST starm-clang (LLVM 21) | not found | n/a |

Optimization flags (from the reference project, applied to the benchmark
sources only — vendor HAL/OpenAMP objects stay at `-O2` for reliability):

```
-Ofast -ffp-contract=fast -funroll-all-loops
```

## Layout & build

Reuses the vendored CubeMP1 subset from `../a7_m4_hello/m4` (HAL, CMSIS,
OpenAMP/libmetal, startup, linker script, syscalls with `_sbrk`). Adds:

```
m4/
├── main.c            # HAL + IPCC + OpenAMP init (TTY_echo style), N_RUNS coremark rounds
├── core_portme.c/h   # HAL_GetTick 1 ms timing, MEM_STATIC, ITERATIONS=10000, SEED_VOLATILE
├── coremark/         # CoreMark 1.0.1 core sources (from the f4-demo reference)
└── Makefile          # GCC; -Ofast -ffp-contract=fast -funroll-all-loops on bench files only
a7_coremark_manager.c # A7 side: collects scores, computes best/avg/spread
```

```sh
. ./env.sh
cd apps/coremark_m4_209m
make deploy BOARD=root@<board-ip>    # elf -> /lib/firmware, manager -> /usr/local/bin
make start                           # (re)start the M4 with coremark_m4.elf
make run                             # collect 3 results from the A7 side
make stop
```

M4 clock verified live: `ck_mcu` = 208 877 930 Hz
(`/sys/kernel/debug/clk/clk_summary`) — the "209 MHz" M4 grade; the A7 RCC
owns the clocks while Linux runs, so no firmware-side clock config is
possible (SystemClock_Config only runs in engineering boot mode).

## Reporting path & gotchas (all hit during bring-up)

Results are printed by the M4 with `printf` and collected by the A7 manager:

1. **newlib-nano drops `%f`** unless the link adds `-u _printf_float`
   (in `m4/Makefile`).
2. **M4 logs go to the remoteproc trace buffer**
   (`/sys/kernel/debug/remoteproc/remoteproc0/trace0`, needs debugfs). Its
   size is `SYSTEM_TRACE_BUF_SZ` (2048 B in `openamp_log.h`): three result
   blocks (~600 B each) **wrap it** — early results vanish. Raising it to
   16 KB prevented the M4 from booting (remoteproc accepted the start but
   the firmware never ran), so the size stays 2048 and the manager polls
   every 300 ms — fast enough to catch each result line before wrap.
3. **trace0 reads are consuming**: the kernel serves only bytes written
   since the last read, and a line printed across two polls is lost — the
   manager therefore counts occurrences over the freshly-read window and
   accepts repeated scores (identical scores are legitimate: the bare-metal
   M4 has deterministic ms-granular timing).
4. remoteproc stop/start between deployments: `make start` handles
   `stop -> firmware -> start`.

## Known limitation

The benchmark is single-context (`MULTITHREAD 1`) and runs while the A7
side idles — Linux activity (ssh, Weston) does not affect the M4 core clock
but the DDR/AXI bandwidth is shared.
