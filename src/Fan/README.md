# fan — 4-wire PWM fans with tachometer RPM

XeWe OS module (project-local, cooling pad) · ported 2026-10-09 from the pad's 0.0.12 firmware · Solo: Max Dokukin · Status: Active (0.1.0)

## Overview

Drives any number of 4-wire PC fans with 25 kHz, 8-bit PWM (LEDC) and, when a tachometer pin is
given, measures their RPM. Part of the XeWe laptop cooling pad; `fan_curve` sets the speed from the
temperature. Licensed with the pad (PolyForm Noncommercial 1.0.0 + No-AI addendum), not GPL: it
lives in the pad project until the owner decides on a move into `xewe-os-modules` (MIGRATION.md CP1).

## Highlights

- PWM on any output-capable GPIO (`ledcAttach(pin, 25000, 8)`), speed 0-255
- Tachometer on a FALLING-edge interrupt, 2 pulses per revolution, >= 1 s windows, 3-sample median,
  EMA (alpha 0.3), rounded to 10 RPM, readings above 10 000 RPM dropped as noise
- Fans persist in NVS as FlexData (`fan/data`, schema 1); speed changes from control loops are not
  written to flash (`set_all(speed, false)`)
- First boot (no stored settings): the fans from the constructor config (Config.h `PAD_FAN*`)

## How it works

```
$fan set 3 128 → parse → ledcWrite(3, 128) → save list to NVS → "Speed updated."
tach ISR → pulse_count++ ; loop(): every >= 1 s per fan → rpm → median → EMA → displayed_rpm
```

- **`Fan` class** (`src/Fan/`): a `xewe::Module` with id `fan`; no first-boot questions (only the
  generic "enable?" prompt); can be disabled (disabling wipes `fan/` and stops `fan_curve`).
- Constructor: `Fan(xewe::Os& host, FanConfig config = {})`; `FanConfig{defaults, ema_alpha,
  absolute_max_rpm, ui_rounding}`, `defaults` = list of `{pwm, tach}` (255 = none).
- API: `add(pwm, tach = 255)`, `remove`, `set(pwm, speed, persist = true)`,
  `set_all(speed, persist = true)`, `get_rpm`, `get_json`.

### Commands

**Prefix:** `$fan` · can be disabled

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`add`** | Add a fan without a tachometer. | `$fan add <pwm_pin>` |
| **`add_w_tach`** | Add a fan with a tachometer. | `$fan add_w_tach <pwm_pin> <tach_pin>` |
| **`set`** | Set one fan's speed (0-255); persisted. | `$fan set <pwm_pin> <speed>` |
| **`set_all`** | Set every fan's speed (0-255); persisted. | `$fan set_all <speed>` |
| **`remove`** | Remove a fan (PWM off, pin detached). | `$fan remove <pwm_pin>` |
| **`print_json`** | `{"fans":[{"pin_pwm","has_tach","speed" (0-100 %),"pin_tach","displayed_rpm","ema_rpm"}]}` | `$fan print_json` |
| `status` / `reset` / `enable` / `disable` | generic module commands | `$fan status` |

While `fan_curve` is enabled it rewrites every fan's speed once a second; `set`/`set_all` are then
only momentary.

### Requirements

| | |
|---|---|
| Modules | none |
| Libraries | XeWeCore >=2.0.0,<3.0.0; ArduinoJson (core dependency) |
| Boards | ESP32-C3, ESP32-C6, ESP32-S3 (arduino-esp32 3.x) |

### Tests

`tests/test_fan.py` in the project, run with `build/.venv/bin/python -m xewe test`.
`test_compiles` runs without a board; `test_status`, `test_add_set_remove` (`XEWE_TEST_FAN_PIN`),
`test_set_rejects_out_of_range`, `test_argc_error` need a provisioned board; `test_rpm_reading`
requires a real fan (`XEWE_TEST_FAN_PWM_PIN`, `XEWE_TEST_FAN_TACH_PIN`).
