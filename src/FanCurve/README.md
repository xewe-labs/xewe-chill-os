# fan-curve — the cooling pad's control loop

XeWe OS module (project code, cooling pad; the old `TempController`) · ported 2026-10-09 · Solo: Max Dokukin · Status: Active (0.1.0)

## Overview

Once a second: sensor temperature → piecewise-linear curve (°C → 0-100 %) → every fan
(`pct x 255 / 100`, not persisted). With `argb` enabled the strips follow the temperature: off below
the first point (with 0.5 °C hysteresis: off only below first − 0.5 °C, on again at the first point),
then a linear blend from the cold colour (first point) to the hot colour (last point
and above). A missing sensor reads 255.0 C, so the fans run at the last point's speed (fail-safe).
It is product logic, kept in the pad project (MIGRATION.md CP2). Licensed with the pad.

## Highlights

- Curve and colours persist as one FlexData blob (`fan_curve/data`, schema 1: points, cold, hot);
  first boot stores the default 22 °C → 0 %, 27 °C → 100 %, 32 °C → 100 %, cold `#00FFFF`, hot `#FF0000`
- At most 16 points; adding a point at an existing temperature (±0.01) replaces it
- `set_curve()` replaces the whole curve with one NVS write (used by the web page's POST `/ui/config`);
  it rejects more than 16 points, temperatures outside −40…200 °C and two points at the same temperature
- A stored blob with another `schema` is rejected at boot with
  `Fan Curve: stored settings have schema N, this firmware reads schema 1; using the default curve ...`;
  the defaults run from RAM and the blob is left as is until the next change. A stored curve that
  fails validation is replaced by the defaults (with a message)
- The decisions are pure functions in `CurveMath.h` (no Arduino): interpolation, clamps, validation,
  schema check, LED hysteresis, colour blend; an unreadable (NaN) temperature counts as hot
- Requires `fan` and `mlx90614`; `argb` is used only while enabled (disabling the LEDs never stops the fans)

### Commands

**Prefix:** `$fan_curve` (was `$tempcontroller`) · can be disabled

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`add_point`** | Add or replace a point. | `$fan_curve add_point <temp_C> <0-100>` |
| **`remove_point`** | Remove the point at a temperature. | `$fan_curve remove_point <temp_C>` |
| **`set_cold`** | LED colour at the first point. | `$fan_curve set_cold #00FFFF` |
| **`set_hot`** | LED colour at the last point. | `$fan_curve set_hot #FF0000` |
| **`print_json`** | `{"curve_edge_colors":{"start","end"},"temp_curve":[{"temp","speed"}]}` | `$fan_curve print_json` |
| `status` / `reset` / `enable` / `disable` | generic module commands | `$fan_curve status` |

### Requirements

| | |
|---|---|
| Modules | fan, mlx90614 (requirements), argb (optional at run time; listed in `depends_modules` because the declare line passes it) |
| Libraries | XeWeCore >=2.0.0,<3.0.0 |
| Boards | ESP32-C3, ESP32-C6, ESP32-S3 |

### Tests

`tests/board/test_fan_curve.py`: `test_compiles`; unit tests (`--unit-only`): `test_curve_math_unit_gpp`
(`tests/unit/test_curve_math.py`) builds and runs `tests/unit/test_curve_math.cpp` (g++ `-Werror`,
64 checks of `CurveMath.h`), `test_curve_math_is_pure`, and `test_properties_match_source`
(`tests/unit/test_fan_curve.py`); on a bare board
`test_status`, `test_add_remove_point`, `test_colours`, `test_offline_sensor_fail_safe`,
`test_settings_survive_restart` (`$system restart`); `test_closed_loop` requires the assembled pad
(`XEWE_TEST_PAD_HARDWARE=1`).
