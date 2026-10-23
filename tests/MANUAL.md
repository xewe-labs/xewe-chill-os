<!--
SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
SPDX-License-Identifier: GPL-3.0-only
xewe-chill-os/tests/MANUAL.md
-->

# Manual test on real hardware

Run this once on the assembled pad: board, two fans with tach wires, MLX90614, LED strip.
Write the result (pass / fail and the value seen) next to each step.

## Before you start

- Wire as in [doc/wiring.md](../doc/wiring.md). Fans and strip on their own supply, grounds joined.
  The strip data pin is `XEWE_LED_PIN_DATA` in `Config.h` (GPIO 6 on C3/C6).
- Have a heat source for the sensor: a warm hand, a hair dryer on low at 30 cm, or a mug of hot water.
- Have a phone or laptop on the same Wi-Fi.

```sh
./setup.sh
./run.sh --chip c3
```

`./run.sh` erases the flash, so this is a first boot. Answer the first-boot questions as in the
README (First boot): name, Wi-Fi, `y` for Fan and MLX90614 if asked, and the strip setup (WS2812B,
5 V, one strip of 12 LEDs, colour order).

## 1. First boot

| Step | Do | Expect |
|---|---|---|
| 1.1 | Watch the boot log | IP address printed. No line starting with `!`. `MLX90614: online at 0x5A, <room temp> C` |
| 1.2 | `$pad status` | `Pad module enabled`, `Lights: on (strip follows the temperature)`, `Cold color: #00FFFF`, `Hot color: #FF0000`, `Temperature: <room temp> C` |
| 1.3 | `$fan curve list` | `22.00 C -> 0 % (PWM 0)`, `27.00 C -> 100 % (PWM 255)`, `32.00 C -> 100 % (PWM 255)` |
| 1.4 | `$led status` | `Data Pin: GPIO_6`, `Length: 12`, `Mode: [0] Solid` while the strip is lit |

## 2. Sensor

| Step | Do | Expect |
|---|---|---|
| 2.1 | `$mlx90614 read` | `MLX90614: object <room temp> C, ambient <room temp> C`, both 15–35 °C |
| 2.2 | `$mlx90614 status` | `Online: yes`, `Read errors: 0` |
| 2.3 | Hold a warm hand 2 cm from the sensor, `$mlx90614 read` | Object rises by 5 °C or more; ambient barely moves |
| 2.4 | `$mlx90614 scan` | `Found device at 0x5A (default MLX90614)`, `Scan complete. Found 1 device(s).`, done in under 1.5 s |

## 3. Fans and RPM

| Step | Do | Expect |
|---|---|---|
| 3.1 | `$fan curve set none`, then `$fan set_all 255` | `Fan: curve set (0 points)`, `Fan: all fans updated`; both fans at full speed |
| 3.2 | Wait 5 s, `$fan status` | Both fans show `tach pin`, RPM near the fan's rated speed (e.g. 1500–3000) |
| 3.3 | `$fan set_all 128`, wait 5 s, `$fan status` | RPM lower than in 3.2, not 0 |
| 3.4 | `$fan set_all 0`, wait 5 s, `$fan status` | RPM 0 or the fan's minimum speed |
| 3.5 | Repeat 3.2 three times, 5 s apart | RPM stays within ±10 % of itself; never frozen while the speed changes |
| 3.6 | `$fan curve set 22:0,27:100,32:100` | `Fan: curve set (3 points)` |

Write down the RPM at 255 and 128 for each fan.

## 4. Curve follows the sensor

Set a curve close to room temperature. Example for a 24 °C room:

```text
$fan curve set 26:0,30:50,34:100
```

| Step | Do | Expect |
|---|---|---|
| 4.1 | Sensor at room temperature, wait 2 s, `$fan status` | `Temperature: 24.xx C (source)`, `Curve target: 0 %`, strip off |
| 4.2 | Warm the sensor to about 30 °C, `$fan status` | `Curve target` about 50 %, fans audibly faster within 2 s |
| 4.3 | Warm to 34 °C or more | `Curve target: 100 %`, RPM at maximum |
| 4.4 | Remove the heat, wait 30 s | Target falls back with the temperature; fans slow down |
| 4.5 | `$fan temp 32` | `Fan: temperature set to 32.00 C, curve target 75 %`. The next reading (0.5 s) overrides it |

## 5. LED colour

Keep the curve from step 4 (26, 30, 34 °C). The strip changes at most once every 2 s
(`XEWE_PAD_COLOR_MIN_MS`) and only for a step of 8 or more on a channel (`XEWE_PAD_COLOR_STEP`);
the cold and hot colours are shown exactly.

| Step | Do | Expect |
|---|---|---|
| 5.1 | Sensor at room temperature (below 25.5 °C) | Strip off; `$led status` → `Power State: OFF` |
| 5.2 | Warm the sensor slowly through 26 °C | Strip turns on cyan at 26 °C, in Solid mode |
| 5.3 | Warm to 30 °C, `$pad status` | `Strip: lit #7F7F7F` or within 8 per channel; the strip is grey-white |
| 5.4 | Warm to 34 °C or more | Strip red, `Strip: lit #FF0000` |
| 5.5 | Watch the strip while the temperature changes | Colour moves in small steps, at most one every 2 s, no flicker |
| 5.6 | Cool to 25.8 °C (between 25.5 and 26) | Strip stays on |
| 5.7 | Cool below 25.5 °C | Strip turns off |
| 5.8 | `$pad set_cold 0000FF` while below 30 °C | Strip turns to a blue tone within 3 s |
| 5.9 | `$led set_brightness 32` | Indicator dims; colour unchanged |
| 5.10 | `$pad lights off`, then `$led set_mode 5` | `Pad lights off: the strip is left to $led.`; the rainbow stays, `$led status` → `Mode: [5]` |
| 5.11 | `$pad lights on` | Within 3 s the strip is back in Solid mode with the indicator colour, `$led status` → `Mode: [0] Solid` |

Set `$pad set_cold 00FFFF` and `$led set_brightness 128` afterwards.

## 6. Web page

Open `http://<device-ip>/` on the phone or laptop.

| Step | Do | Expect |
|---|---|---|
| 6.1 | Load the page | Title "XeWe Laptop Chiller", curve graph, table with the current points |
| 6.2 | Warm the sensor | White dot moves right and up within 2 s; label shows the temperature |
| 6.3 | Change a point's speed, press **Save Curve** | `$fan curve list` shows the new value |
| 6.4 | **Add Point**, **Save Curve** | New point 5 °C after the last one, also in `$fan curve list` |
| 6.5 | **Delete** a point, **Save Curve** | Point gone in `$fan curve list`; Delete is greyed out at 2 points |
| 6.6 | Pick a new Hottest Color, close the picker | `$pad status` shows the new hot colour; strip follows when hot |
| 6.7 | Open `http://<device-ip>/console`, run `$fan status` | Command output in the page |
| 6.8 | Reload the page | Same curve and colours as saved |

## 7. Restart persistence

| Step | Do | Expect |
|---|---|---|
| 7.1 | Note `$fan curve list`, `$pad status`, `$led status` | |
| 7.2 | `$system restart` | Boots without first-boot questions |
| 7.3 | Same three commands | Same curve, colours, lights setting, strip pin and length |
| 7.4 | Unplug power for 10 s, plug in | Same as 7.3; fans follow the curve within 2 s of boot |

## 8. Sensor unplugged

| Step | Do | Expect |
|---|---|---|
| 8.1 | Fans at low speed (sensor cool). Unplug the sensor's SDA wire | Within 6 s: fans at full speed (last point), strip red |
| 8.2 | `$fan status` | `Temperature: offline (source)`, `Curve target: 100 %` |
| 8.3 | `$mlx90614 status` | `Online: no`, `Object: n/a (offline)` |
| 8.4 | Web page | Dot at 0.0 °C and 100 % |
| 8.5 | Plug SDA back in, wait 6 s | `Online: yes`; fans and strip follow the temperature again |
| 8.6 | `$mlx90614 disable`, answer `y`; after the restart wait 12 s, `$fan status` | `Temperature: never received (fail-safe 100 %)`, fans at full speed. `$mlx90614 enable` afterwards |

## 9. Factory reset

| Step | Do | Expect |
|---|---|---|
| 9.1 | `$system reset`, answer `y` | Device restarts and asks the first-boot questions |
| 9.2 | Answer them, `$fan curve list`, `$pad status` | Default curve and colours |
