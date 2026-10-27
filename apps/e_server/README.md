# e_server - embedded web demo (STM32MP157A-DK1 port)

Single-page web app for the **STM32MP157A-DK1** board, served by a native
Linux web server (`server.c`) running directly on the board. Ported from the
apollo-f429 / fire-f429 `e_server` (frontend + host reference backend).

Adapted for the DK1:

- **LED control** drives the three on-board LEDs: **LD5 (PA14)**,
  **LD6 (PA13)** (low-active) and **LD7 (PH7)** (high-active), via the GPIO
  character device (`/dev/gpiochipA|H`, v2 UAPI).
- **Sensors** plots the internal channels only (same set as
  `apps/mini_gui`): **VREFINT** (ADC2 ch13, mV), **CPU temperature**
  (DTS thermal zone, °C) and **VDDCORE** (ADC2 ch14, mV). VBAT / DHT11
  from the original do not exist on this board and were removed. Each plot
  line has its own color.
- **Streaming** tab: live **MJPEG camera stream** from a UVC webcam
  (`/stream`, 640×480 MJPEG @ ~15 fps, multipart/x-mixed-replace — opens in
  any browser `<img>`). One viewer at a time (further clients get 503);
  the camera must not be held by another app (mini_gui etc.).
- **Board info** reports `arch` + `lan_ip` (real), and best-effort
  `public_ip` / geo / weather over plain HTTP (ipify / ip-api / wttr.in);
  anything unavailable shows as "N/A".

## API (unchanged contract, adapted fields)

| Route           | Description |
| --------------- | ----------- |
| `GET /`         | gzipped page (CSS/JS inlined) |
| `GET /api/leds` | `{"leds":[0,0,0]}` — {LD5, LD6, LD7} |
| `POST /api/leds`| body `{"leds":[1,0,1]}` → applied (LD5+LD7 on), `{"leds":[1,0,1]}` |
| `GET /api/adc`  | `{"vrefint_mv":2519.0,"vddcore_mv":1181.9,"temp_c":50.51,"ts":…}` (missing sensors → `null`) |
| `GET /stream`   | MJPEG multipart stream (640×480 @ ~15 fps), single viewer |
| `GET /api/info` | `{"arch":"arm","lan_ip":…,"public_ip":…,"geo":…,"weather":…,"ts":…}` |
| `GET /public/*` | raw image bytes from `embedded_files[]` |

## Streaming implementation notes

- The server opens the first capture-capable `/dev/videoN`, forces
  **MJPEG 640×480** (the camera compresses in hardware — no CPU encoding),
  mmaps 4 buffers and `STREAMON`s.
- Each grabbed frame is pushed as one multipart part
  (`--frame` + `Content-Length` + JPEG bytes); output is throttled to
  ~15 fps (measured: 70 frames in 4.6 s, ~1.9 Mbps).
- **Gotchas fixed during bring-up**: (1) the per-connection worker used to
  close the socket after handing it to the stream thread — `/stream` now
  takes ownership and the worker returns without closing; (2) the
  `cam_stream` struct must be zeroed (an uninitialized one made the camera
  thread hang forever before sending anything); (3) VREFINT needs `-u
  _printf_float`-style care in general — here it is served as the raw
  driver value which is uncalibrated on this chip (blank OTP).

## Build & run (host, x86 test)

```bash
python3 build_web.py     # regenerate web_assets.h after editing web/ or public/
make                     # host gcc build
./e_server 8080          # http://localhost:8080/
```

`/api/adc` returns `null` fields on the host (no STM32 sensors in a PC) —
by design; the page holds the last value / shows N/A.

## Build & run (board, cross-compiled)

```sh
. ./env.sh
cd apps/e_server
$CC -O2 -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE \
    -o e_server_arm server.c -lpthread -lm
scp e_server_arm root@<board-ip>:/usr/local/bin/e_server
ssh root@<board-ip> 'nohup /usr/local/bin/e_server 80 >/tmp/e_server.log 2>&1 &'
```

Then open `http://<board-ip>/` from any browser on the LAN (port **80**;
the server runs as root so it may bind the privileged port).

The `/api/leds` handlers drive the real **LD5/LD6/LD7** through
`/dev/gpiochipA` and `/dev/gpiochipH` (GPIO character device;
LD5/LD6 low-active, LD7 high-active) — the web checkboxes toggle the
physical LEDs.

## Streaming tab

`GET /stream` serves the UVC webcam as
`multipart/x-mixed-replace` MJPEG (640x480 @ ~15 fps, hardware-compressed
by the camera — no CPU encoding). The Streaming tab embeds it with a
**Start/Stop stream** button; one viewer at a time (second client gets
503), and the browser auto-retries if the camera was busy. Verified:
~1.4 MB per 4 s (~2.8 Mbps) over LAN.

## Layout

```
e_server/
  web/             raw frontend sources (index.html, style.css, app.js)
  public/          images served to the page (dk1_board_0.png)
  build_web.py     bundle: inline CSS/JS -> gzip page -> C arrays
  web_assets.h     generated C arrays (page + images + lookup table)
  server.c         the web server (routing + LED GPIO + sensor sysfs reads)
  Makefile         host build (gcc); board build via the SDK $CC (see above)
```

Regenerate `web_assets.h` after any change to `web/` or `public/`
(`python3 build_web.py`); it is committed alongside the sources.

## Porting notes (apollo-f429 → stm32mp157-dk1)

- LEDs: PB1/PB0 (bare-metal) → PA14/PA13 via the kernel GPIO character
  device. Polarity stays low-active; `leds[]` keeps the logical on/off.
- `/api/adc`: the F429 internal channels (VREFINT/die-temp/VBAT) + DHT11
  became the MP157 internal set (VREFINT/VDDCORE via the IIO sysfs of
  `adc@100` + the DTS thermal zone). The 2048-byte-style simulation code
  was replaced by real sysfs reads; missing channels serialize as `null`.
- No other changes: the HTTP plumbing, routing, bundling and the frontend
  framework are identical to the original.
