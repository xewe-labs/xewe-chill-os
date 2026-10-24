# src/Mlx90614: the MLX90614 contactless I2C temperature sensor

## Overview

Reads an MLX90614 infrared thermometer over I2C (`Wire`): object and ambient temperature, every
500 ms while it answers and every 5 s while it does not. Other code subscribes with a listener
(`Mlx90614Listener::on_temperature`), e.g. a project that feeds the `fan` module's curve. A module
for [XeWe OS](https://github.com/xewe-labs/xewe-os), built on
[XeWeCore](https://github.com/xewe-labs/xewe-os-core).

## Highlights

- Error replies are errors, not temperatures: a short read is drained, and a word with the sensor's
  error flag (bit 15; `0xFFFF` from a glitched bus would read as 1037.5 °C) counts as a failed read;
  the sensor is then reported offline (NaN, JSON `null`, `Read errors: N` in status)
- Address validated (`0x01`–`0x7F`, hex with or without `0x`, the whole token must parse)
- `scan` is bounded: 10 ms per probe, 1.5 s in total, and it stops after 3 bus errors/timeouts in a row
  (an unbounded scan of a bare bus without pull-ups blocks the console for tens of seconds)
- Value mapping in `src/Mlx90614/Convert.h`, pure C++ (no Arduino), host-tested with g++
- Settings table: `addr` u8 1-127, `sda`/`scl` u8 (255 = none), each its own NVS key;
  `$mlx90614 set|get|schema`
- First boot and `reset`: pins and address from the `XEWE_MLX90614_*` values (the table defaults)
- SDA/SCL are claimed in the core pin registry (`xewe::pins`) while the bus runs; a pin another
  module holds is refused

## How it works

```
loop(): every 500 ms (5 s offline) → read RAM 0x07 (object), 0x06 (ambient) → raw_to_celsius → listeners
$mlx90614 set_pins 4 5 → validate → apply_setting sda, scl (NVS) → release/claim → Wire.end/begin → poll → "MLX90614: pins set to SDA=4 SCL=5, sensor online"
```

- **`Mlx90614` class** (`src/Mlx90614/`): a `xewe::Module` with id `mlx90614`, name `MLX90614`
  (`declare=Mlx90614 mlx90614(os);`); no first-boot questions (only the generic "enable?" prompt);
  can be disabled (`get_temp()` is then NaN).
- `Wire` is owned by this module: another module that also calls `Wire.begin` on other pins would
  move the bus.

### In Config.h

The values live in [`Config.h`](Config.h) (prefix `XEWE_MLX90614_`); a `--define` or a line in the
project's `Config.h` wins. The address and pins are the defaults of the settings rows, used on the
first boot (or after `$mlx90614 reset`). 255 = not configured.

| Define | Default | Meaning |
|---|---|---|
| `XEWE_MLX90614_ADDR` | 0x5A | sensor address |
| `XEWE_MLX90614_SDA` / `XEWE_MLX90614_SCL` | 4 / 5 | I2C pins (S3 wiring: 8 / 9) |
| `XEWE_MLX90614_LISTENERS_MAX` | 4 | listener slots |
| `XEWE_MLX90614_POLL_MS` / `XEWE_MLX90614_OFFLINE_POLL_MS` | 500 / 5000 | poll period online / offline, ms |
| `XEWE_MLX90614_SCAN_BUDGET_MS` / `XEWE_MLX90614_SCAN_PROBE_MS` | 1500 / 10 | `scan` time budget and per-probe timeout, ms |

On the C6, GPIO 4/5 are MTMS/MTDI: harmless with I2C pull-ups unless a JTAG-select eFuse is burnt.

### Commands

**Prefix:** `$mlx90614` · can be disabled

| Command | Description | Sample Usage |
| :--- | :--- | :--- |
| **`read`** | Read the sensor now. | `$mlx90614 read` |
| **`scan`** | Scan the I2C bus (bounded, see above). | `$mlx90614 scan` |
| **`set_addr`** | Set the sensor address (hex, 0x01-0x7F); persisted. | `$mlx90614 set_addr 0x5A` |
| **`set_pins`** | Set SDA/SCL (two different output-capable GPIOs), restart the bus; persisted. | `$mlx90614 set_pins 4 5` |
| **`set`** / **`get`** / **`schema`** | Table rows `addr` (decimal), `sda`, `scl`; `set sda`/`set scl` restart the bus. | `$mlx90614 set addr 91` |
| **`print_json`** | `{"module","online","object_temp","ambient_temp","i2c_address","sda_pin","scl_pin","read_errors"}` (temperatures `null` while offline) | `$mlx90614 print_json` |
| `status` / `reset` / `enable` / `disable` | generic module commands | `$mlx90614 status` |

### C++ API

```cpp
mlx90614.get_temp();  mlx90614.get_ambient_temp();   // °C, NaN while offline or disabled
mlx90614.is_online();  mlx90614.get_error_count();  mlx90614.set_i2c_address(0x5A);  mlx90614.set_pins(4, 5);

struct MyListener : Mlx90614Listener {
    void on_temperature(float object_c, float ambient_c, bool online) override { /* main loop; keep it short */ }
} my_listener;
mlx90614.listeners.add(&my_listener);  // xewe::ListenerSet; false when all XEWE_MLX90614_LISTENERS_MAX (4) slots are taken
                                       // (add_listener/remove_listener do the same)
```

Listeners are called after every poll (also offline: `online = false`, both temperatures NaN), from
the main loop.

### Requirements

| | |
|---|---|
| Modules | none |
| Libraries | XeWeCore 3.x; ArduinoJson (`xewe.toml` `[libraries]`); Wire (esp32 core) |
| Boards | ESP32-C3, ESP32-C6, ESP32-S3 (arduino-esp32 3.x) |

A project-local module: `xewe-chill-os.ino` includes `src/Mlx90614/Mlx90614.h` and declares `Mlx90614 mlx90614(os);`.

### Settings and NVS keys

| Key (namespace `mlx90614`) | Type | Default | Meaning |
|---|---|---|---|
| `addr` | u8 1–127 | `XEWE_MLX90614_ADDR` (0x5A) | sensor I2C address (decimal in `set`; `set_addr` takes hex) |
| `sda` | u8 | `XEWE_MLX90614_SDA` | SDA GPIO, 255 = not configured; `set` restarts the bus |
| `scl` | u8 | `XEWE_MLX90614_SCL` | SCL GPIO, 255 = not configured; `set` restarts the bus |

### Tests

Unit: `tests/unit/test_mlx90614.py` (builds `tests/unit/test_convert.cpp` with g++). Board:
`tests/board/test_mlx90614.py`. Run with `xewe test` ([doc/TESTING.md](../../doc/TESTING.md)).

## Known issues

- The PEC byte is read but not checked (the error flag and the 3-byte rule catch the bus glitches
  seen so far).
- `scan` blocks the console for up to 1.5 s.
