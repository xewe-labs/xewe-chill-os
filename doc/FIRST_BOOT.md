# First boot

`./run.sh --chip c3` builds, erases the whole flash, flashes and opens the serial console
(115200 baud), so every run is a first boot. Add `--keep-nvs` to keep the settings. Without a
board the commands compile and say `compiled, not run`.

## The questions

Answer them in the console, or run `build/tools/.venv/bin/python -m xewe provision` to answer them
from your settings (see [XEWE_OS_ECOSYSTEM.md](XEWE_OS_ECOSYSTEM.md)).

1. Name the device.
2. Wi-Fi: pick your network, enter the password.
3. The strip: no clock line, WS2812B from the chipset menu, 5 V, one strip of 12 LEDs, then the
   colour-order check (the strip shows green, then red; answer what you see).

## What the device stores

Both fans, the sensor pins, the curve 22 °C → 0 %, 27 °C → 100 %, 32 °C → 100 %, cold colour
`#00FFFF` (cyan) and hot colour `#FF0000` (red). The pins are in [WIRING.md](WIRING.md). The boot
log prints the IP address; open `http://<device-ip>/`.

## Check the parts

```text
$mlx90614 read
$fan status
$pad status
```

The first test on real hardware is [tests/MANUAL.md](../tests/MANUAL.md). If a check fails, see
[TROUBLESHOOTING.md](TROUBLESHOOTING.md).
