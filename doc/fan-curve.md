# The fan curve

The curve turns a temperature into a fan speed. The LED strip uses the same curve for its colour.

## How the speed is chosen

| Temperature | Fan speed |
|---|---|
| at or below the first point | the first point's speed |
| between two points | a straight line between them, rounded down to a whole % |
| at or above the last point | the last point's speed |
| no sensor reading | the last point's speed (fail-safe) |

- Both fans get the same speed. The curve sets them once a second.
- Speed in % becomes PWM duty 0–255: 50 % = 127.
- A curve holds up to 16 points. The page keeps at least 2. Temperatures from -40 to 200 °C
  (0 to 100 °C on the page), no two points at the same temperature.
- With an empty curve (`$fan curve set none`) the fans keep the speed you set with `$fan set`.

## Example

```text
$fan curve set 30:20,40:60,50:100
```

| Sensor reads | Fans |
|---|---|
| 25 °C | 20 % (below the first point) |
| 30 °C | 20 % |
| 35 °C | 40 % |
| 45 °C | 80 % |
| 55 °C | 100 % |
| sensor unplugged | 100 % |

Check it without heating anything:

```text
$fan temp 35
```

The device answers `Temperature set to 35.00 C, curve target 40 %.` The next sensor reading
replaces the value: within 0.5 s, or within 5 s while the sensor is offline.

## Edit the curve

| Task | Command |
|---|---|
| Show points, temperature and target | `$fan curve list` |
| Replace the whole curve | `$fan curve set 30:20,40:60,50:100` |
| Add or change one point | `$fan curve add 45 80` |
| Remove one point | `$fan curve remove 45` |
| Remove all points | `$fan curve set none` |

`curve set` takes `temperature:speed` pairs, comma-separated, in any order. Speed is a whole number
0–100. Decimal temperatures work: `27.5:60`.

On the web page: change the table, then press **Save Curve**. Nothing is stored before you save.

## The strip colour

| Temperature | Strip |
|---|---|
| more than 0.5 °C below the first point | off |
| at the first point | cold colour |
| between first and last point | blend of cold and hot, by position between them |
| at or above the last point | hot colour |
| no sensor reading | hot colour |

The blend runs from the first to the last point, not to the point where the fans reach 100 %.
With the default curve (22, 27, 32 °C) the strip is half way at 27 °C: `#7F7F7F` from cyan and red.

The strip turns on at the first point and turns off only 0.5 °C below it. The colour glides to
each new value in about half a second.

```text
$pad set_cold 00FFFF
$pad set_hot FF0000
```

## Timing

| Setting | Default | Change with |
|---|---|---|
| Sensor read | every 500 ms (5 s while offline) | fixed |
| Curve update | every 1000 ms | `$fan set curve_ms 500` (100–60000) |
| Stale reading | 10000 ms | `$fan set stale_ms 20000` (1000–600000) |

A reading older than `stale_ms` counts as no reading: the fans go to the last point's speed. This
covers a sensor module that stops reporting. An unplugged sensor is reported at once: the
fans reach the fail-safe speed within about 1.5 s.

## Manual control

`$fan set <pwm_pin> <0-255>` and `$fan set_all <0-255>` work, but while the sensor reports, the
curve overwrites them within a second. For full manual control, clear the curve:

```text
$fan curve set none
$fan set_all 128
```

Set a curve again to return to automatic control.
