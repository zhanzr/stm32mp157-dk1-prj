# blink_hello

Blinks the user LEDs LD5/LD6/LD7 using the GPIO character device UAPI
(`linux/gpio.h` v2 ioctls) — no libgpiod dependency.

## Board facts used by this app

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

## Build (in WSL, from the repository root)

```sh
. ./env.sh
cd apps/blink_hello
make
```

`env.sh` loads the OpenSTLinux cross toolchain (`$CC` is the full compiler
command line, used unquoted by the Makefile).

## Deploy

```sh
make deploy                    # BOARD=root@<board-ip> to override the default
```

- deploys to `/usr/local/bin/` on the board
- uses **rsync** if installed on the board (delta transfer, stock image does
  not ship it), otherwise falls back to **tar over ssh** automatically
- `/usr/local/bin` lives on the userfs partition: it survives rootfs reflashes,
  unlike `/usr/bin`
- the stock image PATH does not include `/usr/local/bin` — run it by full path

## Run

```sh
ssh root@<board-ip> /usr/local/bin/blink_hello
```

Ctrl-C stops it and turns the LEDs off.
