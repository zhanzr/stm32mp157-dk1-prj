# blink_check

Hardware bring-up diagnostics for CN2-wired modules. Two small tools:

## blink_check — wire continuity by blinking

Drives PA11 (CN2-5), PA12 (CN2-3) and PF3 (CN2-26) as GPIO outputs in
rotation (400 ms on/off each, labeled on the console) so an LED module (or
DMM/scope) on those wires proves end-to-end continuity, one wire at a time.

Because PA11/PA12 are owned by the I2C5 driver, the tool **unbinds**
`stm32f7-i2c`/`40015000.i2c` before claiming them and **re-binds on exit**
(including Ctrl-C/SIGTERM — verified the bus comes back and `i2cdetect -y 1`
works again afterwards).

```sh
make BOARD=root@<board-ip> deploy
ssh root@<board> /usr/local/bin/blink_check
```

Interpretation (LED module on the three wires, GND to CN2-39):

| Observation | Conclusion |
|---|---|
| 3 LEDs light in rotation | SoC→wire paths correct |
| one LED never lights | that wire is broken / off-by-N on the header |
| wrong LED lights | wires swapped |
| faint glow when off | adjacent-pin bleed / missing series resistor |

## i2cpin_probe — pin mux / electrical state probe

Reads PA11/PA12 as GPIO inputs, first without bias then with internal
pull-up, 5 samples each. Purpose: distinguish "line floating (nothing
connected)" / "held low (short, unpowered device)" / "pulled high (device
present with pull-ups)".

Note: once the I2C5 driver is bound, the pins are muxed to the controller
and the GPIO request fails — which is itself the proof that the DTB/pinctrl
side is active (this is how the SSD1317 bring-up confirmed it). Use it after
an unbind, or on pins not owned by a controller.

```sh
$CC -O2 -o i2cpin_probe i2cpin_probe.c   # inside the SDK environment
scp i2cpin_probe root@<board>:/tmp/ && ssh root@<board> /tmp/i2cpin_probe
```

Output interpretation:

```
all-1 with and without pull-up  = pulled high externally (device present)
0 without / 1 with internal pull-up = line held low (short / unpowered)
random                             = floating (nothing connected)
GPIO request failed: ...           = pin muxed to another function
```
