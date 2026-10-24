# xewe-chill-os

Firmware for a DIY laptop cooling pad on an ESP32-C3, -C6 or -S3, built on
[XeWe OS](https://github.com/xewe-labs/xewe-os). An infrared sensor measures the laptop's underside,
two PC fans follow a temperature curve that you set, and an LED strip shows the temperature as a
colour. You change everything from a web page or the serial console.

## Problem

- A pad with fixed-speed fans makes the same noise whether the laptop is idle or under load, so
  you either live with the noise or switch the fans down and forget to switch them back up.
- You cannot see how hot the laptop's underside is without a monitoring tool running on it.

## Solution

The pad reads the laptop's temperature itself and sets its own fan speed. A curve of up to 16
points maps temperature to speed, and the strip shows the same curve as a colour from cold to hot.
Three ideas carry it:

- An MLX90614 reads the underside every 500 ms, and both fans follow the curve once a second.
- When no reading arrives, the fans run at the curve's last point (100 % by default) and the strip
  shows the hot colour.
- The curve, colours, fans, pins and strip settings live in flash, and you change them from
  `http://<device-ip>/` or the console without rebuilding.

## See it

```text
$fan curve set 30:20,40:60,50:100
$fan temp 35
Temperature set to 35.00 C, curve target 40 %.
```

The next sensor reading replaces the test value, within 0.5 s, or within 5 s while the sensor is
offline.

## Why not a fixed-speed pad

| | Fixed-speed pad | xewe-chill-os |
|---|---|---|
| Fan speed | one setting, chosen by hand | follows the laptop's temperature |
| Temperature | not shown | strip colour, web page, `$mlx90614 read` |
| Sensor failure | not applicable | fans to the last point, strip to the hot colour |
| Build | bought finished | an ESP32, two PC fans, an MLX90614 and a WS2812B strip that you wire |

## Install

You need Python 3.11 or newer and git. Everything installs into the project folder and
`~/.xewe-os`, so you never need sudo. The first `./setup.sh` on a machine downloads the toolchain
once.

```sh
git clone https://github.com/xewe-labs/xewe-chill-os && cd xewe-chill-os
./setup.sh                                          # installs tools, core, wifi, web-interface, led
./run.sh --chip c3                                  # build, erase, flash, open the console
```

`./run.sh` builds, erases the whole flash, flashes and opens the serial console (115200 baud).
Every run is a first boot; add `--keep-nvs` to keep the settings. Without a board the commands
compile and say `compiled, not run`.

### Hardware

| Item | Notes |
|---|---|
| ESP32-C3, ESP32-C6 or ESP32-S3 board | The pad was built with a C3. |
| 2 × 4-wire PWM fans (PC fans) | 12 V or 5 V, with their own supply. |
| MLX90614 sensor module (e.g. GY-906) | I²C, address 0x5A. Use a module with I²C pull-up resistors. |
| WS2812B LED strip | 12 LEDs on the pad. 5 V supply. |
| 2 × 10 kΩ resistors | Optional: external pull-ups on the tach lines (see [doc/WIRING.md](doc/WIRING.md)). |
| Power supply | For the fans and the strip. Connect all grounds together. |

### Pins per chip

| Signal | C3 (the built pad) | C6 | S3 |
|---|---|---|---|
| Fan 1 PWM / tach | GPIO 3 / GPIO 0 | GPIO 3 / GPIO 0 | GPIO 4 / GPIO 5 |
| Fan 2 PWM / tach | GPIO 10 / GPIO 1 | GPIO 10 / GPIO 1 | GPIO 6 / GPIO 7 |
| Sensor SDA / SCL | GPIO 4 / GPIO 5 | GPIO 4 / GPIO 5 | GPIO 8 / GPIO 9 |
| Strip data | GPIO 6 | GPIO 6 | GPIO 48 |

The fan and sensor pins are first-boot defaults (C3 wiring on every chip; an S3 build passes its
pins as defines). The strip data pin is set per chip in `Config.h`. Full wiring and the defines:
[doc/WIRING.md](doc/WIRING.md).

### First boot

The console asks a few questions. Answer them in the console, or run
`build/tools/.venv/bin/python -m xewe provision` to answer them from your settings.

1. Name the device.
2. Wi-Fi: pick your network, enter the password.
3. The strip: no clock line, WS2812B from the chipset menu, 5 V, one strip of 12 LEDs, then the
   colour-order check (the strip shows green, then red; answer what you see).

The device then stores its defaults: both fans, the sensor pins, the curve
22 °C → 0 %, 27 °C → 100 %, 32 °C → 100 %, cold colour `#00FFFF` (cyan), hot colour `#FF0000` (red).
The boot log prints the IP address. Open `http://<device-ip>/`.

Check the parts:

```text
$mlx90614 read
$fan status
$pad status
```

## Usage

Once the pad has booted, it runs on its own. This is what each part does:

| Part | Behaviour |
|---|---|
| Sensor | MLX90614 reads the object temperature every 500 ms (every 5 s while it does not answer). |
| Fans | Two 4-wire PWM fans at 25 kHz. Once a second the curve sets both to the same speed. |
| Tachometer | Each fan reports its RPM (updated about once a second, rounded to 10 RPM). |
| Fan curve | Up to 16 points of temperature → speed. Straight lines between points. |
| LED strip | Off below the first curve point. Cold colour at the first point, hot colour at the last, a blend in between. Changes at most every 2 s, in visible steps. |
| Fail-safe | No reading from the sensor: the fans run at the last point's speed (100 % by default) and the strip shows the hot colour. |
| Web page | `http://<device-ip>/`: curve editor, live operating point, cold and hot colour pickers. |
| Settings | Curve, colours, fans, pins and strip settings survive a restart and a power cut. |

### Set the fan curve

A curve is a list of points: temperature in °C and fan speed in %. Below the first point the fans
run at the first point's speed. Above the last point they run at the last point's speed. In between
the speed is a straight line.

```text
$fan curve set 30:20,40:60,50:100
$fan curve add 45 80
$fan curve remove 45
$fan curve list
```

More in [doc/FAN_CURVE.md](doc/FAN_CURVE.md).

### Read the LED indicator

The strip follows the same curve. With the default curve and colours:

| Temperature | Strip |
|---|---|
| below 21.5 °C | off |
| 22 °C | cyan `#00FFFF` |
| 27 °C | `#7F7F7F` (half way between 22 °C and 32 °C) |
| 32 °C and above | red `#FF0000` |
| no sensor reading | red |

The strip turns on at the first point and turns off 0.5 °C below it, so it does not flicker at the
edge. While `$pad lights on`, the pad sets the strip's state, Solid mode and colour through the led
module. The led module stores each change, so the pad sends one only for a visible step and at most
every 2 s.

```text
$pad set_cold 0000FF
$pad set_hot FF4000
$led set_brightness 64
$pad lights off      # the strip is left to the $led commands (modes, colours)
$pad lights on       # back to the temperature colour
```

### Use the web page

Open `http://<device-ip>/` (also `/pad`). The command console is at `http://<device-ip>/console`.

| Control | What it does |
|---|---|
| Curve graph | The curve, the colour gradient under it and a white dot at the current temperature and fan speed. Updates every second. |
| Table | One row per point: temperature (0–100 °C) and speed (0–100 %). |
| Add Point / Delete | Adds a point 5 °C after the last one / removes a point. The page keeps at least 2 points. |
| Save Curve | Sends the curve and the colours to the device. Nothing changes on the device before you save. |
| Coldest Color / Hottest Color | Saved as soon as you close the colour picker. |

JSON for scripts: `GET /api/state`, `/fan/data`, `/mlx90614/data`, `/led/data`, `/ui/config`;
`POST /ui/config` with `{"temp_curve":[{"temp":30,"speed":20}],"curve_edge_colors":{"start":"#00FFFF","end":"#FF0000"}}`.

### Commands

Type commands in the serial console or on the `/console` page. `$help` lists every group.

| Group | Commands |
|---|---|
| `$pad` | `set_cold` · `set_hot` · `lights` · `print_json` · `status` · `reset` |
| `$fan` | `curve list\|add\|remove\|set` · `temp` · `add` · `add_w_tach` · `set` · `set_all` · `remove` · `get` · `schema` · `print_json` · `status` |
| `$mlx90614` | `read` · `scan` · `set_addr` · `set_pins` · `set` · `get` · `schema` · `print_json` · `status` |
| `$led` | `turn_on` · `turn_off` · `set_brightness` · `set_rgb` · `set_mode` · `set_length` · `set` · `get` · `status` |
| `$wifi` | `connect` · `disconnect` · `scan` |
| `$system` | `restart` · `info` · `status` · `reset` (factory reset) |

Every command with its arguments: [doc/COMMANDS.md](doc/COMMANDS.md).

## Status

Version 1.0.0 (`xewe.toml`) builds for ESP32-C3, -C6 and -S3. It has not yet run on the assembled
pad. Use [tests/MANUAL.md](tests/MANUAL.md) for the first test on real hardware.

Known limits:

- One data pin for the strip, fixed at build time. The pad's two strips share it: wire both inputs
  to the same pin, or chain them and set `$led set_length 24`.
- Each strip change is a flash write in the led module; the pad limits them to one per 2 s at most.
- The tachometer input has no glitch filter. A noisy tach line freezes the RPM value instead of
  showing a wrong one. An external 10 kΩ pull-up to 3.3 V helps.
- If no temperature reading arrives for `stale_ms` after boot, the fans go to the curve's highest
  point and `$fan status` says `never received (fail-safe N %)`. The first real reading hands
  control back to the curve.
- The web page edits 0–100 °C. The console accepts -40 to 200 °C.
- `$mlx90614 scan` blocks the console for up to 1.5 s.
- The device joins an existing Wi-Fi network. It cannot start its own access point.

---

## Development

```sh
./setup.sh
build/tools/.venv/bin/python -m xewe build --chip c3     # or --all-chips
build/tools/.venv/bin/python -m xewe test --unit-only
build/tools/.venv/bin/python -m xewe format
build/tools/.venv/bin/python -m xewe brand-lint .
```

CI runs from [.github/workflows/ci.yml](.github/workflows/ci.yml). The release procedure is in
[scripts/README.md](scripts/README.md).

| What | Where |
|---|---|
| Wiring, pins, build-time defines | [doc/WIRING.md](doc/WIRING.md) |
| The fan curve and the strip colour | [doc/FAN_CURVE.md](doc/FAN_CURVE.md) |
| Every command | [doc/COMMANDS.md](doc/COMMANDS.md) |
| Troubleshooting | [doc/TROUBLESHOOTING.md](doc/TROUBLESHOOTING.md) |
| Repositories, setup, `Config.h`, the `xewe` tools | [doc/XEWE_OS_ECOSYSTEM.md](doc/XEWE_OS_ECOSYSTEM.md) |
| Writing a module, the naming rules | [doc/WRITING_A_MODULE.md](doc/WRITING_A_MODULE.md) |
| Unit tests, board tests, CI | [doc/TESTING.md](doc/TESTING.md) |

## Layout

- [src/](src/README.md) holds the pad's three modules: [Fan](src/Fan/README.md),
  [Mlx90614](src/Mlx90614/README.md) and [PadWeb](src/PadWeb/README.md).
- [tests/](tests/README.md) holds the unit tests, the board tests and the manual hardware test.
- [scripts/](scripts/README.md) holds the release script.
- [doc/](doc/) holds the guides listed above.
- `xewe-chill-os.ino` is the sketch, `Config.h` the configuration, `xewe.toml` the manifest and
  `setup.sh` the installer. XeWeCore, the wifi, web-interface and led modules and the `xewe`
  command are fetched into `build/` by `./setup.sh`.

## License

GPL-3.0-only, see [LICENSE.txt](LICENSE.txt).
