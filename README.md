# XeWe OS Laptop Cooling Pad v2 — temperature-driven fans and LEDs on ESP32

Personal project (XeWe Labs) · v1 2026-03-21 → 2026-05-26, v2 port 2026-10-09, modules extracted 2026-10-09 (CP1/CP10) · Solo: Max Dokukin · Status: v2 compile-verified, not yet run on the pad

Firmware for a DIY laptop cooling pad on the unified XeWe OS: the
[xewe-os](https://github.com/xewe-labs/xewe-os) template, [XeWeCore](https://github.com/xewe-labs/xewe-os-core)
2.x and five modules from [xewe-os-modules](https://github.com/xewe-labs/xewe-os-modules). An
MLX90614 infrared sensor is read every 500 ms; once a second the `fan` module's temperature → speed
curve sets two 4-wire PWM fans, and a WS2812B strip glides from a cold to a hot colour along the same
curve. Everything is changeable from the serial CLI or the pad's web page at `/pad`.

The project itself is one folder of code, `src/PadWeb/` (the glue and the page); everything else is a
repo module.

## Modules

`xewe.toml` `[modules] selected = ["wifi", "web-interface", "fan", "mlx90614", "led"]`; `./setup.sh`
installs them into `build/` and generates `src/Modules.h` (`Wifi wifi(os); WebInterface
web_interface(os, wifi); Fan fan(os); Mlx90614 mlx90614(os); Led led(os);`).

| Module (id) | Role in the pad | README |
|---|---|---|
| `wifi` (`wifi`) | station mode, keeps the connection | `build/modules/src/Wifi/` · repo `modules/wifi` |
| `web-interface` (`web_interface`) | HTTP server; command page at `/console`, PadWeb adds `/pad` and takes `/` (root hook) | repo `modules/web-interface` |
| `fan` (`fan`) | 2 PWM fans + tachometers, **the fan curve** (`$fan curve ...`), fed by `set_temperature()` | repo `modules/fan` |
| `mlx90614` (`mlx90614`) | the IR sensor, bounded bus scan, listeners | repo `modules/mlx90614` |
| `led` (`led`) | the WS2812B strip (FastLED, 50 fps render task); PadWeb drives it with `led.fill()` | repo `modules/led` |
| **PadWeb** (`pad`, project code) | the glue below + the `/pad` page and JSON API | `src/PadWeb/` |

## The glue (`src/PadWeb/`)

`PadWeb pad_web(os, fan, mlx90614, led);` is the only declare in the sketch. It is a project-local
`xewe::Module` (id `pad`, NVS `pad/data`, cannot be disabled, asks nothing at first boot):

1. **Sensor → curve.** PadWeb is an `Mlx90614Listener` and registers itself in its constructor
   (`mlx90614.listeners.add(this)`, the core 2.1 `ListenerSet`). Every poll calls `fan.set_temperature(online ? object : NaN, this)`.
   Chosen over a link in `setup()` because the listener needs an object anyway and PadWeb already
   holds both references. The `fan` module knows nothing about the sensor: offline (NaN) or a source
   silent for 10 s → every fan at the last point's speed (fail-safe).
2. **Strip colour along the curve.** Every 20 ms: on/off from the curve's first point with 0.5 °C
   hysteresis, the colour from `curve_math::led_colour` (cold at the first point, hot at the last, a
   blend between; NaN = hot), an exponential glide (`Glide.h`, the old `argb` glide incl. the R1 clamp
   of `k` to (0, 1]), shown with `led.fill()` (RAM only, no NVS write per frame). "Off" is a black
   fill, not `led.set_state(false)` (that one is persisted). `$pad lights off` calls
   `led.clear_fill()` and the `led` module's own modes take over; `$pad lights on` takes the strip back.
   While lights are on, a `$led mode set` or `$led fill off` lasts one frame.
3. **Web page.** `attach(web_interface)` in `setup()` adds `/pad` and the JSON routes and hands `/` to
   the pad page (`web_interface.set_root(...)`); the module's command console moves to `/console`.

Colours (`#RRGGBB`, with or without `#`, parsed by the core's `xewe::str::parse_hex_color`) and the lights switch live in `pad/data` (FlexData,
schema 1; a foreign schema is never overwritten at boot). The curve itself lives in the `fan` module
(`fan/curve`).

## What changed from 0.0.12

| | 0.0.12 (old tree) | 2.0.0 (this project) |
|---|---|---|
| Framework | private copies of Module, SerialPort, CommandParser, Nvs, System, Wifi, WebInterface (~2.8 k lines) | XeWeCore 2.x + repo modules |
| Pad code | `Fan`, `MLX90614`, `ARGB`, `TempController`, web parts in `src/Modules/` | repo modules `fan` (with the curve), `mlx90614`, `led`; project code `src/PadWeb/` only |
| CLI groups | `$fan`, `$mlx90614` (help said `$temp`), `$argb`, `$tempcontroller` | `$fan` (curve: `$fan curve ...`), `$mlx90614`, `$led`, `$pad` |
| Settings | NVS strings per item | FlexData blobs with a `schema` field: `fan/data`, `fan/curve`, `mlx90614/data`, `pad/data`; `led` keys |
| Pins | hard-coded C3 pins | module build defines with per-chip defaults (below), then NVS / commands |
| LEDs | Adafruit NeoPixel, two strips on two pins | `led` module (FastLED 3.10.3), **one strip per firmware** (both strips on one data pin) |
| Web page | replaced `/` of a forked web interface | `/` and `/pad` on the `web-interface` server (root hook); the module's command page at `/console` |
| Build | `build/*.sh`, `required_libraries.txt`, release matrix | `./setup.sh`, `xewe build/flash/test`, `xewe.toml`; version 2.0.0 |
| Flash wear | `fan.set_all` wrote NVS every second | control loop and LED frames are RAM only |
| Licence | PolyForm Noncommercial + No-AI addendum | **GPL-3.0-only** (owner decision CP1, 2026-10-09) |

Dropped: hotspot (AP) mode (the `wifi` module is station-only), the fixed device name "XeWe Laptop
Chiller" (asked at first boot), the Jinja catch-all CSS route, `static/firmware` 1.0.0 binaries, the
old framework docs, the second LED data pin.

## First five minutes

```sh
export XEWE_TOOLS_SOURCE=…/xewe-os-tools XEWE_CORE_SOURCE=…/xewe-os-core XEWE_MODULES_SOURCE=…/xewe-os-modules  # development; omit to use the lock refs
./setup.sh                                                   # modules from xewe.toml [modules] selected
X=build/tools/.venv/bin/python
$X -m xewe build --all-chips                                  # c3, c6, s3
$X -m xewe build --chip c3 --define LED_PIN_DATA=6 --define LED_COUNT=12   # the real pad (strip pin)
./run.sh --chip c3                                            # build, flash, serial console
$X -m xewe test --no-board                                    # unit tests + "compiled, not run"
$X -m xewe test --unit-only                                   # unit tests only, seconds
```

The modules come from the modules repo at `[modules] ref` (`v0.2.0`); `fan` and `mlx90614` are new
since that tag, so until the repo is tagged again set `XEWE_MODULES_SOURCE` (or bump the ref).
`xewe.toml [libraries]` pins ArduinoJson 7.4.3 (`fan`, `mlx90614`, PadWeb) and FastLED 3.10.3
(`led`); the lock wins over the modules repo's `libraries.toml`.

Agents: compile-only sessions export `XEWE_NO_BOARD=1` (or pass `--no-board`): `build` and `test`
never probe a serial port, `flash`/`provision`/`boards` exit 4.

## First boot

`fan` and `mlx90614` ask once "Would you like to enable <name> module?" (`xewe provision` answers
yes); `led` and `pad` ask nothing. On its first boot each module finds no settings and stores its
defaults: two fans with tachometers, the sensor pins and address, the default curve
22 °C → 0 %, 27 °C → 100 %, 32 °C → 100 %, cold `#00FFFF`, hot `#FF0000`, lights on.

**Upgrading resets the pad settings**: neither 0.0.12 keys nor the earlier v2 namespaces
(`fan_curve/data`, `argb/data`) are read (CP6, CP12). Re-enter a custom curve or colours after flashing.

## Pins per chip

Module build defines; `xewe build --define KEY=VALUE` wins. They only matter on a module's first
boot (or after `$<id> reset`); afterwards the pins live in NVS and change with `$fan add/remove`,
`$mlx90614 set_pins/set_addr`. 255 = not connected.

| Define (module) | C3 (the built pad) | C6 | S3 |
|---|---|---|---|
| `FAN1_PWM` / `FAN1_TACH` (fan) | 3 / 0 | 3 / 0 | 4 / 5 |
| `FAN2_PWM` / `FAN2_TACH` (fan) | 10 / 1 | 10 / 1 | 6 / 7 |
| `MLX90614_SDA` / `MLX90614_SCL` / `MLX90614_ADDR` (mlx90614) | 4 / 5 / 0x5A | 4 / 5 / 0x5A | 8 / 9 / 0x5A |
| `LED_PIN_DATA` (led) | 8 (module default) → **pad: 6** | 8 → 6 | 48 (devkit LED) |
| `LED_COUNT` (led) | 60 → **pad: 12** (or `$led set num_led 12`) | 60 | 60 |

- **Strip.** The `led` module drives one strip and its data pin is a FastLED template argument, so it
  is fixed per image: build the pad with `--define LED_PIN_DATA=6` and wire both strips' DIN to GPIO 6
  (they always showed the same colour). `xewe test` cannot pass defines, so the test image uses the
  module default (8 on C3/C6), which is harmless with nothing on it.
- **C3**: the original wiring. GPIO 0 is a tach input, harmless after boot.
- **C6**: the C3 pins avoid the C6's boot strapping pins (8, 9, 15); 4/5 (MTMS/MTDI) matter only with
  a JTAG-select eFuse burnt. GPIO 0/1 double as the 32 kHz crystal pins; a board with that crystal
  needs other tach pins, e.g. `--define FAN1_TACH=18 --define FAN2_TACH=19`. (Datasheets, not a C6.)
- **S3**: 0 and 3 are strapping pins, 19/20 USB, 26-32 flash/PSRAM: the module defaults use free GPIOs.

## Commands

`$help` lists every group; `$help <id>` one group. Every module also has `status` and `reset`;
`fan` and `mlx90614` also `enable`/`disable`.

| Group | Commands (details in each module's README) |
|---|---|
| `$fan` | `add <pwm>` · `add_w_tach <pwm> <tach>` · `set <pwm> <0-255>` · `set_all <0-255>` · `remove <pwm>` · `temp <°C>` · `curve list` · `curve add <°C> <0-100>` · `curve remove <°C>` · `curve set <T:P,T:P,...\|none>` · `print_json` |
| `$mlx90614` | `read` · `scan` (≤ 1.5 s) · `set_addr <hex 0x01-0x7F>` · `set_pins <sda> <scl>` · `print_json` |
| `$led` | `on` · `off` · `brightness <0-255>` · `set <key> <value>` · `fill <rrggbb\|off>` · `mode ...` · `checksum` |
| `$pad` | `set_cold <#RRGGBB>` · `set_hot <#RRGGBB>` · `lights <on\|off>` · `print_json` |

```text
$fan curve add 40.5 50
$fan curve set 22:0,27:100,32:100
$pad set_cold 00FFFF
$led brightness 255
$fan status
```

While the sensor reports (online or offline), the curve rewrites every fan speed once a second, so
`$fan set` is momentary; `$fan curve set none` empties the curve and hands the fans back to manual
control. While `$pad lights on`, the strip shows the curve colour.

## Web page

With WiFi connected the boot log prints the device IP.

| URL | |
|---|---|
| `http://<ip>/`, `http://<ip>/pad` | the pad page (curve editor, colours, live operating point), assets unchanged from 0.0.12; `/` through the web-interface root hook |
| `http://<ip>/console` | the `web-interface` module's command page |
| `GET /api/state` | `{"fan_data","argb_data","sensor_data","ui_config"}` (polled by the page every second; `argb_data` is now the strip state from PadWeb) |
| `GET /fan/data`, `/led/data`, `/mlx90614/data`, `/ui/config` | JSON of fan, strip, sensor, curve + colours |
| `POST /ui/config` | `{"temp_curve":[{"temp","speed"}],"curve_edge_colors":{"start","end"}}`: at most one NVS write per changed blob (`pad/data`, `fan/curve`) |
| `GET /cmd?c=<command>` | run any CLI command (`web-interface` module) |

## Tests

`build/tools/.venv/bin/python -m xewe test` runs the project tests in `tests/` plus the tests of
every selected module (wifi, web-interface, fan, mlx90614, led; the fan curve's 87-check g++ test is
`fan`'s `tests/unit/test_curve.cpp`). Without a board: unit tests run, every `test_compiles` builds
the firmware, every serial test is "compiled, not run". With a board the session flashes once and
needs a provisioned board.

| Project file | What |
|---|---|
| `tests/unit/test_pad.py` | `Glide.h` g++ test (`test_glide.cpp`, 16 checks: R1 clamp, convergence, rounding), header purity, glue wiring by source (listener, `set_temperature`, `led.fill`, no persisted LED setter per frame), one NVS write per blob per POST, the sketch declares only PadWeb, GPL headers |
| `tests/board/test_pad.py` | bare board: `test_status`, `test_colours`, `test_lights_toggle` (`$led status` source mode/fill), `test_offline_sensor_fail_safe` (no sensor → `Curve target: 100 %`, strip lit hot), `test_settings_survive_restart`; `test_closed_loop`: the assembled pad, `XEWE_TEST_PAD_HARDWARE=1` |

Module board tests that need free pins or hardware read `XEWE_TEST_FAN_PIN`, `XEWE_TEST_FAN_PIN2`,
`XEWE_TEST_FAN_PWM_PIN`/`XEWE_TEST_FAN_TACH_PIN`, `XEWE_TEST_MLX_SDA`/`XEWE_TEST_MLX_SCL`,
`XEWE_TEST_MLX_PRESENT` and skip with the reason when unset (see each module's README).

## First hardware session (Mac)

Not run yet: the v2 firmware has never booted. From the project folder, board attached by USB (the
tools pick the port when one board is attached, `--port /dev/cu.usbmodemXXXX` otherwise). Do not set
`XEWE_NO_BOARD` here.

```sh
./setup.sh
X=build/tools/.venv/bin/python

# 1a. the real pad (ESP32-C3): module pin defaults + the strip on GPIO 6
$X -m xewe flash --chip c3 --erase --define LED_PIN_DATA=6 --define LED_COUNT=12
# 1b. or the ESP32-S3 devkit (module S3 defaults are free GPIOs; strip = the devkit LED on 48)
$X -m xewe flash --chip s3 --erase

# 2. answer the first-boot prompts (name, Wi-Fi, timezone, enable fan / mlx90614)
$X -m xewe provision

# 3. the test session (flashes its own test image once, then runs every serial test)
$X -m xewe -v test --chip c3 --require-board
XEWE_TEST_FAN_PIN=12 XEWE_TEST_FAN_PIN2=13 XEWE_TEST_MLX_SDA=14 XEWE_TEST_MLX_SCL=15 \
  $X -m xewe -v test --chip s3 --require-board
XEWE_TEST_MLX_PRESENT=1 XEWE_TEST_PAD_HARDWARE=1 XEWE_TEST_FAN_PWM_PIN=3 XEWE_TEST_FAN_TACH_PIN=0 \
  $X -m xewe -v test --chip c3 --require-board          # the assembled pad

# 4. look at it
./run.sh --chip c3                                      # serial console ($fan status, $pad status, ...)
```

`--require-board` turns a missing board into a failure (exit 4). The test image is built without
`--define`, so on the C3 its strip pin is 8, not 6. `--erase` wipes NVS: provision again afterwards.

## Licence

GPL-3.0-only ([LICENSE.txt](LICENSE.txt)), like XeWeCore, the modules and the xewe-os template:
the owner relicensed the pad's code on 2026-10-09 (MIGRATION.md CP1). Source files carry
`SPDX-License-Identifier: GPL-3.0-only`. The page assets in `src/PadWeb/*_html.h`, `*_js.h`,
`*_css.h` are the 0.0.12 page, unchanged, under the same licence.
