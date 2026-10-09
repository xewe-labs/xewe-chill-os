"""ARGB module tests (project-local module, src/Argb/). Run from the project: `python -m xewe test`.

Hardware preconditions: a provisioned board. `test_add_rgb_remove` needs a free GPIO that is not a
Config.h strip pin: XEWE_TEST_ARGB_PIN (on an S3 devkit the onboard WS2812 on GPIO 48 makes the
result visible; nothing attached is harmless). The C3 and C6 have two RMT TX channels, so with the
two default strips present the add is refused there: remove one first or run it on an S3 (four).
Colour and glide checks need a strip and eyes (`requires hardware`).
"""
import os

import pytest

ID = "argb"
NAME = "ARGB"          # module.properties name == C++ name argument


def test_compiles(compiled):
    assert compiled.is_file()


def test_status(serial):
    serial.command(f"${ID} status", expect=rf"{NAME} module (enabled|disabled)", timeout=5)


def test_add_rgb_remove(serial):
    pin = os.environ.get("XEWE_TEST_ARGB_PIN")
    if not pin:
        pytest.skip("needs XEWE_TEST_ARGB_PIN (a free GPIO, not a Config.h strip pin)")
    serial.command(f"$argb add {pin}", expect=r"LED strip added\.", timeout=5)
    # fan_curve (when enabled) rewrites every strip once a second; with no sensor that is state on and
    # the hot colour #FF0000, so the test sets exactly that to keep the read-back stable (C2 fix)
    try:
        serial.command(f"$argb set_rgb {pin} 255 0 0", expect=r"RGB updated\.", timeout=5)
        serial.command(f"$argb set_state {pin} 1", expect=r"State updated\.", timeout=5)
        serial.command("$argb status", expect=rf"Pin {pin} \(\d+ LEDs\), on, RGB \(255, 0, 0\)", timeout=5)
        serial.command(f"$argb set_length {pin} 1", expect=r"Length updated\.", timeout=5)
        serial.command(f"$argb set_state {pin} 2", expect=r"Failed to update LED state\.", timeout=5)
        serial.command(f"$argb print_json", expect=rf'"pin":{pin},"length":1,"state":true,"r":255,"g":0,"b":0', timeout=5)
    finally:
        serial.command(f"$argb remove {pin}", expect=r"LED strip removed\.", timeout=5)


def test_rejects_bad_values(serial):
    serial.command("$argb set_rgb 6 256 0 0", expect=r"Failed to update RGB\.", timeout=5)
    serial.command("$argb remove 200", expect=r"Failed to remove LED strip\.", timeout=5)


def test_colour_on_strip(serial):
    if not os.environ.get("XEWE_TEST_ARGB_VISUAL"):
        pytest.skip("requires hardware: a WS2812B strip and someone looking at it (XEWE_TEST_ARGB_VISUAL=1)")
    serial.command("$argb print_json", expect=r'"state":', timeout=5)
