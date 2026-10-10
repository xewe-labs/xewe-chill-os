# XeWe Laptop Cooling Pad

Firmware for a DIY laptop cooling pad on an ESP32. An infrared sensor measures the laptop's
underside. Two PC fans follow a temperature curve that you set. An LED strip shows the temperature
as a colour, from cold to hot. You change everything from a web page or the serial console.

Version 2.0.0 · GPL-3.0-only · built on [XeWe OS](https://github.com/xewe-labs/xewe-os)

> Status: the firmware builds for ESP32-C3, C6 and S3. It has not yet run on the assembled pad.
> Use [tests/MANUAL.md](tests/MANUAL.md) for the first test on real hardware.

## What it does

| Part | Behaviour |
|---|---|
| Sensor | MLX90614 reads the object temperature every 500 ms (every 5 s while it does not answer). |
| Fans | Two 4-wire PWM fans at 25 kHz. Once a second the curve sets both to the same speed. |
| Tachometer | Each fan reports its RPM (updated about once a second, rounded to 10 RPM). |
| Fan curve | Up to 16 points of temperature → speed. Straight lines between points. |
| LED strip | Off below the first curve point. Cold colour at the first point, hot colour at the last, a blend in between. The colour glides; it never jumps. |
| Fail-safe | No reading from the sensor: the fans run at the last point's speed (100 % by default) and the strip shows the hot colour. |
| Web page | `http://<device-ip>/`: curve editor, live operating point, cold and hot colour pickers. |
| Settings | Curve, colours, fans and pins survive a restart and a power cut. |

## Hardware

| Item | Notes |
|---|---|
| ESP32-C3, ESP32-C6 or ESP32-S3 board | The pad was built with a C3. |
| 2 × 4-wire PWM fans (PC fans) | 12 V or 5 V, with their own supply. |
| MLX90614 sensor module (e.g. GY-906) | I²C, address 0x5A. Use a module with I²C pull-up resistors. |
| WS2812B LED strip | 12 LEDs on the pad. 5 V supply. |
| 2 × 10 kΩ resistors | Optional: external pull-ups on the tach lines (see [doc/wiring.md](doc/wiring.md)). |
| Power supply | For the fans and the strip. Connect all grounds together. |

## Pins per chip

These are the first-boot defaults. Change them later with commands (see
[doc/wiring.md](doc/wiring.md)).

| Signal | C3 (the built pad) | C6 | S3 |
|---|---|---|---|
| Fan 1 PWM / tach | GPIO 3 / GPIO 0 | GPIO 3 / GPIO 0 | GPIO 4 / GPIO 5 |
| Fan 2 PWM / tach | GPIO 10 / GPIO 1 | GPIO 10 / GPIO 1 | GPIO 6 / GPIO 7 |
| Sensor SDA / SCL | GPIO 4 / GPIO 5 | GPIO 4 / GPIO 5 | GPIO 8 / GPIO 9 |
| Strip data | GPIO 6 (build with `LED_PIN_DATA=6`) | GPIO 6 (same) | GPIO 48 |

The strip pin is GPIO 8 on C3 and C6 unless you build with `--define LED_PIN_DATA=6` or set it
later with `$led set pin_data 6`. Full wiring: [doc/wiring.md](doc/wiring.md).

## Build and flash

You need Python 3.11 or newer and git. The first `./setup.sh` downloads the toolchain once.

```sh
./setup.sh
./run.sh --chip c3 --define LED_PIN_DATA=6 --define LED_COUNT=12
```

`./run.sh` builds, erases the whole flash, flashes and opens the serial console (115200 baud).
Every run is a first boot. Add `--keep-nvs` to keep the settings. Use `--port /dev/ttyACM0` (Linux)
or `--port /dev/cu.usbmodemXXXX` (macOS) when more than one board is attached.

Build only:

```sh
build/tools/.venv/bin/python -m xewe build --chip c3 --define LED_PIN_DATA=6 --define LED_COUNT=12
```

For an S3 or C6 board, change `--chip`. On the S3 the defaults need no `--define`.

## First boot

The console asks a few questions. Answer them in the console, or run
`build/tools/.venv/bin/python -m xewe provision` to answer them from your settings.

1. Name the device and confirm the name.
2. Enable WiFi, pick your network, enter the password.
3. Enable the Fan module: answer `y`.
4. Enable the MLX90614 module: answer `y`.

The device then stores its defaults: both fans, the sensor pins, the curve
22 °C → 0 %, 27 °C → 100 %, 32 °C → 100 %, cold colour `#00FFFF` (cyan), hot colour `#FF0000` (red).
The boot log prints the IP address. Open `http://<device-ip>/`.

Check the parts:

```text
$mlx90614 read
$fan status
$pad status
```

## The fan curve

A curve is a list of points: temperature in °C and fan speed in %. Below the first point the fans
run at the first point's speed. Above the last point they run at the last point's speed. In between
the speed is a straight line.

Example, the default curve:

| Temperature | Fan speed |
|---|---|
| 20 °C | 0 % (below the first point) |
| 22 °C | 0 % |
| 24.5 °C | 50 % |
| 27 °C and above | 100 % |

Set your own:

```text
$fan curve set 30:20,40:60,50:100
$fan curve add 45 80
$fan curve remove 45
$fan curve list
```

More in [doc/fan-curve.md](doc/fan-curve.md).

## The LED indicator

The strip follows the same curve. With the default curve and colours:

| Temperature | Strip |
|---|---|
| below 21.5 °C | off |
| 22 °C | cyan `#00FFFF` |
| 27 °C | `#7F7F7F` (half way between 22 °C and 32 °C) |
| 32 °C and above | red `#FF0000` |
| no sensor reading | red |

The strip turns on at the first point and turns off 0.5 °C below it, so it does not flicker at the edge.

```text
$pad set_cold 0000FF
$pad set_hot FF4000
$led brightness 64
$pad lights off      # the strip shows a normal LED mode instead
$pad lights on       # back to the temperature colour
```

`$led brightness`, `$led on` and `$led off` also apply to the indicator.

## The web page

Open `http://<device-ip>/` (also `/pad`).

| Control | What it does |
|---|---|
| Curve graph | The curve, the colour gradient under it and a white dot at the current temperature and fan speed. Updates every second. |
| Table | One row per point: temperature (0–100 °C) and speed (0–100 %). |
| Add Point | Adds a point 5 °C after the last one. |
| Delete | Removes a point. A curve keeps at least 2 points on the page. |
| Save Curve | Sends the curve and the colours to the device. Nothing changes on the device before you save. |
| Coldest Color / Hottest Color | Saved as soon as you close the colour picker. |

The command console is at `http://<device-ip>/console`.

JSON for scripts: `GET /api/state`, `/fan/data`, `/mlx90614/data`, `/led/data`, `/ui/config`;
`POST /ui/config` with `{"temp_curve":[{"temp":30,"speed":20}],"curve_edge_colors":{"start":"#00FFFF","end":"#FF0000"}}`.

## Commands

Type commands in the serial console or on the `/console` page. `$help` lists every group.

| Group | Commands |
|---|---|
| `$pad` | `set_cold` · `set_hot` · `lights` · `print_json` · `status` · `reset` |
| `$fan` | `curve list\|add\|remove\|set` · `temp` · `add` · `add_w_tach` · `set` · `set_all` · `remove` · `get` · `schema` · `print_json` · `status` |
| `$mlx90614` | `read` · `scan` · `set_addr` · `set_pins` · `set` · `get` · `schema` · `print_json` · `status` |
| `$led` | `on` · `off` · `brightness` · `set` · `get` · `fill` · `mode ...` · `status` |
| `$wifi` | `connect` · `disconnect` · `scan` |
| `$system` | `restart` · `info` · `status` · `reset` (factory reset) |

Every command with its arguments: [doc/commands.md](doc/commands.md).

## Settings and restart

Settings are written to flash when you change them. The control loop and the LED frames never
write to flash. After a restart the device loads the curve, the colours, the fans, the sensor pins
and the strip settings, and continues.

Factory reset: `$system reset`, then answer `y`. See [doc/troubleshooting.md](doc/troubleshooting.md).

Upgrading from firmware 0.0.12: the old settings are not read. Enter your curve and colours again.

## Known limits

- One data pin for the strip. The pad's two strips share it: wire both inputs to the same pin, or
  chain them and set `$led set num_led 24`. The two strips always show the same colour.
- The tachometer input has no glitch filter. A noisy tach line freezes the RPM value instead of
  showing a wrong one. An external 10 kΩ pull-up to 3.3 V helps.
- If no temperature reading arrives for `stale_ms` after boot (sensor module disabled, pins not set,
  bus dead), the fans go to the curve's highest point and `$fan status` says `never received
  (fail-safe N %)`. The first real reading hands control back to the curve.
- The web page shows 0.0 °C while the sensor gives no reading. `$mlx90614 status` shows `Online: no`.
- The web page edits 0–100 °C. The console accepts -40 to 200 °C.
- `$mlx90614 scan` blocks the console for up to 1.5 s.
- The device joins an existing WiFi network. It cannot start its own access point.

## Documents

| File | Contents |
|---|---|
| [doc/wiring.md](doc/wiring.md) | Wiring tables per chip, power, pin notes |
| [doc/fan-curve.md](doc/fan-curve.md) | How the curve and the colours work, examples |
| [doc/commands.md](doc/commands.md) | Every command |
| [doc/troubleshooting.md](doc/troubleshooting.md) | Sensor offline, fans silent, RPM 0, factory reset |
| [tests/MANUAL.md](tests/MANUAL.md) | Test plan for real hardware |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Build system and project layout, for developers |

## For developers

The project code is `src/PadWeb/`. The fans, the sensor, the strip, WiFi and the web server are
modules from [xewe-os-modules](https://github.com/xewe-labs/xewe-os-modules), listed in `xewe.toml`.

```sh
build/tools/.venv/bin/python -m xewe build --all-chips    # c3, c6, s3
build/tools/.venv/bin/python -m xewe test --unit-only     # developer machine, seconds
build/tools/.venv/bin/python -m xewe test                 # with a provisioned board attached
```

Board tests in `tests/board/` read `XEWE_TEST_MLX_PRESENT=1` and `XEWE_TEST_PAD_HARDWARE=1` for the
assembled pad. The test image uses the module default strip pin (8 on C3/C6).

## Licence

GPL-3.0-only. See [LICENSE.txt](LICENSE.txt).
