# argb — WS2812B LED strips on FastLED

XeWe OS module (project-local, cooling pad) · ported 2026-10-09 from the pad's 0.0.12 firmware · Solo: Max Dokukin · Status: Active (0.1.0)

## Overview

Drives WS2812B (GRB, 800 kHz) strips through FastLED 3.10.3, one RMT channel per strip. Each strip
has an on/off state and a target colour and glides to it (frame-rate independent exponential
approach, x8 per second, dt capped at 0.1 s). The old firmware used Adafruit NeoPixel; FastLED is the
ecosystem's one LED library (MIGRATION.md CP3). Licensed with the pad (PolyForm Noncommercial 1.0.0
+ No-AI addendum).

## Highlights

- Strip pin chosen at run time: one FastLED controller is instantiated per GPIO that FastLED accepts
  on the chip (`if constexpr` over `FastPin<N>::validpin()`), picked by the stored pin
- Per-strip length (1-300, default 12 from Config.h `PAD_ARGB_LEN`), new in v2
- At most `SOC_RMT_TX_CANDIDATES_PER_GROUP` strips: 2 on C3/C6, 4 on S3
- Strips persist in NVS as FlexData (`argb/data`, schema 1); `set_all_*` from the control loop pass
  `persist = false`

## How it works

```
$argb set_rgb 6 255 0 0 → target colour → transitioning → loop(): glide → fill_solid → driver->showLeds(255)
```

Each strip is shown through its own `CLEDController::showLeds`, never `FastLED.show()`, so other
FastLED users in the same firmware are not redrawn. A removed strip is blanked and its controller
detached from its buffer (FastLED cannot unregister a controller; re-adding the pin reuses it).

### Commands

**Prefix:** `$argb` · can be disabled

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`add`** | Add a strip with the default length. | `$argb add <pin>` |
| **`remove`** | Blank and remove a strip. | `$argb remove <pin>` |
| **`set_length`** | Set the LED count (1-300). | `$argb set_length <pin> <n>` |
| **`set_state`** | Turn on (1) or off (0). | `$argb set_state <pin> <1\|0>` |
| **`set_rgb`** | Set the target colour. | `$argb set_rgb <pin> <r> <g> <b>` |
| **`print_json`** | `[{"pin","length","state","r","g","b"}]` | `$argb print_json` |
| `status` / `reset` / `enable` / `disable` | generic module commands | `$argb status` |

While `fan_curve` is enabled and the sensor reads at or above the first curve point, it sets every
strip on and to the curve colour once a second (not persisted); below the first point it turns them off.

### Requirements

| | |
|---|---|
| Modules | none |
| Libraries | XeWeCore >=2.0.0,<3.0.0; **FastLED 3.10.3** (`xewe.toml [libraries]`) |
| Boards | ESP32-C3, ESP32-C6, ESP32-S3 (arduino-esp32 3.x) |

### Tests

`tests/board/test_argb.py`: `test_compiles` (no board); `test_status`, `test_rejects_bad_values`,
`test_add_rgb_remove` (`XEWE_TEST_ARGB_PIN`; on C3/C6 only with a free RMT channel) on a bare
board; `test_colour_on_strip` requires a strip and a person (`XEWE_TEST_ARGB_VISUAL=1`).
