# Migration report: xewe-os-laptop-cooling-pad 0.0.12 → v2 (2.0.0)

Agent C1, night run 2026-10-08/09 (22:18–22:40 PDT). Inputs: `wip/xewe-os-laptop-cooling-pad/`
(read-only), its `MIGRATION.md` (CP1–CP9, followed) and `MIGRATION-SURVEY.md`. Built from the working
trees (D24): `done/xewe-os-core` (branch `tests/round-1`), `done/xewe-os-modules`, `done/xewe-os-tools`.
Logs: `test_logs/2026-10-09-night-run/cooling-pad/`. Nothing written outside this folder and the log
folder; no git writes.

## Ported

| Old | New | Notes |
|---|---|---|
| `Hardware/Fan` (384 lines) | `src/Fan/` (444) — id `fan`, `Fan fan(os, {...})` | RPM filter unchanged; FlexData list; pin validation; `set_all(speed, persist)` |
| `Sensors/Temperature/MLX90614` (389) | `src/Mlx90614/` (393) — id `mlx90614`, name `MLX90614` | no pin prompt; 255 = unconfigured; offline back-off to 5 s; boot prints one line instead of a bus scan |
| `Hardware/ARGB` (431, NeoPixel) | `src/Argb/` (508) — id `argb`, FastLED 3.10.3 | per-strip length + `set_length`; strip count capped at the RMT TX channels |
| `Software/TempController` (406) | `src/FanCurve/` (421) — id `fan_curve` (was `$tempcontroller` / NVS `tempctrl`) | requires fan + mlx90614, argb optional; `set_curve()`; max 16 points; colours validated `#RRGGBB` |
| `Software/WebInterface` pad parts (313 + 760 asset lines) | `src/PadWeb/` (119 glue + 760 assets copied verbatim) | page at `/pad`; same JSON routes; POST `/ui/config` is one NVS write |
| `SystemController`, `.ino` | `xewe-os-laptop-cooling-pad.ino` (51) | template sketch + 4 local declares after `Modules.h`; routes attached before `os.begin()` |
| `Config.h` | `Config.h` (template + `PAD_*` defaults, S3 set under `CONFIG_IDF_TARGET_ESP32S3`) | |
| — | `tests/test_{fan,mlx90614,argb,fan_curve}.py` (329) | |

Line counts: old pad-specific C++ 1923 lines (Fan, MLX90614, ARGB, TempController, WebInterface .h/.cpp)
plus ~2.8 k lines of framework copies; v2 pad C++ 1885 lines (incl. PadWeb glue) + 760 asset lines,
no framework copies.

Each local module folder has `<Folder>.h/.cpp`, `module.properties` (same 12 keys as the repo,
`version=0.1.0`, `repo=` the future `xewe-os-modules/tree/main/modules/<slug>` URL), `README.md`; its
tests are in the project `tests/`. Moving one into the modules repo = `src/<Folder>/{module.properties,
README.md}` → `modules/<slug>/`, sources → `modules/<slug>/src/<Folder>/`, `tests/test_<id>.py` →
`modules/<slug>/tests/test_<slug>.py`, then replace the hand-written declare in the `.ino` (keep the
Config.h argument) — after the CP1 licence decision.

## Dropped / deferred

- Dropped: framework copies (Module, SerialPort, CommandParser, Nvs, System, Wifi, WebInterface,
  XeWeStringUtils, Debug.h, src_templates), `build/*.sh`, `required_libraries.txt`, release matrix,
  `static/firmware` (generic 1.0.0 images), old framework docs, the Jinja catch-all CSS route.
- Lost with the old WiFi fork: **hotspot (AP) mode** — the repo `wifi` module is station-only.
  Deferred: a hotspot feature belongs in the `wifi` module.
- Lost: fixed device name "XeWe Laptop Chiller" (core asks at first boot; `xewe provision` sets it).
- Not copied: `static/media` screenshots (~3.2 MB, README v1 only). Deferred to the human.
- Not migrated: old NVS data (CP6). First boot of v2 stores defaults; old keys stay unused.

## API deltas hit

- `Module(controller, name, desc, nvs_key, ...)` → `xewe::Module(host, id, name, description, ...)`;
  id = CLI group = NVS namespace, so `$mlx90614` and `$fan_curve` now match their namespaces.
- `commands_storage.push_back` + `sscanf` → `register_command({..., argc, [this](xewe::span<...>)})`
  + `xewe::str::parse_int/parse_float`; argc is enforced by the CLI (`Argument count mismatch`).
- `begin_routines_*(const ModuleConfig&)` → no-arg overrides; configs are constructor structs filled
  from `Config.h` in the `.ino` (designated initializers).
- `controller.nvs.read_str/write_uint8` per item → `os.nvs.read_flex/write_flex(id, "data", ...)`
  with a `schema` first field; keys ≤ 15 chars (`data`).
- `status()` now starts with `Module::status(false)` → first line `<Name> module enabled|disabled`.
- Cross-module `controller.fan` → injected references (`FanCurve(os, fan, mlx90614, argb)`,
  `PadWeb(fan, mlx90614, argb, fan_curve)`), `add_requirement` for fan and mlx90614 only.
- `cli`/`controller` word bans: the FastLED controller pointer is named `driver` (the repo validator
  flags any `controller` token).
- Name clash found at compile time: a helper namespace `fan_curve` collided with the global
  `fan_curve` object → renamed `curve_math`.

## Behaviour changes worth knowing

- Every pad module `can_be_disabled = true` (CP7), so first boot asks the generic "enable?" question
  once per module (4 extra y/n prompts; `xewe provision` answers them). No pad-specific prompts.
- Flash wear fix: the curve loop no longer writes NVS every second (old `fan.set_all` saved each call).
- `fan_curve` keeps driving the fans while `argb` is disabled (old code had no such case).
- Disabling `fan` or `mlx90614` disables `fan_curve` (core cascade); fans then keep their last speed.
- `$argb set_state` accepts only 0/1; `$fan set` rejects > 255 (old code truncated); `set_addr`
  rejects 0x00 and > 0x7F and non-hex.

## Build (clean, `xewe build --all-chips --clean`, default pins)

| Chip | Sketch | Flash (of 2 MB app, no_ota) | Globals | Warnings |
|---|---|---|---|---|
| C3 | 1 468 217 B | 70 % | 47 800 B (14 %) | 0 |
| C6 | 1 487 746 B | 70 % | 55 324 B (16 %) | 0 |
| S3 | 1 346 040 B | 64 % | 59 000 B (18 %) | **6** |

The 6 S3 warnings are all inside FastLED 3.10.3 itself
(`third_party/yves/I2SClockLessLedDriveresp32s3.h:397-398`, `-Wdeprecated-declarations` for
`psram_trans_align` / `sram_trans_align` against esp32 core 3.3.12's `esp_lcd_io_i80.h`). That file is
compiled unconditionally on S3; our code adds none. Not fixed (would need a FastLED patch or a newer
FastLED); the led-os migration pins the same FastLED and will see the same 6.
`xewe build --chip s3` with the explicit S3 `--define` set (README) also succeeds (1 346 040 B, 64 %;
its "0 warnings" is only because FastLED came from the warm cache).

## Tests

`xewe test --chip c3`, no board (serial devices hidden, see Risks): **exit 0** —
`8 host passed, 24 compiled, not run, 0 failed, 6 hardware passed` (the 6 are `test_compiles`: 4 pad
+ wifi + web-interface). Pad tests: 30 = 4 `test_compiles` + 6 host + 20 serial; of the serial ones,
4 need real hardware (RPM, temperature, strip colour, closed loop) and skip with the reason,
4 need a free-pin env var (`XEWE_TEST_FAN_PIN`, `XEWE_TEST_ARGB_PIN`, `XEWE_TEST_MLX_SDA/SCL`), the
rest run on a bare provisioned board (CP9 list: fan add/set/remove, ARGB add/set_rgb, MLX address
validation + empty-bus scan, curve add/set/remove, settings across `$system restart`).

## Validator

`done/xewe-os-modules/tools/validate.py` takes no path (it validates its own repo). It was run
unmodified against a staging copy laid out as `modules/<slug>/` in the session scratchpad (loaded with
importlib, `REPO` pointed at the copy, `--harness` = this project): **4 modules, 0 errors, 0 warnings**
(tools rules + repo rules incl. `class`, `declare-os`, `name`, `id-source`, `files`, `tests`, `repo`,
`depends_libraries` against this lock's `[libraries]`, `source`, `stray`; only the `MODULES.md` index
check was skipped). First run found 11 `controller` hits in Argb (FastLED pointer), fixed.

## Risks and unverified items

- **Board touched once by mistake.** The first `xewe test --chip c3` run found the attached S3 on
  `/dev/ttyACM0`: the tools probed it with esptool (chip id, 22:33:47 PDT, recorded in
  `build/boards.toml`) and then failed every serial test with "board is s3, tests were built for c3".
  No flash, no erase, no serial session. The port was present again right after. The reported run was
  repeated inside `bwrap --dev /dev` (no serial devices visible) with `XEWE_PORT=` so it is a true
  no-board run. Log of the first attempt: `test-c3-first-attempt-board-probed.log`.
- Nothing ran on hardware: FastLED runtime-pin driver selection (one template instance per valid
  GPIO, `showLeds` per strip, `setLeds(nullptr, 0)` on remove), RMT channel use next to WiFi, LEDC
  25 kHz on C3/S3, tach ISR, Wire restart in `set_pins`, the `/pad` page and POST `/ui/config`
  are compile-verified only.
- The S3 `Config.h` defaults (`CONFIG_IDF_TARGET_ESP32S3` via `<sdkconfig.h>`) are checked by reading
  the S3 sdkconfig, not by a run. This adds per-chip defaults beyond "S3 by `--define`" because
  `xewe test`'s `compiled` fixture cannot pass defines.
- C6 pin notes are from datasheets, not a C6.
- I2C scan on a bare bus can block the console for tens of seconds (one bus timeout per address);
  the test allows 60 s.
- `test_add_rgb_remove` cannot run on C3/C6 with both default strips present (2 RMT TX channels).
- `PROJECT_URL` set to `https://github.com/xewe-labs/xewe-os-laptop-cooling-pad` (assumed repo URL).
- `depends_modules` of fan-curve lists `argb` (the declare passes it) though it is not a runtime
  requirement.
- Template docs `AGENTS.md` / `ARCHITECTURE.md` are the generic xewe-os ones, unchanged.

## Open for the human

CP1 relicensing / move into the modules repo; hotspot mode; whether `mlx90614` becomes a generic
temperature-sensor module; the FastLED S3 warnings (accept, patch, or bump FastLED); `static/media`.

## Verification (C2)

Agent C2, 22:38–22:45 PDT. Logs: `test_logs/2026-10-09-night-run/cooling-pad/c2-*`. No git writes,
no flash, no erase; the board was never opened by C2 (it was gone before C2's first board command).

### Compile side (independent re-run)

- Clean setup: `build/` deleted, `./setup.sh --modules wifi,web-interface --arduino-data …/.toolchain/arduino15`
  with the three `XEWE_*_SOURCE` working trees → ok (`c2-setup.log`).
- `xewe build --all-chips --clean` → exit 0, sizes identical to C1's table: C3 1 468 217 B (70 %),
  C6 1 487 746 B (70 %), S3 1 346 040 B (64 %); warnings 0 / 0 / 6 (`c2-build-all-chips-clean.log`,
  `c2-compile-{c3,c6,s3}.log`). All 6 S3 warnings are `-Wdeprecated-declarations` in
  `FastLED/src/third_party/yves/I2SClockLessLedDriveresp32s3.h:397-398`, included from FastLED's own
  `platforms/esp/32/clockless_i2s_esp32s3.cpp`; none come from project code.
- Validator (own staging): full copy of `done/xewe-os-modules` + the 4 pad modules as
  `modules/{fan,mlx90614,argb,fan-curve}/` (tests renamed `test_<slug>.py`), `--write-index` run on the
  copy so the index check is included this time, then `validate.py --harness <this project>` →
  **12 modules, 0 errors, 0 warnings** (`c2-validate.log`; no id/slug clash with the repo's modules).
- `xewe test --chip c3`, board hidden (`bwrap --dev /dev`, `XEWE_PORT=`) → exit 0,
  `8 host passed, 24 compiled, not run, 0 failed, 6 hardware passed` (`c2-test-c3-noboard.log`).
- Same for `--chip s3` with the four `XEWE_TEST_*` pin variables (12/13/14/15) → exit 0, same counts
  (`c2-test-s3-noboard.log`): the S3 test image the board session would have flashed compiles.
- Note: `-v` is a global flag (`xewe -v test …`); `xewe test … -v` is rejected by argparse.

### Code review (modules)

Contract shape ok (folder, `module.properties` 12 keys, README, `#include <XeWeCore.h>`); every
`status()` starts with `Module::status(false)`; only NVS key is `data`, namespaces ≤ 9 chars; no pins
in module code (all from `Config.h` via constructor structs, 255 = unset); command groups are the ids
(`$fan`, `$mlx90614`, `$argb`, `$fan_curve`); bad args are rejected with a message (parse_int range,
pin validity, 0-100 %, `#RRGGBB`, hex address 0x01-0x7F), argc enforced by the CLI.

Fix (tests only): `test_argb.py::test_add_rgb_remove` set the strip to `0 0 64` and read it back over
several commands, but `fan_curve` rewrites every strip once a second (no sensor → state on, hot colour
#FF0000), so the read-back would race the curve on a real board. The test now sets `255 0 0` (what the
curve writes in the fail-safe case), commented. Consequence: on a bare board the colour read-back no
longer proves `set_rgb` itself; `set_rgb` acceptance and `set_length`/`set_state` validation still are.
No `src/` changes.

### Board session: not run (board offline)

`/dev/ttyACM0` disappeared at ~22:39:40 PDT (`dmesg [14718.17] usb 3-3: USB disconnect`), before C2's
first board command (22:41, `port /dev/ttyACM0 not found`). Cause: the led-os session
(`test_logs/…/led/test-led-strip-isolation.log`) flashed the S3; esptool wrote 2 105 344 B at 0xe000
and the port vanished during "Verifying written data". Offline protocol followed: 3 × 30 s checks
(22:42:14, 22:42:44, 22:43:14) all absent (`c2-board-offline.log`); board work stopped, no recovery.

| Test | C3 no board | S3 board |
|---|---|---|
| `test_properties_match_source` ×4, `test_curve_math_defaults`, `test_cpp_matches_mirror` (host) | pass | — |
| `test_compiles` ×4 | pass | pass (compile only) |
| fan: `test_status`, `test_set_rejects_out_of_range`, `test_argc_error`, `test_add_set_remove` | compiled, not run | not run: board offline |
| fan: `test_rpm_reading` | skip (hardware) | not run: board offline (would skip) |
| mlx90614: `test_status`, `test_set_addr_validation`, `test_set_pins_rejects_same_pin`, `test_scan_bare_bus` | compiled, not run | not run: board offline |
| mlx90614: `test_read_temperature` | skip (hardware) | not run: board offline (would skip) |
| argb: `test_status`, `test_rejects_bad_values`, `test_add_rgb_remove` | compiled, not run | not run: board offline |
| argb: `test_colour_on_strip` | skip (hardware) | not run: board offline (would skip) |
| fan_curve: `test_status`, `test_add_remove_point`, `test_colours`, `test_offline_sensor_fail_safe`, `test_settings_survive_restart` | compiled, not run | not run: board offline |
| fan_curve: `test_closed_loop` | skip (hardware) | not run: board offline (would skip) |
| wifi / web-interface module tests | compiled, not run | not run: board offline |

Also not done (needed the board): pre-check `$system status`, provisioning carry-over to a new
project name (core NVS `root/init_setup_flag`), the four generic enable prompts via `xewe provision`,
the end-state status captures.

### Still unverified

Everything at runtime: the v2 firmware has never booted. Real fans (LEDC 25 kHz, tach ISR, RPM
filter), the MLX90614 (Wire restart in `set_pins`, bare-bus scan duration), WS2812B strips (FastLED
runtime pin selection, RMT next to WiFi), the closed loop, `/pad` and the JSON routes, first-boot
defaults per chip, provisioning carry-over, and whether `#` in `$fan_curve set_cold #RRGGBB` reaches
the handler intact through the core CLI.

### Board state at the end

Not attached (`/dev/ttyACM0` absent since ~22:39:40 PDT; needs a physical/VM re-attach). Firmware
unknown: the led-os test image was written but not verified; NVS (0x9000-0xdfff) was outside the
written range, so provisioning is probably intact. The cooling pad v2 firmware was never flashed.

## Host tests (C3)

Agent C3, 22:58–23:10 PDT. Logs: `test_logs/2026-10-09-night-run/cooling-pad/c3-*`. No git writes;
every `xewe` command ran with `XEWE_NO_BOARD=1` (board never probed or opened); toolchain data in
`.toolchain/arduino15`; `setup.sh` re-run with the three `XEWE_*_SOURCE` working trees so the venv has
the current tools (no-board switch, library catalogue) (`c3-setup.log`).

### Fan curve as pure logic

- New `src/FanCurve/CurveMath.h` (namespace `curve_math`, includes only `<cctype> <cmath> <cstddef>
  <cstdint> <string> <vector>`): templates over any point type with `temp`/`speed` (the FlexData
  `CurvePoint` on the device, a plain struct on the host). Functions: `target_speed` (interpolation,
  below-first/above-last clamps, speed clamped to 0-100), `speed_to_pwm` / `target_pwm` (duty per fan),
  `validate_points` (≤ 16 points, −40…200 °C, speed ≤ 100, strictly increasing, no two points within
  0.01 °C) + `error_text`, `schema_ok` / `schema_message`, `leds_on` (hysteresis), `parse_hex_color`,
  `led_colour` (cold/hot selection and blend). `SCHEMA` and `MAX_POINTS` live there;
  `FanCurveStore::SCHEMA` and `FanCurve::MAX_POINTS` alias them.
- `FanCurve.cpp` keeps no maths of its own: the loop, `status`, `add_point`, `set_curve`,
  `update_leds` and `begin_routines_required` call `curve_math::`.
- Behaviour changes (all small, all host-tested):
  - LED on/off now has **0.5 °C hysteresis** below the first point (old: off at `temp < first`, so
    sensor noise at the first point blinked the strips once a second). New member `leds_lit`.
  - An unreadable **NaN** temperature counts as hot (fans at the last point, strips hot). Old code
    returned 0 % for NaN; the MLX module reports 255.0 °C on errors, so this was latent only.
  - `set_curve()` (web POST `/ui/config`) now **rejects two points at the same temperature**
    (old: kept both). Speeds above 100 % are still clamped, the curve is still sorted first.
  - Boot: a stored curve that fails validation (after sorting) → message
    `Fan Curve: stored curve rejected (<reason>); using and storing the default curve`, defaults saved.
- **Schema check (CP6)**: a stored blob whose `schema` is not 1 → message
  `Fan Curve: stored settings have schema N, this firmware reads schema 1; using the default curve
  (the stored blob is left untouched until the next change)`; defaults run from RAM and are **not**
  written at boot (a downgrade does not destroy a newer blob; the first curve change overwrites it).
  Old behaviour silently overwrote it with defaults. Host-tested through `schema_ok`/`schema_message`;
  the wiring in `begin_routines_required` is checked by source (`test_curve_math_is_pure`) and the
  build only — no serial test (it would need a way to write a foreign blob into NVS); unverified on a
  board. Limitation: FlexData leaves a missing field at its default, so a blob without a `schema` field
  reads as schema 1.

### Tests

- `tests/host/test_curve_math.cpp` (LED-module pattern: `CHECK` macro, one line per check,
  `PASSED|FAILED: N check(s), F failure(s)`, exit code): **64 checks**, g++ 16 `-std=c++17 -Wall
  -Wextra -Werror`, 0 failures (`c3-host-test-curve-math.log`). Covers: interpolation incl. truncation
  and on-point values; below-first and above-last clamps (first/last speed, not 0/100); empty curve;
  255 °C and NaN fail-safe; monotonic sweep over 15-50 °C; > 100 % stored speed; PWM mapping (42 % →
  107 as in `status`); validation (sorted ok, unsorted, exact and within-0.01 duplicates, speed > 100,
  temp out of range/NaN, 16 vs 17 points); schema 1 accepted, 0/2/255 rejected, message text;
  hysteresis (band keeps the state, noise of ±0.3 °C never flaps, a slow sweep down/up flips exactly
  once each way); `#RRGGBB` parsing; cold/hot/blend colour selection, single-point curve.
- `tests/test_fan_curve.py`: the Python mirror (`test_curve_math_defaults`, `test_cpp_matches_mirror`)
  is replaced by `test_curve_math_host` (`@pytest.mark.host`; compiles and runs the file in
  `tmp_path`, asserts PASSED and ≥ 15 checks; skips without g++) and `test_curve_math_is_pure`
  (header includes only std headers; `FanCurve.cpp` calls the header, no local `namespace
  curve_math`). Host tests: still 8 in total (2 removed, 2 added).
- `xewe test --host-only -v` → exit 0, **8 host passed** (`c3-test-host-only.log`; per-test:
  `c3-pytest-fan-curve-host.log`, 3/3 fan_curve host tests PASSED, none skipped).
- `xewe test --chip c3` (no-board switch) → exit 0, `8 host passed, 24 compiled, not run, 0 failed,
  6 hardware passed` (`c3-test-c3-noboard.log`).

### Build (`xewe build --all-chips`, default pins; `c3-build-all-chips.log`, `c3-compile-{c3,c6,s3}.log`)

| Chip | Sketch | Flash | Globals | Warnings |
|---|---|---|---|---|
| C3 | 1 469 679 B | 70 % | 47 800 B (14 %) | 0 |
| C6 | 1 489 204 B | 71 % | 55 324 B (16 %) | 0 |
| S3 | 1 347 340 B | 64 % | 59 000 B (18 %) | 6 (all FastLED `I2SClockLessLedDriveresp32s3.h:397-398`, as before) |

0 project warnings. Sketch +1.3-1.5 KB against C1/C2 (validation, messages; the core/modules working
trees were re-copied, so not all of it is necessarily ours). Globals unchanged.

### Validator

Staging as in C1/C2 (scratchpad copy of `done/xewe-os-modules` + the 4 pad modules as
`modules/<slug>/`, `tests/host/` copied into `modules/fan-curve/tests/`, `--write-index` first):
`validate.py --harness <this project>` → **12 modules, 0 errors, 0 warnings** (`c3-validate.log`).
The host test also builds and passes from the staged module layout (`../../src/FanCurve/CurveMath.h`
resolves in both layouts).

### Docs

README: FastLED pin vs catalogue note, "Agents" (`XEWE_NO_BOARD=1`, `xewe test -v`, `--host-only`),
host-test paragraph, "First hardware session (Mac)" (flash C3 or S3 with the `--define` pins,
`xewe provision`, `xewe test --chip c3|s3 --require-board -v`, hardware env vars). `src/FanCurve/README.md`:
hysteresis, validation, schema message, `CurveMath.h`, tests.

### Still unverified (C3)

Everything at runtime, as in C2. New since C2: the hysteresis and schema-rejection paths on a board,
and the boot messages' wording on the serial console. The "First hardware session" commands are
written from `--help` output; none were run (no board, and `XEWE_NO_BOARD=1`).

## Review (R1)

Adversarial review for defects that only show on hardware (2026-10-08 night run, agent R1). Scope:
`src/{Fan,Mlx90614,Argb,FanCurve,PadWeb}/`. Logs: `test_logs/2026-10-09-night-run/review/r1-*`.
Builds ran in a scratchpad copy of this project (`./setup.sh --modules wifi,web-interface
--arduino-data …/.toolchain/arduino15`, the three `XEWE_*_SOURCE` working trees, `XEWE_NO_BOARD=1`).

### Fixed

| Sev | Where | Defect | Fix |
|---|---|---|---|
| Medium | `PadWeb/PadWeb.cpp` `handle_ui_config_post`, `FanCurve.cpp` setters | Every "Save" in the page posts both colours and the curve: 3 writes of the same `fan_curve/data` blob per click, colours written even when unchanged. | `set_cold_color/set_hot_color/set_curve(…, bool persist = true)`; unchanged colours are not written; PadWeb applies all three with `persist = false`, then one `save_settings()` (only while the module is enabled). |
| Medium | `Mlx90614.cpp` `read_i2c_temp` | The MLX error flag (bit 15 of the MSB) was not checked: a glitched bus returning `0xFFFF` read as 1037.5 C and **online**; a short read (fewer than 3 bytes) left stale bytes in the Wire buffer. | `requestFrom(...) != 3` → drain, NaN; `msb & 0x80` → NaN (offline → `error_temp` → fans at the last point, as before). |
| Medium | `Fan.cpp` `loop` | `pulses * 30000` in 32 bits wraps above 143 165 pulses (floating/ringing tach line, or a long loop stall), producing a small plausible RPM that passes the noise filter. | 64-bit product, saturated to `UINT32_MAX` (then dropped by `absolute_max_rpm`). |
| Medium | `Fan.cpp` `add` | No pin check against the other fans: a tach pin equal to another fan's PWM pin got an interrupt on an LEDC output; a tach pin shared with another fan replaced that fan's ISR argument, and removing either fan detached the other's tach interrupt. | `add` rejects a PWM pin that is another fan's tach, and a tach pin that is another fan's PWM or tach. |
| Low | `Argb.cpp` `loop` | Glide factor `k = glide_speed * dt` unclamped: `glide_speed` > 10 overshoots (> 20 diverges); a negative float cast to `uint8_t` is undefined. | `k` clamped to (0, 1]; NaN/negative jumps to the target. |
| Low | `FanCurve` `set_cold/set_hot` | `RRGGBB` without `#` rejected, unlike `$led fill` / `$led_modes color`. | `curve_math::normalize_hex_color` (with or without `#`, stored upper-case `#RRGGBB`); 9 new host checks in `tests/host/test_curve_math.cpp` (73 checks total). Failure message unchanged (`test_colours` matches it). |

### Documented, not changed

- **Medium: tach has no debounce or glitch filter** (`Fan.cpp:254-255`, `FALLING` on `INPUT_PULLUP`).
  The internal ~45 kΩ pull-up gives slow edges on an open-collector tach, and 25 kHz PWM crosstalk
  adds edges; counts above `absolute_max_rpm` are dropped silently, so the shown RPM freezes instead
  of reading wrong. Proposal (needs a scope on hardware): an external 10 kΩ pull-up, and/or ignore
  edges closer than ~1 ms in the ISR (`esp_timer_get_time()` is IRAM-safe); PCNT with its glitch filter
  exists on C6/S3 but not on C3.
- ISR itself is fine: `IRAM_ATTR`, `volatile` counter, read-and-clear under `noInterrupts()` on the
  core the interrupt was attached from (loopTask on all three chips); `detachInterrupt` precedes
  `delete`.
- **Low: FlexData schema mismatch overwrites at boot** in Fan/Mlx90614/Argb (`load()` falls back to the
  defaults and `save()`s), unlike FanCurve, which leaves another firmware's blob alone. Proposal: same
  rule as FanCurve.
- **Low: no cross-module pin check.** `$argb add 3`, `$mlx90614 set_pins` can take a fan's pin; no
  strapping-pin check (C3: 2/8/9, C6: 4/5/8/9/15, S3: 0/3/45/46). The C6 defaults put SDA/SCL on 4/5
  (MTMS/MTDI strapping): harmless with I2C pull-ups for a normal SPI boot, flagged for the first C6
  hardware session.
- **Low: RMT.** `Argb::max_strips()` = `SOC_RMT_TX_CANDIDATES_PER_GROUP` (2 on C3/C6). FastLED 3.10.3
  uses an RMT5 worker pool, so the cap is conservative; it does not count other RMT users (adding
  `led-strip` to this project on C3/C6 would need the cap lowered). `showLeds()` runs on the main loop
  only while a strip is gliding (12 LEDs ≈ 0.4 ms).
- **Info: LEDC.** Fans use `ledcAttach` (auto channel, 25 kHz, 8 bit); nothing else in this build
  uses LEDC (the `pins` module would share timer pairs). Disabling `fan` detaches LEDC: the PWM pin
  floats and most 4-wire fans then run at 100 % (safe direction).
- **Info: I2C blocking.** Wire's default 50 ms timeout bounds a dead bus; offline polling backs off to
  5 s, so worst case is one 50 ms stall per 5 s. `set_pins` restarts the bus with `Wire.end()` /
  `Wire.begin()`; no other module uses `Wire` here.
- NVS: no writes from `loop()` (FanCurve passes `persist = false` to Fan and Argb). Status lines
  start with `<Name> module enabled|disabled` (core `Module::status`), matching the tests' regex.

### Verification (scratchpad copy, after the fixes)

- `xewe build --all-chips --clean` → exit 0: C3 1 470 335 B, C6 1 489 860 B, S3 1 347 952 B;
  warnings 0 / 0 / 6, all six in FastLED's `I2SClockLessLedDriveresp32s3.h:397-398` (none in project
  code) (`r1-pad-build-all-chips.log`, `r1-pad-compile-{c3,c6,s3}.log`).
- `xewe -v test --host-only` → 8 host passed (`r1-pad-test-host-only.log`); curve-math host binary:
  `PASSED: 73 check(s)` (`r1-host-curve-math.log`).
- Validator (staging copy of `done/xewe-os-modules` + the 4 pad modules) `--harness` this project →
  13 modules, 0 errors, 0 warnings (`r1-validate-pad.log`).
- Not verified: everything at runtime (no board).

## CP1 restructure (CP10, 2026-10-09)

Owner decision CP1: GPL-3.0, `fan` (with the curve) and `mlx90614` extracted into
`done/xewe-os-modules` (branch `led-modules`, uncommitted), `argb` replaced by the repo `led` module,
the project keeps only `src/PadWeb/`. Decisions CP11-CP21 in `wip/xewe-os-laptop-cooling-pad/MIGRATION.md`.
Logs: `test_logs/2026-10-09-pad-extract/`. No git writes, no board (`XEWE_NO_BOARD=1`).

| Before (night run) | After |
|---|---|
| `src/Fan/` | repo `modules/fan/` (`Fan fan(os);`), curve merged in: `src/Fan/Curve.h` (was `src/FanCurve/CurveMath.h`), `$fan curve list/add/remove/set`, `$fan temp`, `fan.set_temperature(c, origin)`; NVS `fan/data` + `fan/curve` |
| `src/FanCurve/` (`$fan_curve`) | gone: curve → `fan`; colours → PadWeb (`$pad set_cold/set_hot`, NVS `pad/data`) |
| `src/Mlx90614/` | repo `modules/mlx90614/` (`Mlx90614 mlx90614(os);`), `Mlx90614Listener::on_temperature`, `Convert.h` (pure), bounded scan, NaN offline (no 255.0 error value) |
| `src/Argb/` (2 strips, FastLED) | repo `led` (one strip); PadWeb shows the curve colour with `led.fill()` and its own glide (`src/PadWeb/Glide.h`, R1 clamp kept) |
| `src/PadWeb/` (plain class) | project-local module `pad` (`PadWeb pad_web(os, fan, mlx90614, led);`, the only declare in the `.ino`): sensor → `fan.set_temperature` (listener registered in its constructor), strip colour, `/pad` page |
| `Config.h` `PAD_*` | module build defines `FAN1_PWM`…, `MLX90614_SDA/SCL/ADDR`, `LED_PIN_DATA`/`LED_COUNT`, per-chip defaults in the module headers |
| PolyForm-NC + No-AI (`LICENSE.md`, `LICENSE-NO-AI.md`) | GPL-3.0-only, `LICENSE.txt` = `done/xewe-os/LICENSE.txt`; SPDX `GPL-3.0-only` on every project source |
| `tests/` 4 board + 6 unit files | `tests/board/test_pad.py`, `tests/unit/test_pad.py` + `test_glide.cpp`; module tests moved with the modules (curve test: 73 → 87 checks with `curve set` parsing) |

R1 fixes carried over: tach 64-bit maths, shared-pin refusal (`fan`), MLX error flag and short read
(`mlx90614`, now counted as `Read errors`), one NVS write per save (POST `/ui/config`: at most one
write per changed blob, `pad/data` and `fan/curve`), schema mismatch never overwritten at boot (now in
`fan` fans + curve, `mlx90614` and `pad` alike), glide clamp (`Glide.h`, 16 host checks).

**Verification** (all exit 0):

| Step | Result |
|---|---|
| modules `tools/validate.py --write-index`, then `--harness` this project | 9 modules, 0 errors, 0 warnings |
| `./setup.sh </dev/null` from scratch (`build/` deleted, local sources) | wifi, web-interface, fan, mlx90614, led; ArduinoJson 7.4.3, FastLED 3.10.3 |
| `xewe build --all-chips` | C3 1 503 321 B (71 %, was 70), C6 1 512 106 B (72 %, was 71), S3 1 361 800 B (64 %, was 64); 0 / 0 / 6 warnings, the 6 all in FastLED `I2SClockLessLedDriveresp32s3.h:397-398` (0 project warnings) |
| `xewe test --unit-only` | 26 unit passed (project 6, fan 4 incl. the 87-check curve binary, mlx90614 4, led 10, wifi 1, web-interface 1) |
| `xewe test --no-board` | 26 unit passed, 6 `test_compiles` passed, 42 compiled, not run, 0 failed |
| harness `migration/phase_2/hw-s3`, `setup --modules all` + `build --chip s3` | 9 modules, 1 481 088 B (70 %), 0 warnings; `--unit-only` 24 passed |

Flash grew ~34 KB on C3/C6 (the `led` module's render task and 7 modes replace the 400-line `argb`).
Not verified: anything at runtime (no board), `led.fill()` at 50 Hz next to the render task, the
bounded scan's timing on a real bare bus, the `/pad` page against the new JSON.
