# record-play

Interactive audio recorder/player for the DK1 (CS42L51 codec on the 3.5 mm
jack: MIC_IN / line-out). Native ALSA (`-lasound` from the SDK sysroot), no
external tools.

## Flow

```
app start
  -> "Press any key"          -> start recording (48 kHz, 2ch, S16_LE)
  -> records up to 30 s
     or until 'S' pressed     (live second counter, any other key ignored)
  -> "Press any key"          -> play the recorded buffer through the jack
  -> loop back to the start   (Ctrl-C exits cleanly, LEDs of the console intact)
```

## Build & deploy (in WSL, from the repository root)

```sh
. ./env.sh
cd apps/record-play
make
make deploy                   # BOARD=root@<board-ip> to override
```

## Run

On the board console (ST-LINK serial) or `ssh -tt`:

```sh
/usr/local/bin/record-play
```

Plug a headset/mic into the CN10 audio jack. Note the stock mixer routes the
left ADC to MIC+preamp but the right ADC to AIN; for stereo-from-mic:

```sh
amixer -c 0 cset numid=23 3   # PGA-ADC Mux Right -> MIC+preamp Right
```

## Devices & implementation notes (verified on this board)

- capture: `hw:0,1` (cs42l51-hifi capture, `pcmC0D1c`)
- playback: `hw:0,0` (cs42l51-hifi playback, `pcmC0D0p`)
- `default` is broken on this image (`arecord -D default` -> "Host is down");
  the i2s D2 PCM is the SPDIF-ish controller, not the jack.
- the image ships `arecord` but **no `aplay`** — hence the self-contained
  playback path.
- **ALSA gotcha**: `snd_pcm_set_params()` alone leaves the capture
  `start_threshold` at the full buffer size (8192 frames here); the hardware
  never receives its start trigger and reads fail with `-EIO`. The app
  explicitly sets `start_threshold = 1` and `avail_min` via
  `snd_pcm_sw_params_*` — do not remove that block.
- stdin is put into raw mode for "any key" detection; capture polls it via
  `select()` between ALSA reads so 'S' stops mid-recording.
- signal handling: handlers are installed via `sigaction` **without
  SA_RESTART** (glibc `signal()` auto-restarts blocking reads, which made
  Ctrl-C dead after the first loop iteration); SIGTSTP restores the terminal
  and SIGCONT re-arms raw mode, so suspending never leaves the tty broken nor
  the PCM devices locked.
