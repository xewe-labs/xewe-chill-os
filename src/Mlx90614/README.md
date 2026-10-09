# mlx90614 — contactless I2C temperature sensor

XeWe OS module (project-local, cooling pad) · ported 2026-10-09 from the pad's 0.0.12 firmware · Solo: Max Dokukin · Status: Active (0.1.0)

## Overview

Reads object and ambient temperature from an MLX90614 IR sensor over I2C (`Wire`, raw SMBus reads of
RAM 0x07 / 0x06, value x 0.02 - 273.15). Polls every 500 ms; while no sensor answers it reports the
error temperature 255.0 C, which drives the pad's fans to the top of the curve (fail-safe), and backs
off to one poll every 5 s. Licensed with the pad (PolyForm Noncommercial 1.0.0 + No-AI addendum).

## Highlights

- Pins and address persist in NVS as FlexData (`mlx90614/data`, schema 1); first boot uses the
  constructor config (Config.h `PAD_MLX_SDA`, `PAD_MLX_SCL`, `PAD_MLX_ADDR`)
- Unconfigured pins (255) are reported, never a crash; no first-boot pin prompt
- `set_pins` restarts the bus at once; `set_addr` validates 0x01-0x7F

## How it works

```
loop(): every 500 ms (5 s while offline) → read 0x07, 0x06 → online flag + cached values
get_temp() → cached object temperature, or 255.0 when disabled/offline
```

- **`Mlx90614` class** (`src/Mlx90614/`): a `xewe::Module` with id `mlx90614`, name `MLX90614`;
  can be disabled (that also disables `fan_curve`).
- Constructor: `Mlx90614(xewe::Os& host, Mlx90614Config config = {})`;
  `Mlx90614Config{sda_pin, scl_pin, i2c_address, poll_interval_ms, offline_poll_interval_ms, error_temp}`.

### Commands

**Prefix:** `$mlx90614` · can be disabled (the old firmware's help said `$temp`; the group is now the id)

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`read`** | Read the sensor now. | `$mlx90614 read` |
| **`scan`** | Scan the I2C bus (0x01-0x7E). | `$mlx90614 scan` |
| **`set_addr`** | Set the sensor address (hex, 0x01-0x7F). | `$mlx90614 set_addr 0x5A` |
| **`set_pins`** | Set SDA/SCL and restart the bus. | `$mlx90614 set_pins <sda> <scl>` |
| **`print_json`** | `{"module","online","object_temp","ambient_temp","i2c_address","sda_pin","scl_pin"}` | `$mlx90614 print_json` |
| `status` / `reset` / `enable` / `disable` | generic module commands | `$mlx90614 status` |

### Requirements

| | |
|---|---|
| Modules | none |
| Libraries | XeWeCore >=2.0.0,<3.0.0 (Wire comes with the esp32 core) |
| Boards | ESP32-C3, ESP32-C6, ESP32-S3 (arduino-esp32 3.x) |

### Tests

`tests/board/test_mlx90614.py`: `test_compiles` (no board); `test_status`, `test_set_addr_validation`,
`test_set_pins_rejects_same_pin` and `test_scan_bare_bus` (`XEWE_TEST_MLX_SDA`/`XEWE_TEST_MLX_SCL`,
restores the pins) on a bare board; `test_read_temperature` requires a sensor (`XEWE_TEST_MLX_PRESENT=1`).
Without pull-ups an I2C scan can take tens of seconds (each probe may hit the bus timeout).
