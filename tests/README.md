# tests/

`unit/` holds pytest files that check source facts on the host, with no board and no build. `test_fan.py` and `test_mlx90614.py` also build `test_curve.cpp` and `test_convert.cpp` with g++ (`-std=c++17 -Wall -Wextra -Werror`) against the pure headers `src/Fan/Curve.h` and `src/Mlx90614/Convert.h`. `board/` holds pytest files that run against a flashed board over serial. `MANUAL.md` is the first test on the assembled pad, by hand.

```sh
build/tools/.venv/bin/python -m xewe test --unit-only
build/tools/.venv/bin/python -m xewe test --chip c3 -- -m board
```

Without a board, the board tests are reported as `compiled, not run` and the command exits 0. [doc/TESTING.md](../doc/TESTING.md) has the flags and the dotenv keys.

## Board test preconditions

Every board test needs a provisioned board: the first boot done, then `xewe provision`, which answers the "Would you like to enable ...?" prompts of fan and mlx90614. Pad asks nothing. The pad tests also need the led setup done.

| File | Needs | Keys |
|---|---|---|
| `board/test_fan.py` | Nothing wired for most tests. The add, set and remove test needs a free output GPIO that is not a default fan pin, and the shared-pin test a second one; PWM into nothing is harmless. The RPM tests need a real 4-wire fan. | `XEWE_TEST_FAN_PIN`, `XEWE_TEST_FAN_PIN2`, `XEWE_TEST_FAN_PWM_PIN` and `XEWE_TEST_FAN_TACH_PIN` |
| `board/test_mlx90614.py` | Nothing wired for the address test, which restores the address it found. The bare-bus test moves the I2C bus to two free GPIOs with nothing attached and moves it back. The temperature test needs a real MLX90614 on the configured pins. | `XEWE_TEST_MLX_SDA`, `XEWE_TEST_MLX_SCL`, `XEWE_TEST_MLX_PRESENT=1` |
| `board/test_pad.py` | Nothing wired. Without a sensor, mlx90614 reports offline every 5 s, so the fans run at the last point's speed and the strip shows the hot colour, which `test_offline_sensor_fail_safe` checks. The closed loop needs the real pad. | `XEWE_TEST_PAD_HARDWARE=1` |

PadWeb feeds the curve from the sensor, NaN while it is offline, so the curve rewrites every fan speed once per `curve_ms` and a speed set by hand is not read back. While `$pad lights` is on, PadWeb changes the strip at most once per `XEWE_PAD_COLOR_MIN_MS` (2 s), and `SETTLE_S` in `test_pad.py` waits that out. The restart test runs last in `test_pad.py`, so a board that does not come back fails only that test.
