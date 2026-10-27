# mini_gui

Minimal GTK3 GUI for the DK1 running on Weston/HDMI.

- **Upper half**: live UVC webcam view embedded in the GTK window.
  Pipeline candidates are tried in order (all capture nodes × all modes ×
  both sinks) until one reaches PLAYING; the working combination observed
  on this hardware is `/dev/video0` + `image/jpeg ! jpegdec` +
  **`gtksink`** (software GTK sink). `gtkglsink` fails when the app runs
  as root over ssh (EGL/GL context unavailable) — the try-order handles
  it automatically.
  If no camera pipeline succeeds, the top area falls back to a string.
- **Lower half**: a row of auto-scaling scrolling plots (500 ms sampling,
  120 points ≈ 60 s window), one panel per **internal** sensor:
  - CPU temperature (DTS digital sensor, `thermal_zone0`) in °C
  - `vrefint` (ADC2 ch13, processed mV)
  - `vddcore` (ADC2 ch14, raw × scale ≈ 1.18 V)
  - Internal channel map per RM0436 Table 187: ch12 vsense, ch13 vrefint,
    ch14 vddcore, ch15 vbat/4, ch16/17 dac — panels appear only for
    channels instantiated by the DT (this BSP exposes ch13 + ch14).
  - External pins (ch18/19) and unlabeled channels are deliberately
    excluded.

## Build & run

```sh
. ./env.sh
cd apps/mini_gui
make
make deploy BOARD=root@<board-ip>
make run BOARD=root@<board-ip>      # kills camera holders, launches on Weston/HDMI
make stop
```

`make run` first terminates any process holding `/dev/video*` (a previous
camera app keeps the device busy otherwise), then sets
`XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0` (Weston runs as
user `weston`, uid 1000) and starts the app in the background; its log is
`/tmp/mini_gui.log`.

## Dependencies (all present in the SDK sysroot and on the board image)

| Library | Version |
|---|---|
| gtk+-3.0 | 3.24.41 |
| gstreamer-1.0 / gstreamer-video-1.0 | 1.22.12 |
| cairo (via GTK) | 1.18.0 |

Board GStreamer elements: `v4l2src`, `jpegdec`, `videoconvert`, `gtksink`,
`gtkglsink` (all verified present).

## Notes

- Camera selection: scans `/dev/video*`, keeps nodes with
  `V4L2_CAP_VIDEO_CAPTURE` (UVC metadata nodes are skipped), tries
  MJPEG first (this webcam: 640×480@30 MJPG), then raw YUY2, then any raw.
- `gtkglsink` requires EGL/GL: it fails with "Failed to initialize OpenGL
  with Gtk" when the app runs as root over ssh (no GPU device access) —
  `gtksink` (software, cairo) is tried second and is the reliable default.
  The window background is forced to black via CSS so letterbox/unpainted
  areas never show stale-pixel artifacts.
- ADC scale: `_raw` channels are converted with the **global**
  `in_voltage_scale` of the device (0.038147 mV/LSB for the 2.5 V/16-bit
  setup). There is no per-channel `_scale` file.
- `vrefint*` is marked uncalibrated: the driver's processed value (~2.5 V)
  is computed from the OTP `VREFINT_CAL` word, which is blank on this chip
  (see `apps/blink_hello` notes) — treat its reading as invalid.
- Sensors are re-read from sysfs on every tick; each panel auto-scales.
