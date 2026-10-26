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

Expansion connector pinouts: [docs/expansion-connectors.md](docs/expansion-connectors.md)

## Usage

- Source `env.sh` (WSL) to load the cross toolchain, then `make` inside an app
  directory; each app has its own README with details
- `make deploy` transfers the built binary to the board
  (rsync if the board has it, tar-over-ssh otherwise)
- User LEDs LD5/LD6/LD7 are available to applications; LD8 is the kernel
  heartbeat

## Applications

- [apps/blink_hello](apps/blink_hello) — blinks LD5/LD6/LD7, samples DTS temperature + ADC channels
- [apps/coremark](apps/coremark) — EEMBC CoreMark, ARM vs Thumb-2 builds and scores
- [apps/record-play](apps/record-play) — interactive audio record (30 s / S-key) and playback, loops
- [apps/netbench](apps/netbench) — TCP throughput benchmark for GbE and USB NCM gadget
