# Wiring

Connect all grounds: ESP32 GND, fan supply ground, strip supply ground, sensor GND.
Do not power fans or the strip from the ESP32's 3.3 V pin.

## Pins per chip

First-boot defaults. After the first boot the pins are stored on the device; change them with the
commands below the table.

| Signal | C3 (the built pad) | C6 | S3 |
|---|---|---|---|
| Fan 1 PWM | GPIO 3 | GPIO 3 | GPIO 4 |
| Fan 1 tach | GPIO 0 | GPIO 0 | GPIO 5 |
| Fan 2 PWM | GPIO 10 | GPIO 10 | GPIO 6 |
| Fan 2 tach | GPIO 1 | GPIO 1 | GPIO 7 |
| Sensor SDA | GPIO 4 | GPIO 4 | GPIO 8 |
| Sensor SCL | GPIO 5 | GPIO 5 | GPIO 9 |
| Sensor address | 0x5A | 0x5A | 0x5A |
| Strip data | GPIO 6 with `--define LED_PIN_DATA=6`, else GPIO 8 | same as C3 | GPIO 48 |
| Strip length | 12 with `--define LED_COUNT=12`, else 60 | same | 60 |

## Fans (4-wire PC fans)

| Fan wire (usual colour) | Connect to |
|---|---|
| GND (black) | common ground |
| +12 V or +5 V (yellow or red) | fan supply |
| Tach / sense (green) | tach GPIO |
| PWM / control (blue) | PWM GPIO |

- PWM: 25 kHz, 8 bit. The ESP32 drives the PWM wire directly at 3.3 V.
- Tach: the ESP32 uses its internal pull-up. Add a 10 kΩ resistor from each tach GPIO to 3.3 V if
  the RPM reading jumps or freezes.
- A fan without a tach wire works too: `$fan add <pwm_pin>`.

## Sensor (MLX90614)

| Sensor pin | Connect to |
|---|---|
| VIN / VCC | the voltage its module asks for (3.3 V or 5 V) |
| GND | common ground |
| SDA | SDA GPIO |
| SCL | SCL GPIO |

- The I²C lines need pull-up resistors. Most GY-906 modules have them on board.
- Point the sensor at the laptop's underside, near the hottest spot.

## LED strip (WS2812B)

| Strip pad | Connect to |
|---|---|
| 5 V | 5 V supply |
| GND | common ground |
| DIN | strip data GPIO |

- One data pin drives the strip. For two strips, wire both DIN pads to the same GPIO and keep
  `num_led` at the length of one strip. Or chain them (DOUT of the first to DIN of the second) and
  set the total length: `$led set num_led 24`.
- A 300–500 Ω resistor in series with DIN, close to the strip, protects the first LED.

## Change pins after the first boot

| What | Command |
|---|---|
| Sensor pins | `$mlx90614 set_pins <sda> <scl>` |
| Sensor address | `$mlx90614 set_addr 0x5A` |
| Strip data pin | `$led set pin_data <gpio>`, then `$system restart` |
| Strip length | `$led set num_led <count>` |
| Replace a fan | `$fan remove <pwm_pin>`, then `$fan add_w_tach <pwm_pin> <tach_pin>` |
| No second fan | `$fan remove <pwm_pin>` (GPIO 10 on a C3) |

A pin can only belong to one function. The device refuses a pin that another part already uses.

## Build-time defaults

Pass these to `./run.sh` or `xewe build` to change the first-boot pins:

```sh
./run.sh --chip c3 --define LED_PIN_DATA=6 --define LED_COUNT=12 --define FAN2_PWM=255
```

| Define | Meaning |
|---|---|
| `FAN1_PWM`, `FAN1_TACH`, `FAN2_PWM`, `FAN2_TACH` | fan pins; 255 = no fan or no tach |
| `MLX90614_SDA`, `MLX90614_SCL`, `MLX90614_ADDR` | sensor pins and address |
| `LED_PIN_DATA`, `LED_COUNT` | strip data pin and length |

They apply on the first boot only, or after `$<group> reset`.

## Pin notes per chip

| Chip | Notes |
|---|---|
| C3 | GPIO 0 (fan 1 tach) is fine after boot. GPIO 8 and 9 are boot pins: keep the strip off GPIO 8 when you can. |
| C6 | GPIO 8, 9 and 15 are boot pins; the defaults avoid them for fans and sensor. GPIO 4/5 (sensor) are JTAG pins; they work for I²C unless the board's JTAG fuse is burnt. On boards with a 32 kHz crystal, GPIO 0/1 are taken: move the tach pins, e.g. `--define FAN1_TACH=18 --define FAN2_TACH=19`. |
| S3 | GPIO 0, 3, 45 and 46 are boot pins; 19/20 are USB; 26–32 are flash. The defaults avoid all of them. |

The C6 notes come from the datasheet. The pad has not been built with a C6.
