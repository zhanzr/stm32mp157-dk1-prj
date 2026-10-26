# blink_hello

Blinks the user LEDs LD5/LD6/LD7 and periodically samples the on-board
sensors, using the GPIO character device UAPI (`linux/gpio.h` v2 ioctls) and
sysfs (`/sys/class/thermal`, `/sys/bus/iio`) — no library dependencies.

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

## Sensors reported (verified on this board)

Temperature — the **DTS digital temperature sensor** (DS 3.24), exposed by the
`stm32_thermal` driver as `thermal_zone0 [cpu-thermal]`, printed in °C.

ADC — the two ADC instances are exposed via IIO, sampled at **16-bit
oversampling with VREF+ = 2.5 V from the internal VREFBUF** (confirmed by
`in_voltage_scale` = 2500/65536 and the enabled `vrefbuf@50025000` DT node):

| Instance | ch   | sysfs            | Reading on this board | DT label  |
|----------|------|------------------|-----------------------|-----------|
| ADC2     | 13   | `in_voltage13_input` (processed mV) | ~2.52 V (≈ VREFBUF rail) | vrefint |
| ADC2     | 14   | `in_voltage14_raw` × scale | ~1.18 V (matches the 1.2 V core) | vddcore |
| ADC1+ADC2| 18   | `in_voltage18_raw` | 0 (dead on this board) | -         |
| ADC1+ADC2| 19   | `in_voltage19_raw` | ~1.68 V | -         |

Notes:

- The analog temperature sensor VTS described in DS 3.23 (ADC2_INP12) is
  **not wired in this BSP**: there is no channel@12 in the device tree or sysfs,
  and the factory TS calibration OTP words (92/94) read as zero on this chip.
  Use the DTS thermal zone for temperature.
- DS 3.23's channel numbering differs from this BSP's DT (labels vrefint@13 /
  vddcore@14); trust sysfs at runtime — the app enumerates whatever is exposed.
- Channel 19 shows ~1.68 V on both ADC instances but is unlabeled in this
  image; identify before relying on it.

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
