# ssd1317_md066_64x48_iic

SSD1317 "MD066" 0.66" 64x48 mono OLED (I2C) demo for the DK1 — a Linux port
of the f425-start bare-metal app
`D:\at32-start-f407\f425-start\bare\ssd1317_md066_64x48_iic`
(itself ported from the Zhongjingyuan vendor STM32F103 example). Same init
sequence, same `Set_Pos` window mapping (page `0xB2+y`, column `x+0x20`),
same fonts, same demo pages and animated FPS sweep; pacing LEDs LD5/LD6/LD7
mirror the AT32 LD2/LD3/LD4 toggles (200/400/800 ms).

## Wiring (CN2 40-pin GPIO expansion header)

| Module | DK1 CN2 pin | Signal            |
|--------|-------------|-------------------|
| GND    | 39          | GND               |
| VDD    | 1           | 3V3               |
| SCL    | 5           | GPIO3 / I2C5_SCL (PA11) |
| SDA    | 3           | GPIO2 / I2C5_SDA (PA12) |
| RES    | 26          | GPIO7 (PF3)       |

Module I2C address 0x3C (7-bit) / 0x78 (8-bit write), as on the AT32 port.

## BSP prerequisite: I2C5 must be enabled (patch applied)

The stock OpenSTLinux DK1 device tree ships `i2c@40015000` **disabled** (only
I2C1 for HDMI/touch and I2C4 for the PMIC are enabled). The node is otherwise
fully configured — pinctrl `i2c5-0` maps exactly PA11 AF5 / PA12 AF5
open-drain, 400 kHz — so the patch is a one-line flip:

```sh
dtc -I dtb -O dts -o dk1.dts stm32mp157a-dk1.dtb
# in the i2c@40015000 node (the A7 one, not the rproc-srm copy):
#   status = "disabled";  ->  status = "okay";
dtc -I dts -O dtb -o dk1-i2c5.dtb dk1.dts
scp dk1-i2c5.dtb root@<board>:/tmp/
ssh root@<board> 'cp /tmp/dk1-i2c5.dtb /boot/stm32mp157a-dk1.dtb && sync && reboot'
```

Already applied to this board (verified: `i2c-1: STM32F7 I2C(0x40015000)`,
survives reboot). **Re-apply after any bootfs reflash** — the archived
`splitted-bootfs.ext4` still contains the unpatched DTB. The app
auto-detects the adapter by controller address at runtime.

## Build & run

```sh
. ./env.sh
cd apps/ssd1317_md066_64x48_iic
make && make deploy
ssh root@<board> /usr/local/bin/ssd1317_demo
```

Startup self-checks, printed on the console:

```
RES = GPIOF3 ok
pacing LEDs available: 3
SSD1317 MD066 64x48 I2C demo start (/dev/i2c-1 @ 0x3c)
```

With the module absent the probe fails cleanly:
`OLED not detected at 0x3c on /dev/i2c-1 (No such device or address)` —
a clean NAK (ENXIO) proves the controller is alive; the module is simply not
answering.

## Possible conflicts with this wiring (the requested analysis)

1. **I2C5 disabled in the BSP device tree** — the main conflict, resolved by
   the patch above. Source: ST's `stm32mp15xx-dkx` dts enables only I2C1
   (HDMI/touch) and I2C4 (PMIC).
2. **PA11/PA12 are shared with the Arduino connector**: CN13 pins 9/10
   (ARD_D14/D15) are wired directly to PA12/PA11 = I2C5_SDA/SCL. Any shield
   stacked on CN13/CN14 with an I2C entry on D14/D15 shares this bus — its
   pull-ups and device addresses land on the same wires as the OLED.
   CN17 A4/A5 do **not** share I2C5 in the default solder-bridge config
   (SB24/SB26 ON route A4/A5 to PC3/PF12); fitting SB23/SB25 instead of
   SB24/SB26 would add A4/A5 to this bus.
3. **PA11/PA12 as USB-looking pins is a false lead**: UM2534 Table 4 shows
   PA11/PA12 as `USB_STLK_N/P` — but those belong to the **ST-LINK MCU**
   (U19), not the STM32MP157. The MP157's USB HS PHY uses dedicated internal
   pins, so I2C5 on PA11/PA12 has no USB conflict.
4. **RES on PF3 (CN2-26)**: free in the BSP — zero device-tree references,
   claimed successfully from userspace at runtime (verified). Note the DTB
   also carries `m4-i2c5-0` pinctrl for the Cortex-M4: if an M4 firmware ever
   claims I2C5 on the coprocessor side, it would fight Linux for PA11/PA12 —
   not an issue while no M4 image runs.
5. **Address space**: 0x3C is exclusive on this bus — the DTB attaches no
   other device to I2C5. (On I2C1/I2C4, 0x3C-region addresses are used by
   HDMI/touch — which is exactly why the module must be on I2C5.)
6. **Power**: CN2-1 is the 3V3 rail (BUCK4). A 64x48 OLED draws ~10-20 mA —
   negligible. The module makes its own panel voltage via charge pump
   (init `0x8D 0x14`). Per UM2534's caution: never feed 3.3/5 V *from* the
   module/shield side into the board.
7. **Bus speed**: the BSP node sets 400 kHz; the SSD1317 accepts it (the
   AT32 original ran ~200 kHz — identical visible behavior, ~2x faster
   refresh).
8. **Practical**: CN2 pins 1/3/5/26 are used with individual jumpers — pins
   1 (3V3) and 4 (5V) are adjacent; verify orientation before power (5 V on
   the module's VDD can destroy it).
