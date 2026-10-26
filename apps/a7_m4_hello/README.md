# a7_m4_hello

Minimal Cortex-A7 ↔ Cortex-M4 hello: the M4 runs ST's OpenAMP TTY echo
firmware, and the A7 "manager skeleton" exchanges rpmsg messages with it
over `/dev/ttyRPMSG0`.

```
 A7 (Linux)                          M4 (bare metal)
 a7_manager  <--- /dev/ttyRPMSG0 ---> OpenAMP_TTY_echo.elf
 (this app)        rpmsg/virtio       OpenAMP + HAL, IPCC mailboxes
                   over IPCC          built from the vendored CubeMP1 subset
```

**Status: verified on hardware** — remoteproc0 `offline → running`,
`/dev/ttyRPMSG0` created, 3/3 messages echoed by the M4.

## Was the CubeMP1 HAL necessary? Yes.

A working rpmsg firmware needs: HAL (RCC, IPCC, HSEM, GPIO, DMA, PWR, CORTEX,
EXTI + CMSIS), the OpenAMP stack (rpmsg, virtio, remoteproc_virtio), libmetal
(generic port + `template/sys.c`), ST's virtual UART driver, and the BSP LED
file the example uses. All of these were vendored from
`C:\Users\user1\STM32Cube\Repository\STM32Cube_FW_MP1_V1.7.0` into `m4/` in
the exact subset the CubeIDE project compiles (source list extracted from
that project's `.project`/`.cproject`). Layout:

```
m4/
├── Inc, Src                  # example app (echo loop, IPCC, resource table…)
├── Startup/, Application/User/syscalls.c, STM32MP157CAAX_RAM.ld
├── Drivers/{CMSIS, STM32MP1xx_HAL_Driver, BSP/STM32MP15xx_DISCO}
└── Middlewares/OpenAMP/{open-amp, libmetal, virtual_driver}
```

Two port patches (recorded inside the files):
- `Application/User/syscalls.c`: added `_sbrk` (newlib in Arm GNU Toolchain
  ≥ 13 expects the app to provide it; CubeIDE-era newlib used libnosys).
- `STM32MP157CAAX_RAM.ld`: appended `PROVIDE(end = .);` for the heap.

## Build

M4 firmware (needs `arm-none-eabi-gcc`; this machine has 15.3.1 symlinked in
`/usr/local/bin` from `D:\arm-none-eabi-tc`):

```sh
make m4/OpenAMP_TTY_echo.elf     # or: make -C m4
```

A7 manager (uses the OpenSTLinux SDK):

```sh
. ./env.sh && make a7_manager
```

## Deploy & run

```sh
make deploy BOARD=root@<board-ip>   # elf -> /lib/firmware, manager -> /usr/local/bin
make start                          # remoteproc0: load firmware, start M4
make run                            # manager: 3 hello/echo rounds
make stop                           # halt the M4
```

Expected:

```
state: running
/dev/ttyRPMSG0  /dev/ttyRPMSG1
A7 sent : "hello from A7 (1)"
A7 got  : "hello from A7 (1)"
... (3 rounds)
```

## Notes

- The M4 firmware is ST's `OpenAMP_TTY_echo` example verbatim (echo on the
  rpmsg TTY channels 0x400/0x401) — the "conversation" today is echo; the
  manager skeleton (`rpmsg_send`/`rpmsg_recv` in `a7_manager.c`) is the place
  to grow a real command protocol.
- remoteproc0 returns to `offline` after a board reboot; `make start` reboots
  the M4 side.
- Our DK1 DTB patch (I2C5) is unrelated to this app; if you later hand
  I2C5/pins to the M4, flip ownership in the device tree (see the disabled
  `m4@...`/`rproc-srm-dev` nodes) — not needed for rpmsg TTY.
