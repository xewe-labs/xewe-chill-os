# Commands

Type a command in the serial console (115200 baud) or on `http://<device-ip>/console`.
Format: `$<group> <command> [arguments]`.

| Command | What it does |
|---|---|
| `$help` | List every group and command |
| `$help <group>` | List the commands of one group, e.g. `$help pad` |

Every group has `status` and `reset`. `reset` clears that group's settings and restarts the device.
Groups that can be switched off also have `enable` and `disable`: `fan`, `mlx90614`, `led`, `wifi`.
Groups with a settings table also have `get <key>`, `set <key> <value>` and `schema`.

## `$pad`: the temperature indicator

| Command | Arguments | Example | What it does |
|---|---|---|---|
| `set_cold` | `RRGGBB` or `#RRGGBB` | `$pad set_cold 00FFFF` | Strip colour at the first curve point. Saved. |
| `set_hot` | `RRGGBB` or `#RRGGBB` | `$pad set_hot FF0000` | Strip colour at the last curve point. Saved. |
| `lights` | `on` or `off` | `$pad lights off` | `on`: the strip follows the temperature (state, Solid mode, colour). `off`: the strip is left as it is, for the `$led` commands. Saved. |
| `print_json` | | `$pad print_json` | Colours, curve and strip state as JSON. |
| `status` | | `$pad status` | Lights on/off, both colours, temperature, the strip colour now. |
| `reset` | | `$pad reset` | Colours back to cyan and red, lights on. Restarts. |

Replies: `Cold color set to #00FFFF`, `Failed to set hot color. Use RRGGBB or #RRGGBB.`,
`Pad lights off: the strip is left to $led.`

With lights on, the pad changes the strip at most once every 2 s (`XEWE_PAD_COLOR_MIN_MS`), and only
when a colour channel moved by 8 or more (`XEWE_PAD_COLOR_STEP`) or the colour reached the cold or
hot colour. The led module stores each change.

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

With `$pad lights on` the pad sets the strip's state, mode and colour itself; run `$pad lights off`
first to keep a change made here.

| Command | Arguments | Example | What it does |
|---|---|---|---|
| `turn_on` / `turn_off` / `toggle_state` | | `$led turn_off` | Fade the strip in or out. Saved. |
| `set_state` | `0` or `1` | `$led set_state 1` | The same as a value. |
| `set_brightness` | `<0-255>` | `$led set_brightness 64` | Brightness. Saved. |
| `adj_brightness` | `<delta>` | `$led adj_brightness -10` | Change the brightness by a step. |
| `set_rgb` | `<r> <g> <b>` | `$led set_rgb 255 128 0` | Colour of the current mode. Also `set_r`, `set_g`, `set_b`, `adj_rgb`. |
| `set_hsv` | `<h> <s> <v>` | `$led set_hsv 75 255 255` | The same in HSV. Also `set_hue`, `set_sat`, `set_val`, `adj_hsv`. |
| `set_mode` | `<id>` | `$led set_mode 0` | Choose a mode (0 = Solid). `adj_mode 1` picks the next. |
| `set_mode_param` | `<key> <value>` | `$led set_mode_param speed 7` | A parameter of the current mode; `get_mode_params` lists them. |
| `reset_current_mode` | | | The current mode's parameters back to defaults. |
| `set_length` | `<count>` | `$led set_length 12` | Number of LEDs. |
| `set_color_order` | `RGB` ... `BGR` | `$led set_color_order GRB` | Colour order of the strip. |
| `set` | `<key> <value>` | `$led set chip 0` | Strip settings: `use_clk`, `chip`, `voltage`, `lines`, `num_led`, `colorder`. |
| `get` / `schema` | | `$led get num_led` | Print settings. |
| `status` | | `$led status` | Settings, colour, brightness, state and mode. |
| `reset` | | `$led reset` | Strip settings and modes back to defaults. Restarts. |

The data pin is fixed at build time (`XEWE_LED_PIN_DATA` in `Config.h`, see [wiring.md](wiring.md)).

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
