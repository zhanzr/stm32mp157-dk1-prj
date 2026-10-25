# mp157-dk1-prj

Application repository for the STM32MP157A-DK1 discovery board running
OpenSTLinux 6.6-yocto-scarthgap-mpu-v26.06.10 (st-image-weston).

## Board

STM32MP157A-DK1 discovery kit (MB1272 board):

![STM32MP157A-DK1 board](board_images/board_0.png)

![board top](board_images/board_1.png)

![board bottom](board_images/board_2.png)

Connector close-ups live in [board_images/](board_images/) (DSI, HDMI, Ethernet,
TF card, USB host, Arduino, audio jack, GPIO connector, boot switches, LCD
daughter board).

## Board facts used by this repo

| LED  | Color  | GPIO | Polarity    | Note |
|------|--------|------|-------------|------|
| LD5  | green  | PA14 | active-low  | shares pin with USER1 button B3 |
| LD6  | red    | PA13 | active-low  | shares pin with USER2 button B4 |
| LD7  | orange | PH7  | active-high | free for user apps |
| LD8  | blue   | PD11 | active-high | **reserved by kernel** (heartbeat trigger), do not use |

PA13/PA14 may be held by the kernel `gpio-keys` driver (user buttons B3/B4).
If a line is busy, `blink_hello` reports it and keeps blinking the remaining
LEDs. To reclaim LD5/LD6 permanently, disable the buttons in the board
devicetree and reflash `bootfs`.

## Repository layout

```
mp157-dk1-prj/
├── env.sh                  # sources the OpenSTLinux cross-SDK (WSL)
└── apps/
    └── blink_hello/        # first application: blink LD5/LD6/LD7
        ├── Makefile
        └── blink_hello.c   # pure GPIO chardev UAPI (linux/gpio.h), no libgpiod dependency
```

New applications go into `apps/<name>/` with their own Makefile following the
same pattern.

## Build (in WSL)

```sh
cd ~/mp157-dk1-prj
. ./env.sh          # sets CC/CFLAGS/sysroot for the board (arm-ostl-linux-gnueabi)
cd apps/blink_hello
make
```

Result: ARM 32-bit binary (cortex-a7, hard-float), linked against the target
sysroot.

## Host/board split

This repository exists **only on the host** (WSL). The board never receives
sources, Makefiles or the SDK — only compiled binaries are transferred.

## Deploy (binary transfer)

Binaries live in `/usr/local/bin` on the board — that path is on the **userfs**
partition and survives rootfs reflashes, unlike `/usr/bin` which belongs to the
rootfs image and gets replaced on every reflash.

Default board address: `root@192.168.5.201` (override with `make deploy BOARD=root@<ip>`).

**Preferred: rsync** (`make deploy` in `apps/blink_hello` — auto-detects, see below)

```sh
rsync -avz --chmod=D755,F755 blink_hello root@192.168.5.201:/usr/local/bin/
```

- delta transfer: redeploys after a small code change send only the changed
  bytes, not the whole file
- preserves the executable bit and timestamps

Requirement: rsync on **both** ends. The stock OpenSTLinux weston image does
**not** include rsync (verified on this board) — `make deploy` detects this and
falls back to `tar` over ssh automatically, which needs nothing beyond sshd.
To get real rsync, add `rsync` to the image in the Yocto build
(`IMAGE_INSTALL:append = " rsync"`), then reflash.

**Fallback: scp** (always available, ships with sshd)

```sh
scp blink_hello root@192.168.5.201:/usr/local/bin/
```

**Other options**

| Method | When it beats rsync/scp |
|---|---|
| `tar c blink_hello \| ssh root@... 'tar x -C /usr/local/bin'` | no requirements beyond ssh, keeps perms |
| `python3 -m http.server` on host + `wget` on board | pull model, no board-side push access needed |
| NFS export of `apps/<name>/` mounted on the board | heavy iteration: run directly from host FS, zero transfer |

## Run

```sh
ssh root@192.168.5.201 /usr/local/bin/blink_hello
```

(full path: the stock image PATH does not include /usr/local/bin)

Ctrl-C stops it and turns the LEDs off.

## Default addressing

- Board (end0): 192.168.5.201/24, PC: 192.168.5.200/24, direct Ethernet cable
- USB cable link (optional): board usb0 = 192.168.7.1, PC gets 192.168.7.2
