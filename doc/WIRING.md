# Wiring

Connect all grounds: ESP32 GND, fan supply ground, strip supply ground, sensor GND.
Do not power fans or the strip from the ESP32's 3.3 V pin.

## Parts

| Item | Notes |
|---|---|
| ESP32-C3, ESP32-C6 or ESP32-S3 board | The pad was built with a C3. |
| 2 × 4-wire PWM fans (PC fans) | 12 V or 5 V, with their own supply. |
| MLX90614 sensor module (e.g. GY-906) | I²C, address 0x5A. Use a module with I²C pull-up resistors. |
| WS2812B LED strip | 12 LEDs on the pad. 5 V supply. |
| 2 × 10 kΩ resistors | Optional: external pull-ups on the tach lines. |
| Power supply | For the fans and the strip. Connect all grounds together. |

## Pins per chip

First-boot defaults. After the first boot the fan and sensor pins are stored on the device; change
them with the commands below the table. The strip data pin is fixed at build time.

| Signal | C3 (the built pad) | C6 | S3 |
|---|---|---|---|
| Fan 1 PWM | GPIO 3 | GPIO 3 | GPIO 4 |
| Fan 1 tach | GPIO 0 | GPIO 0 | GPIO 5 |
| Fan 2 PWM | GPIO 10 | GPIO 10 | GPIO 6 |
| Fan 2 tach | GPIO 1 | GPIO 1 | GPIO 7 |
| Sensor SDA | GPIO 4 | GPIO 4 | GPIO 8 |
| Sensor SCL | GPIO 5 | GPIO 5 | GPIO 9 |
| Sensor address | 0x5A | 0x5A | 0x5A |
| Strip data | GPIO 6 | GPIO 6 | GPIO 48 |
| Strip length | asked at first boot: answer 12 | same | same |

The C3 column is the firmware's default on every chip for the fans and the sensor; an S3 build
passes its pins (see [Build-time defaults](#build-time-defaults)). The strip data pin per chip is
set in `Config.h` (`XEWE_LED_PIN_DATA`).

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
  set the total length: `$led set_length 24`.
- A 300–500 Ω resistor in series with DIN, close to the strip, protects the first LED.

## Change pins after the first boot

| What | Command |
|---|---|
| Sensor pins | `$mlx90614 set_pins <sda> <scl>` |
| Sensor address | `$mlx90614 set_addr 0x5A` |
| Strip data pin | `XEWE_LED_PIN_DATA` in `Config.h`, then build and flash |
| Strip length | `$led set_length <count>` |
| Replace a fan | `$fan remove <pwm_pin>`, then `$fan add_w_tach <pwm_pin> <tach_pin>` |
| No second fan | `$fan remove <pwm_pin>` (GPIO 10 on a C3) |

A pin can only belong to one function. The device refuses a pin that another part already uses.

## Build-time defaults

Pass these to `./run.sh` or `xewe build` to change the first-boot pins, or set them in `Config.h`:

```sh
./run.sh --chip c3 --define XEWE_FAN_FAN2_PWM=255
./run.sh --chip s3 --define XEWE_FAN_FAN1_PWM=4 --define XEWE_FAN_FAN1_TACH=5 \
    --define XEWE_FAN_FAN2_PWM=6 --define XEWE_FAN_FAN2_TACH=7 \
    --define XEWE_MLX90614_SDA=8 --define XEWE_MLX90614_SCL=9
```

| Define | Meaning | Where the default lives |
|---|---|---|
| `XEWE_FAN_FAN1_PWM`, `XEWE_FAN_FAN1_TACH`, `XEWE_FAN_FAN2_PWM`, `XEWE_FAN_FAN2_TACH` | fan pins; 255 = no fan or no tach | `src/Fan/Config.h` |
| `XEWE_MLX90614_SDA`, `XEWE_MLX90614_SCL`, `XEWE_MLX90614_ADDR` | sensor pins and address | `src/Mlx90614/Config.h` |
| `XEWE_LED_PIN_DATA` | strip data pin | `Config.h` (led block) |

The fan and sensor values apply on the first boot only, or after `$<group> reset`. The strip data
pin applies on every boot.

## Pin notes per chip

| Chip | Notes |
|---|---|
| C3 | GPIO 0 (fan 1 tach) is fine after boot. GPIO 8 and 9 are boot pins, so the strip uses GPIO 6. |
| C6 | GPIO 8, 9 and 15 are boot pins; the defaults avoid them for fans and sensor. GPIO 4/5 (sensor) are JTAG pins; they work for I²C unless the board's JTAG fuse is burnt. On boards with a 32 kHz crystal, GPIO 0/1 are taken: move the tach pins, e.g. `--define XEWE_FAN_FAN1_TACH=18 --define XEWE_FAN_FAN2_TACH=19`. |
| S3 | GPIO 0, 3, 45 and 46 are boot pins; 19/20 are USB; 26–32 are flash. The defaults avoid all of them. |

The C6 notes come from the datasheet. The pad has not been built with a C6.
