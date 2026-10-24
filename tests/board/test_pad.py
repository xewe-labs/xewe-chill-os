# SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
# xewe-chill-os/tests/board/test_pad.py
"""Board tests of `$pad`; the preconditions and keys are in tests/README.md."""

import os
import re

import pytest
from xewe.board.serialio import BOOT_READY, BOOT_UNPROVISIONED, wait_for_banner

ID = "pad"
NAME = "Pad"
SETTLE_S = 3.0  # > XEWE_PAD_COLOR_MIN_MS + XEWE_PAD_UPDATE_MS
HEX = r"#[0-9A-F]{6}"


def _send(serial, cmd):
    """Send a command that prints nothing else and wait for the board's echo of it."""
    serial.command(cmd, expect=rf"^{re.escape(cmd)}$", timeout=5)


def _text(serial, cmd):
    serial.send(cmd)
    return "\n".join(serial.collect(silence=1.0, limit=10))


def _pad(serial):
    """Lights, colours and strip from `$pad status`."""
    text = _text(serial, f"${ID} status")
    strip = re.search(rf"Strip: (lit|dark) ({HEX})", text)
    return {
        "lights": re.search(r"Lights: (on|off)", text)[1],
        "cold": re.search(rf"Cold color: ({HEX})", text)[1],
        "hot": re.search(rf"Hot color: ({HEX})", text)[1],
        "strip": strip.groups() if strip else None,
    }


def _led(serial):
    """Mode, colour and state from `$led status`."""
    text = _text(serial, "$led status")
    return {
        "mode": int(re.search(r"Mode: \[(\d)\]", text)[1]),
        "rgb": tuple(
            int(v)
            for v in re.search(
                r"Color \(RGB\):\s+\((\d+), (\d+), (\d+)\)", text
            ).groups()
        ),
        "on": bool(re.search(r"Power State:\s+ON", text)),
    }


def _settle(serial):
    serial.collect(silence=SETTLE_S, limit=SETTLE_S + 1)


def _lights(serial, on):
    word = "on" if on else "off"
    tail = (
        r"on: the strip follows the temperature\."
        if on
        else r"off: the strip is left to \$led\."
    )
    serial.command(f"${ID} lights {word}", expect=rf"Pad lights {tail}", timeout=5)


def test_compiles(compiled):
    # the pad firmware: wifi, web-interface, led + Fan, Mlx90614, PadWeb
    assert compiled.is_file()


def test_status(serial):
    serial.command(
        f"${ID} status", expect=rf"{NAME} module (enabled|disabled)", timeout=5
    )
    serial.expect(
        r"Lights: (on \(strip follows the temperature\)|off \(strip left to \$led\))",
        timeout=5,
    )
    serial.expect(rf"Cold color: {HEX}", timeout=5)
    serial.expect(rf"Hot color: {HEX}", timeout=5)
    serial.expect(
        r"Temperature: (offline \(no sensor reading\)|-?\d+\.\d+ C)", timeout=5
    )


def test_print_json(serial):
    serial.send(f"${ID} print_json")
    serial.expect(
        rf'^\{{"curve_edge_colors":\{{"start":"{HEX}","end":"{HEX}"\}},"temp_curve":\[',
        timeout=5,
    )
    serial.expect(
        rf'^\{{"lights":(true|false),"shown":"{HEX}","temperature":', timeout=5
    )


def test_colours(serial):
    cold = _pad(serial)["cold"]
    try:
        serial.command(
            f"${ID} set_cold #12abef", expect=r"Cold color set to #12ABEF", timeout=5
        )
        serial.command(
            f"${ID} set_cold 00ffff", expect=r"Cold color set to #00FFFF", timeout=5
        )
        serial.command(
            f"${ID} set_hot red",
            expect=r"Failed to set hot color\. Use RRGGBB or #RRGGBB\.",
            timeout=5,
        )
        serial.command(
            f"${ID} set_cold #12345", expect=r"Failed to set cold color\.", timeout=5
        )
    finally:
        serial.command(
            f"${ID} set_cold {cold}", expect=rf"Cold color set to {cold}", timeout=5
        )


def test_lights_off_leaves_strip_to_led(serial):
    before = _led(serial)
    _lights(serial, False)
    try:
        serial.command(
            f"${ID} status", expect=r"Lights: off \(strip left to \$led\)", timeout=5
        )
        # a mode set by hand stays: PadWeb no longer drives the strip
        _send(serial, "$led set_mode 5")
        _settle(serial)
        assert _led(serial)["mode"] == 5
        serial.command(
            f"${ID} lights maybe", expect=r"Usage: \$pad lights <on\|off>", timeout=5
        )
    finally:
        _lights(serial, True)
    _settle(serial)
    pad = _pad(serial)
    assert pad["lights"] == "on" and pad["strip"] is not None
    if pad["strip"][0] == "lit":
        # PadWeb owns the strip again: on, Solid mode
        led = _led(serial)
        assert led["on"] and led["mode"] == 0, led
    else:
        # below the first curve point the strip is off and keeps the mode it had
        assert not _led(serial)["on"]
        _send(serial, f"$led set_mode {before['mode']}")


def test_offline_sensor_fail_safe(serial):
    if os.environ.get("XEWE_TEST_MLX_PRESENT"):
        pytest.skip("a sensor is attached: the fail-safe path is not taken")
    pad = _pad(serial)
    if pad["lights"] != "on":
        pytest.skip("$pad lights is off: the strip is left to $led")
    points = re.findall(r"([-\d.]+) C -> (\d+) %", _text(serial, "$fan curve list"))
    if not points:
        pytest.skip("the fan curve has no points")
    # mlx90614 offline -> on_temperature(NaN) -> fan curve at the last point
    _settle(serial)
    serial.command("$fan status", expect=r"Temperature: offline \(source\)", timeout=5)
    serial.command("$fan status", expect=rf"Curve target: {points[-1][1]} %", timeout=5)
    serial.command(
        f"${ID} status", expect=r"Temperature: offline \(no sensor reading\)", timeout=5
    )
    # NaN counts as hot: the strip is lit in the hot colour, exactly, in Solid mode
    serial.command(f"${ID} status", expect=rf"Strip: lit {pad['hot']}", timeout=5)
    led = _led(serial)
    hot = tuple(int(pad["hot"][i : i + 2], 16) for i in (1, 3, 5))
    assert led["on"] and led["mode"] == 0 and led["rgb"] == hot, led


def test_closed_loop(serial):
    if not os.environ.get("XEWE_TEST_PAD_HARDWARE"):
        pytest.skip(
            "requires hardware: the assembled pad (sensor, fans with tach); XEWE_TEST_PAD_HARDWARE=1"
        )
    serial.command(
        "$mlx90614 read", expect=r"MLX90614: object -?\d+\.\d+ C", timeout=10
    )
    serial.collect(silence=2.5, limit=5)
    serial.command(
        "$fan status", expect=r"Temperature: -?\d+\.\d+ C \(source\)", timeout=5
    )
    serial.command("$fan status", expect=r"\d+ RPM", timeout=5)
    serial.command(f"${ID} status", expect=r"Temperature: -?\d+\.\d+ C", timeout=5)


def test_settings_survive_restart(serial):
    # last in the file: a restart that does not come back then takes down only this test
    hot = _pad(serial)["hot"]
    serial.command(
        f"${ID} set_hot #FF8000", expect=r"Hot color set to #FF8000", timeout=5
    )
    try:
        _lights(serial, False)
        serial.send("$system restart")
        m = wait_for_banner(
            serial, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", 90, reset=False
        )
        assert m[0] == BOOT_READY, "board came back unprovisioned"
        serial.collect(silence=0.5)
        pad = _pad(serial)
        assert pad["hot"] == "#FF8000" and pad["lights"] == "off", pad
    finally:
        serial.command(
            f"${ID} set_hot {hot}", expect=rf"Hot color set to {hot}", timeout=5
        )
        _lights(serial, True)
