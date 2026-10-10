# Commands

Type a command in the serial console (115200 baud) or on `http://<device-ip>/console`.
Format: `$<group> <command> [arguments]`.

| Command | What it does |
|---|---|
| `$help` | List every group and command |
| `$help <group>` | List the commands of one group, e.g. `$help pad` |

Every group has `status` and `reset`. `reset` clears that group's settings and restarts the device.
Groups that can be switched off also have `enable` and `disable`: `fan`, `mlx90614`, `wifi`.
Groups with a settings table also have `get <key>`, `set <key> <value>` and `schema`.

## `$pad`: the temperature indicator

| Command | Arguments | Example | What it does |
|---|---|---|---|
| `set_cold` | `RRGGBB` or `#RRGGBB` | `$pad set_cold 00FFFF` | Strip colour at the first curve point. Saved. |
| `set_hot` | `RRGGBB` or `#RRGGBB` | `$pad set_hot FF0000` | Strip colour at the last curve point. Saved. |
| `lights` | `on` or `off` | `$pad lights off` | `on`: the strip shows the temperature. `off`: the strip shows the LED mode from `$led mode`. Saved. |
| `print_json` | | `$pad print_json` | Colours, curve and strip state as JSON. |
| `status` | | `$pad status` | Lights on/off, both colours, temperature, the colour shown now. |
| `reset` | | `$pad reset` | Colours back to cyan and red, lights on. Restarts. |

Replies: `Cold color set to #00FFFF`, `Failed to set hot color. Use RRGGBB or #RRGGBB.`,
`Pad lights off: the strip shows the led module's mode.`

## `$fan`: fans and the curve

| Command | Arguments | Example | What it does |
|---|---|---|---|
| `curve list` | | `$fan curve list` | Points, current temperature, target speed. |
| `curve set` | `T:P,T:P,...` or `none` | `$fan curve set 22:0,27:100,32:100` | Replace the whole curve. `none` empties it. Saved. |
| `curve add` | `<°C> <0-100>` | `$fan curve add 40.5 50` | Add a point, or change the point at that temperature. Saved. |
| `curve remove` | `<°C>` | `$fan curve remove 40.5` | Remove the point at that temperature. Saved. |
| `temp` | `<°C>` | `$fan temp 30` | Feed a temperature to the curve now, as the sensor would. |
| `add` | `<pwm_pin>` | `$fan add 3` | Add a fan without a tach wire. Saved. |
| `add_w_tach` | `<pwm_pin> <tach_pin>` | `$fan add_w_tach 3 0` | Add a fan with a tach wire. Saved. |
| `set` | `<pwm_pin> <0-255>` | `$fan set 3 128` | Set one fan's PWM duty. The curve overwrites it while it runs. |
| `set_all` | `<0-255>` | `$fan set_all 128` | Set every fan's PWM duty. Same rule. |
| `remove` | `<pwm_pin>` | `$fan remove 10` | Remove a fan and free its pins. Saved. |
| `set` | `curve_ms <100-60000>` | `$fan set curve_ms 1000` | How often the curve sets the fans, ms. |
| `set` | `stale_ms <1000-600000>` | `$fan set stale_ms 10000` | Age after which a reading counts as missing, ms. |
| `get` | `<key>` | `$fan get stale_ms` | Print one setting. |
| `schema` | | `$fan schema` | Every setting, fan and curve point as JSON lines. |
| `print_json` | | `$fan print_json` | Fans (speed in %, RPM), curve and target as JSON. |
| `status` | | `$fan status` | Each fan's pins, duty and RPM; the curve; temperature; target. |
| `enable` / `disable` | | `$fan disable` | Switch the fan group on or off. Disabled fans usually run at full speed. |
| `reset` | | `$fan reset` | Fans and curve back to the defaults. Restarts. |

## `$mlx90614`: the sensor

| Command | Arguments | Example | What it does |
|---|---|---|---|
| `read` | | `$mlx90614 read` | Read now. Reply: `Object: 31.20 C, ambient: 27.85 C`. |
| `scan` | | `$mlx90614 scan` | List the I²C addresses that answer. Takes up to 1.5 s. |
| `set_addr` | `<0x01-0x7F>` | `$mlx90614 set_addr 0x5A` | Sensor address, hex. Saved. |
| `set_pins` | `<sda> <scl>` | `$mlx90614 set_pins 4 5` | I²C pins. Restarts the bus. Saved. |
| `set` | `addr\|sda\|scl <value>` | `$mlx90614 set addr 90` | Same settings as a table; `addr` in decimal. |
| `get` / `schema` | | `$mlx90614 get sda` | Print settings. |
| `print_json` | | `$mlx90614 print_json` | Online, temperatures (`null` offline), pins, errors. |
| `status` | | `$mlx90614 status` | Pins, address, `Online: yes/no`, temperatures, read errors. |
| `enable` / `disable` | | | Switch the sensor on or off. |
| `reset` | | | Pins and address back to the defaults. Restarts. |

## `$led`: the strip

| Command | Arguments | Example | What it does |
|---|---|---|---|
| `on` / `off` | | `$led off` | Fade the strip in or out. Also hides the indicator. Saved. |
| `brightness` | `<0-255>` | `$led brightness 64` | Brightness, also of the indicator. Saved. |
| `set` | `<key> <value>` | `$led set num_led 12` | Strip settings: `num_led`, `pin_data`, `chip`, `colorder`, `brightness`, `state`, `voltage`. `pin_data` and `chip` apply after `$system restart`. |
| `get` / `schema` | | `$led get pin_data` | Print settings. |
| `fill` | `<rrggbb> [ms]` or `off` | `$led fill ff0000` | Show one colour. With `$pad lights on` the indicator takes the strip back on the next frame. |
| `mode list` | | `$led mode list` | LED modes. |
| `mode set` | `<id or name>` | `$led mode set rainbow` | Choose a mode. Visible after `$pad lights off`. |
| `mode color` | `<rrggbb>` | `$led mode color ff8000` | Colour of the current mode. |
| `mode param` | `<mode> <key> <value>` | `$led mode param 5 speed 7` | A mode parameter. |
| `mode speed` | `<value>` | `$led mode speed 10` | Speed of the current mode. |
| `mode reset_params` | `[mode]` | `$led mode reset_params` | Mode parameters back to defaults. |
| `checksum` | | `$led checksum` | CRC-32 of the current frame (for tests). |
| `status` | | `$led status` | Settings, pins in use, brightness, `Source: fill` (indicator) or `mode`. |
| `reset` | | `$led reset` | Strip settings and modes back to defaults. Restarts. |

## `$wifi`

| Command | Example | What it does |
|---|---|---|
| `connect` | `$wifi connect` | Connect, or pick a network and enter the password. |
| `disconnect` | `$wifi disconnect` | Disconnect. |
| `scan` | `$wifi scan` | List networks. |
| `status` | `$wifi status` | Network name, `Local ip:` and MAC address. |

## `$system`

| Command | Example | What it does |
|---|---|---|
| `restart` / `reboot` | `$system restart` | Restart the device. Settings stay. |
| `info` | `$system info` | Chip, flash and build information. |
| `status` | `$system status` | Every group, enabled or not. |
| `reset` | `$system reset` | Factory reset: clears every setting, Wi-Fi included. Asks `OK?`; answer `y` within 15 s. |
