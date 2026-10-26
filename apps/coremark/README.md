# coremark

EEMBC CoreMark 1.0 (from the local coremark_backup clone, EEMBC sources) built
for the DK1 in two instruction-set variants, run on the board.

## Build (in WSL, from the repository root)

```sh
. ./env.sh
cd apps/coremark
make ITERATIONS=40000        # default 25000; >= 10 s runtime for valid scores
make deploy
```

- `coremark-arm`   : compiled with `-marm`  (A32 instruction set)
- `coremark-thumb` : compiled with `-mthumb` (T32/Thumb-2 instruction set)

Optimization: `-O3 -funroll-loops -flto` on top of the SDK's
`-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard`. Runs use
`PERFORMANCE_RUN=1`, `SEED_METHOD=SEED_VOLATILE`, `MEM_METHOD=MEM_MALLOC`
(heap), fixed `ITERATIONS`.

Note: the `linux` port defaults `SEED_METHOD=SEED_ARG`, which ignores
`-DITERATIONS` at compile time and falls back to auto-calibration
(~30000 iterations here) — the Makefile pins `SEED_VOLATILE` to make the
iteration count compile-time fixed.

## Run

```sh
ssh root@<board-ip> /usr/local/bin/coremark-arm
ssh root@<board-ip> /usr/local/bin/coremark-thumb
```

## Results — STM32MP157AAC (dual Cortex-A7 @ 650 MHz, single context, rev B)

Clock verified on the running board: `ck_mpu` = 650 MHz
(`/sys/kernel/debug/clk/clk_summary`, `scaling_cur_freq` = 650 MHz). The A
part is a 650 MHz grade per the datasheet; the 800 MHz grades are C/F.

Validated runs, 40000 iterations each (both CRC-checked correct):

| Binary         | ISA (our units) | CoreMark/s   | File size | text    | data | bss |
|----------------|-----------------|--------------|-----------|---------|------|-----|
| coremark-arm   | ARM (A32)       | 1966.665028  | 31 740 B  | 22 071  | 316  | 56  |
| coremark-thumb | Thumb-2 (T32)   | 1976.675232  | 27 692 B  | 17 347  | 316  | 56  |

Observations:

- Thumb-2 is ~0.5 % faster and ~25 % smaller — expected on Cortex-A7 where
  Thumb-2 has full performance with higher code density.
- ~1967–1977 CoreMark/s @ 650 MHz ≈ 3.03–3.04 CoreMark/MHz, in line with
  published Cortex-A7 figures.
- Pure single-ISA binaries are not feasible with the SDK: glibc/crt objects
  come with their own ISA attributes (mixed `Tag_ARM_ISA_use` +
  `Tag_THUMB_ISA_use` in both final ELFs). The build flags guarantee the ISA
  of all project compilation units; the measurement differences above show the
  project code dominates the hot path. With LTO, the `core_*` functions are
  also inlined/internalized, so per-symbol ISA checks are not meaningful.
