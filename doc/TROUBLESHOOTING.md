# Troubleshooting

Start with the status of each part:

```text
$mlx90614 status
$fan status
$pad status
$led status
```

## The sensor reads offline

Signs: `$mlx90614 status` shows `Online: no`; `$fan status` shows `Temperature: offline (source)`;
the fans run at the last point's speed; the strip is red; the page shows 0.0 °C.

| Check | How |
|---|---|
| Pins | `$mlx90614 status` → `Pins: SDA 4, SCL 5` must match your wiring. Fix: `$mlx90614 set_pins <sda> <scl>`. |
| Pins not set | `Pins: not configured` → `$mlx90614 set_pins <sda> <scl>`. |
| Address | `$mlx90614 scan` lists the addresses that answer. Use the one it finds: `$mlx90614 set_addr 0x5A`. |
| Scan finds nothing | Check power, GND, and that SDA and SCL are not swapped. The bus needs pull-up resistors. |
| Scan stops early | "bus not responding" or "time budget spent": the lines are floating or shorted. Check the pull-ups. |
| Read errors grow | `Read errors: N` rises: loose wire or a long cable. Shorten it, or re-seat the connector. |
| Module disabled | `MLX90614 module (disabled)` → `$mlx90614 enable`. |

While the sensor is offline the device retries every 5 s. It comes back on its own.

## The fans are silent

| Check | How |
|---|---|
| Curve target | `$fan status` → `Curve target: 0 %`: the temperature is below the curve's first point. This is normal on a cold laptop. Test with `$fan temp 40`. |
| Curve idle | `Temperature: none yet (curve idle ...)`: no reading has arrived. Check that the MLX90614 module is enabled. Until then the fans keep their manual speed, 0 after the first boot. |
| No fans | `No fans configured` → `$fan add_w_tach <pwm> <tach>`. |
| Wrong pin | The PWM pin in `$fan status` must match your wiring. |
| Power | The fans need their own 12 V or 5 V supply and a ground shared with the ESP32. |
| Module disabled | `Fan module (disabled)` → `$fan enable`. |
| Fan stops at low duty | Many fans stop below about 20 %. Raise the lowest curve point. |

To test a fan by hand:

```text
$fan curve set none
$fan set_all 255
```

Set your curve again afterwards.

## RPM shows 0

| Check | How |
|---|---|
| No tach | `$fan status` shows `no tach` → re-add the fan: `$fan remove <pwm>`, `$fan add_w_tach <pwm> <tach>`. |
| Fan not turning | The RPM is 0 when the duty is 0. Check with `$fan set_all 255`. |
| Wait | The first value takes a few seconds; it settles over about 5 s. |
| Wiring | The tach wire (often green) goes to the tach GPIO. |
| Frozen value | The RPM stays at an old value: the tach line is noisy. Add a 10 kΩ pull-up from the tach GPIO to 3.3 V and keep the tach wire away from the PWM wire. |

## The strip stays dark or shows the wrong colour

| Check | How |
|---|---|
| Below the first point | The strip is off more than 0.5 °C below the first curve point. Test with `$fan temp 40`. |
| Lights off | `$pad status` → `Lights: off` → `$pad lights on`. |
| Brightness | `$led status` shows brightness 0 → `$led set_brightness 128`. |
| Data pin | The pin is set at build time: `XEWE_LED_PIN_DATA` in `Config.h` (GPIO 6 on C3/C6). Build and flash again after a change. |
| Length | `$led set_length <count>`. |
| Colours swapped | Red shows as green: `$led set_color_order GRB` (try `RGB` for other strips). |
| Slow to follow | With lights on the pad changes the strip at most every 2 s, and only for a visible step. |
| Always red | No sensor reading. See "The sensor reads offline". |

## The page does not open

| Check | How |
|---|---|
| IP address | `$wifi status` shows it. Use `http://`, not `https://`. |
| Not connected | `$wifi connect` and pick the network again. |
| Same network | Your computer must be on the same network. Guest networks often block devices. |

## Factory reset

Clears every setting: name, Wi-Fi, fans, curve, colours, strip.

```text
$system reset
```

Answer `y` within 15 s. The device restarts and asks the first-boot questions again.

From a computer, erase the flash while flashing:

```sh
./run.sh --chip c3
```

`./run.sh` erases the flash unless you pass `--keep-nvs`.

To reset one part only: `$pad reset`, `$fan reset`, `$mlx90614 reset` or `$led reset`.
