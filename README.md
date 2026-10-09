# XeWe OS Laptop Cooling Pad v2 — temperature-driven fans and ARGB on ESP32

Personal project (XeWe Labs) · v1 2026-03-21 → 2026-05-26, v2 port 2026-10-09 · Solo: Max Dokukin · Status: v2 compile-verified, not yet run on the pad

Firmware for a DIY laptop cooling pad, rebuilt on the unified XeWe OS: the
[xewe-os](https://github.com/xewe-labs/xewe-os) template, [XeWeCore](https://github.com/xewe-labs/xewe-os-core)
2.x and the `wifi` + `web-interface` modules from
[xewe-os-modules](https://github.com/xewe-labs/xewe-os-modules). An MLX90614 infrared sensor is read
every 500 ms; once a second a temperature → fan-speed curve sets two 4-wire PWM fans, and two WS2812B
strips fade from a cold to a hot colour across the same curve. Everything is changeable from the serial
CLI or the pad's web page at `/pad`.

## What changed from 0.0.12

| | 0.0.12 (old tree) | 2.0.0 (this project) |
|---|---|---|
| Framework | private copies of Module, SerialPort, CommandParser, Nvs, System, Wifi, WebInterface (~2.8 k lines) | XeWeCore 2.x + repo modules `wifi`, `web-interface` (pinned in `xewe.lock`, fetched into `build/`) |
| Pad code | `Fan`, `MLX90614`, `ARGB`, `TempController` inside `src/Modules/` | contract-shaped project-local modules `src/Fan/`, `src/Mlx90614/`, `src/Argb/`, `src/FanCurve/` (each with `module.properties`, `README.md`, tests in `tests/`), plus `src/PadWeb/` |
| CLI groups | `$fan`, `$mlx90614` (help said `$temp`), `$argb`, `$tempcontroller` | `$fan`, `$mlx90614`, `$argb`, **`$fan_curve`** (group = module id = NVS namespace) |
| Settings | NVS strings per item (`fan/fan_cfg_<i>`, `temp/*`, `argb/argb_cfg_<i>`, `tempctrl/curve_data`) | one FlexData blob per module, `<id>/data`, with a `schema` field |
| Pins | hard-coded C3 pins in module code | `Config.h` `PAD_*` defaults (per chip), applied on first boot, then NVS; `--define` overrides |
| LED library | Adafruit NeoPixel | **FastLED 3.10.3** (`xewe.lock [libraries]`), per-strip length |
| Web page | replaced `/` of a forked web interface | **`/pad`** on the `web-interface` module's server; `/` stays the module's command page |
| Build | `build/*.sh`, `required_libraries.txt`, release matrix | `./setup.sh`, `xewe build/flash/test`, `xewe.lock`; version 2.0.0 |
| Flash wear | `fan.set_all` wrote NVS every second from the curve loop | control-loop writes are RAM only (`persist = false`) |
| Sensor offline | 2 I2C reads every 500 ms | same fail-safe (255.0 °C → fans at the last point), polls back off to 5 s |

Dropped: the old firmware's own WiFi hotspot mode (the repo `wifi` module has station mode only), the
fixed device name "XeWe Laptop Chiller" (the core asks for a name at first boot), the Jinja catch-all
CSS route, the generic XeWe OS 1.0.0 binaries under `static/firmware`, the old framework docs.

## First five minutes

```sh
export XEWE_TOOLS_SOURCE=…/xewe-os-tools XEWE_CORE_SOURCE=…/xewe-os-core XEWE_MODULES_SOURCE=…/xewe-os-modules  # development (D24); omit to use the lock refs
./setup.sh --modules wifi,web-interface [--arduino-data DIR]
build/.venv/bin/python -m xewe build --all-chips          # c3, c6, s3
./run.sh --chip c3                                         # build, flash, serial console
build/.venv/bin/python -m xewe test --chip c3              # no board: host tests + "compiled, not run"
```

The four pad modules are not in the modules repo (see Licence), so `setup.sh` selects only `wifi` and
`web-interface`; the pad modules are declared by hand in `xewe-os-laptop-cooling-pad.ino`, after the
generated `Modules.h`.

**FastLED** (used by `argb`): `xewe.lock [libraries]` pins `FastLED = 3.10.3` explicitly, and the
lock wins. The modules repo's library catalogue (`xewe-os-modules/libraries.toml`, also FastLED
3.10.3) is only the fallback `setup` uses for a module's `depends_libraries` the lock does not pin;
keep the explicit pin here, because the pad modules are not in that repo and declare no
`depends_libraries` for `setup` to resolve.

### Agents

- Compile-only sessions (no board may be touched) export **`XEWE_NO_BOARD=1`** (or pass
  `--no-board`): `build`, `test` and `flash` then never probe or open a serial port; `test` reports
  every serial test `compiled, not run (board access disabled ...)` and exits 0; `flash`, `provision`
  and `boards` exit 4.
- `xewe test -v` (flag after the command) now works, as does `xewe -v test`.
- The host tests need only Python and `g++`: `xewe test --host-only` (seconds, no firmware build).

## First boot

There are **no pad-specific questions**. Like every disableable XeWe OS module, each pad module asks
once "Would you like to enable <name> module?" (`xewe provision` answers yes). On its first enabled
boot each module finds no settings and stores the `Config.h` defaults: two fans with tachometers, the
sensor pins and address, two strips of `PAD_ARGB_LEN` LEDs, and the default curve
22 °C → 0 %, 27 °C → 100 %, 32 °C → 100 %, cold `#00FFFF`, hot `#FF0000`.

**Upgrading a pad from 0.0.12 resets the pad settings**: the old NVS keys are not read (MIGRATION.md
CP6). Re-enter a custom curve, colours or pins after flashing. The old keys stay in flash unused.

## Pins per chip

Defaults come from `Config.h` (`#ifndef`-guarded, so `--define` wins). 255 = not connected.

| Define | C3 (the built pad) | C6 | S3 (default in Config.h) |
|---|---|---|---|
| `PAD_FAN1_PWM` / `PAD_FAN1_TACH` | 3 / 0 | 3 / 0 | 4 / 5 |
| `PAD_FAN2_PWM` / `PAD_FAN2_TACH` | 10 / 1 | 10 / 1 | 6 / 7 |
| `PAD_MLX_SDA` / `PAD_MLX_SCL` / `PAD_MLX_ADDR` | 4 / 5 / 0x5A | 4 / 5 / 0x5A | 8 / 9 / 0x5A |
| `PAD_ARGB1_PIN` / `PAD_ARGB2_PIN` / `PAD_ARGB_LEN` | 6 / 7 / 12 | 6 / 7 / 12 | 10 / 11 / 12 |

- **C3**: the original wiring. GPIO 0 is a tach input, harmless after boot.
- **C6**: no override needed: the C3 pins avoid the C6's boot-mode strapping pins (8, 9, 15); 4/5
  (MTMS/MTDI, the I2C pins) matter only with a JTAG-select eFuse burnt. GPIO 0/1 double as the
  32 kHz crystal pins; a board that fits that crystal needs other tach pins, e.g.
  `--define PAD_FAN1_TACH=18 --define PAD_FAN2_TACH=19`. (From the datasheets, not checked on a C6.)
- **S3**: GPIO 0 (BOOT) and 3 are strapping pins, 19/20 are USB, 26-32 flash/PSRAM, 48 the devkit LED.
  `Config.h` therefore defaults to free GPIOs on the S3 (`xewe test` builds without `--define`, so an
  S3 test session gets safe pins too). The same set as explicit defines, for a build or another board:

```sh
build/.venv/bin/python -m xewe build --chip s3 \
  --define PAD_FAN1_PWM=4 --define PAD_FAN1_TACH=5 --define PAD_FAN2_PWM=6 --define PAD_FAN2_TACH=7 \
  --define PAD_MLX_SDA=8 --define PAD_MLX_SCL=9 --define PAD_ARGB1_PIN=10 --define PAD_ARGB2_PIN=11
```

Defines only matter on a module's first boot (or after `$<id> reset`); afterwards the pins live in NVS
and change with `$fan add/remove`, `$mlx90614 set_pins/set_addr`, `$argb add/remove/set_length`.

## Commands

`$help` lists every group; `$help <id>` one group. Every module also has `status`, `reset`, `enable`,
`disable`.

| Module (id) | Commands |
|---|---|
| Fan (`fan`) | `add <pwm>` · `add_w_tach <pwm> <tach>` · `set <pwm> <0-255>` · `set_all <0-255>` · `remove <pwm>` · `print_json` |
| MLX90614 (`mlx90614`) | `read` · `scan` · `set_addr <hex 0x01-0x7F>` · `set_pins <sda> <scl>` · `print_json` |
| ARGB (`argb`) | `add <pin>` · `remove <pin>` · `set_length <pin> <1-300>` · `set_state <pin> <1\|0>` · `set_rgb <pin> <r> <g> <b>` · `print_json` |
| Fan Curve (`fan_curve`) | `add_point <°C> <0-100>` · `remove_point <°C>` · `set_cold #RRGGBB` · `set_hot #RRGGBB` · `print_json` |

```text
$fan_curve add_point 40.5 50
$fan_curve set_cold #00FFFF
$fan status
$mlx90614 status
$argb set_rgb 6 255 0 0
```

While `fan_curve` is enabled it rewrites every fan speed once a second and, with `argb` enabled, the
strip state and colour; manual `$fan set` / `$argb set_rgb` are then momentary. `fan_curve` requires
`fan` and `mlx90614` (disabling either disables it); `argb` is optional. Module details: the README in
each `src/<Folder>/`.

## Web page

With WiFi connected (the `wifi` module) and `web-interface` enabled, the boot log prints the device IP.

| URL | |
|---|---|
| `http://<ip>/pad` | the pad page (curve editor, colours, live operating point), assets unchanged from 0.0.12 |
| `http://<ip>/` | the `web-interface` module's command page |
| `GET /api/state` | `{"fan_data","argb_data","sensor_data","ui_config"}` (polled by the page every second) |
| `GET /fan/data`, `/argb/data`, `/mlx90614/data`, `/ui/config` | each module's JSON |
| `POST /ui/config` | `{"temp_curve":[{"temp","speed"}],"curve_edge_colors":{"start","end"}}` replaces the curve (one NVS write) |
| `GET /cmd?c=<command>` | run any CLI command (`web-interface` module) |

## Tests

`build/.venv/bin/python -m xewe test` runs `tests/` (the pad modules) plus the `wifi` and
`web-interface` module tests. Without a board: host tests run, every `test_compiles` builds the
firmware, every serial test is reported "compiled, not run". With a board the session flashes once and
needs a **provisioned** board (first boot answered, e.g. `xewe provision`).

| File | Bare board (nothing wired) | Needs env / hardware |
|---|---|---|
| `test_fan.py` | `test_status`, `test_set_rejects_out_of_range`, `test_argc_error` | `test_add_set_remove`: `XEWE_TEST_FAN_PIN` (free GPIO, not a `PAD_FAN*` pin); `test_rpm_reading`: a real fan on `XEWE_TEST_FAN_PWM_PIN` / `XEWE_TEST_FAN_TACH_PIN` |
| `test_mlx90614.py` | `test_status`, `test_set_addr_validation` (restores the address), `test_set_pins_rejects_same_pin` | `test_scan_bare_bus`: `XEWE_TEST_MLX_SDA` / `XEWE_TEST_MLX_SCL` (restores the pins; a scan without pull-ups can take up to a minute); `test_read_temperature`: a sensor, `XEWE_TEST_MLX_PRESENT=1` |
| `test_argb.py` | `test_status`, `test_rejects_bad_values` | `test_add_rgb_remove`: `XEWE_TEST_ARGB_PIN` (C3/C6 have 2 RMT channels, both used by the default strips: run it on an S3, or remove a strip first); `test_colour_on_strip`: `XEWE_TEST_ARGB_VISUAL=1` |
| `test_fan_curve.py` | `test_status`, `test_add_remove_point`, `test_colours`, `test_offline_sensor_fail_safe`, `test_settings_survive_restart` (`$system restart`) | `test_closed_loop`: the assembled pad, `XEWE_TEST_PAD_HARDWARE=1` |

Each file also has host tests (properties vs source). `test_fan_curve.py::test_curve_math_host`
compiles `tests/host/test_curve_math.cpp` with the host `g++ -std=c++17 -Wall -Wextra -Werror` and
runs it: 64 checks of `src/FanCurve/CurveMath.h`, the pure curve logic (interpolation, clamps, sorted
and duplicate-point validation, schema version, LED hysteresis, colours); it skips when `g++` is
missing. Tests that need wiring skip with the reason when their variable is unset.

## First hardware session (Mac)

Not run yet: the v2 firmware has never booted. From the project folder on the Mac, board attached
by USB (find the port with `ls /dev/cu.usbmodem*`; the tools pick it automatically when one board is
attached, add `--port /dev/cu.usbmodemXXXX` otherwise). Do not set `XEWE_NO_BOARD` here.

```sh
./setup.sh --modules wifi,web-interface
X=build/.venv/bin/python

# 1a. the real pad (ESP32-C3): default Config.h pins
$X -m xewe flash --chip c3 --erase       # --erase wipes NVS for a clean first boot; drop it to keep provisioning
# 1b. or the ESP32-S3 devkit: free GPIOs as explicit defines (same values as the Config.h S3 defaults)
$X -m xewe build --chip s3 \
  --define PAD_FAN1_PWM=4 --define PAD_FAN1_TACH=5 --define PAD_FAN2_PWM=6 --define PAD_FAN2_TACH=7 \
  --define PAD_MLX_SDA=8 --define PAD_MLX_SCL=9 --define PAD_ARGB1_PIN=10 --define PAD_ARGB2_PIN=11
$X -m xewe flash --chip s3 --no-build --erase

# 2. answer the first-boot prompts (name, Wi-Fi, timezone, the 4 pad "enable?" questions)
$X -m xewe provision                     # XEWE_* settings: see `xewe provision --help`

# 3. the test session (flashes its own test image once, then runs every serial test)
$X -m xewe test --chip c3 --require-board -v
#    on the S3 (s3 test image), with free pins for the add/remove tests:
XEWE_TEST_FAN_PIN=12 XEWE_TEST_ARGB_PIN=13 XEWE_TEST_MLX_SDA=14 XEWE_TEST_MLX_SCL=15 \
  $X -m xewe test --chip s3 --require-board -v
#    on the assembled pad, also the hardware tests:
XEWE_TEST_MLX_PRESENT=1 XEWE_TEST_PAD_HARDWARE=1 XEWE_TEST_ARGB_VISUAL=1 \
XEWE_TEST_FAN_PWM_PIN=3 XEWE_TEST_FAN_TACH_PIN=0 \
  $X -m xewe test --chip c3 --require-board -v    # fan 1 of the C3 pad

# 4. look at it
./run.sh --chip c3                       # build, flash, serial console ($fan_curve status, ...)
```

`--require-board` turns a missing board into a failure (exit 4) instead of `compiled, not run`. The
test session needs a provisioned board, so step 2 comes before step 3 (and again after any `--erase`).
The test session's image has the `Config.h` defaults (no `--define`), which on the S3 are the same
free GPIOs as 1b.

## Licence

Two licences live in this tree:

- The pad's own code (`src/Fan/`, `src/Mlx90614/`, `src/Argb/`, `src/FanCurve/`, `src/PadWeb/`,
  `tests/`) is the owner's 0.0.12 code, ported: **PolyForm Noncommercial 1.0.0 + No AI Use Addendum
  v1.0**, see [LICENSE.md](LICENSE.md) and [LICENSE-NO-AI.md](LICENSE-NO-AI.md) (copied unchanged).
  Those files carry `SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI`.
- The xewe-os template files (`setup.sh`, `run.sh`, `Config.h`, the sketch skeleton, `AGENTS.md`,
  `ARCHITECTURE.md`) are GPL-3.0-only ([LICENSE.txt](LICENSE.txt)), as are XeWeCore and the repo
  modules fetched into `build/`.

Whether the pad modules move into the GPL `xewe-os-modules` repo (relicensing) is the owner's call
(MIGRATION.md CP1); each `src/<Folder>/` is already shaped so that the move is a `mv` into
`modules/<slug>/` (the repo validator passes on a staged copy).
