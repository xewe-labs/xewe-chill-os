# SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
"""Cooling pad project tests (PadWeb, id `pad`, and the glue between fan, mlx90614 and led).
Run from the project: `build/tools/.venv/bin/python -m xewe test`.

Hardware preconditions: a provisioned board (`xewe provision` answers the "enable?" prompts of fan
and mlx90614; pad and led ask nothing). Nothing needs to be wired: without a sensor mlx90614
reports offline every 5 s, PadWeb passes NaN to the fan curve and every fan runs at the last point's
speed (fail-safe), which `test_offline_sensor_fail_safe` checks. The closed loop needs the real pad.
"""
import os

import pytest

from xewe.board.serialio import BOOT_READY, BOOT_UNPROVISIONED, wait_for_banner

ID = "pad"
NAME = "Pad"


def test_compiles(compiled):
    # the pad firmware: wifi, web-interface, fan, mlx90614, led + PadWeb
    assert compiled.is_file()


def test_status(serial):
    serial.command(f"${ID} status", expect=rf"{NAME} module (enabled|disabled)", timeout=5)


def test_colours(serial):
    serial.command("$pad set_cold #12abef", expect=r"Cold color set to #12ABEF", timeout=5)
    serial.command("$pad set_cold 00ffff", expect=r"Cold color set to #00FFFF", timeout=5)
    serial.command("$pad set_hot red", expect=r"Failed to set hot color\. Use #RRGGBB\.", timeout=5)


def test_lights_toggle(serial):
    serial.command("$pad lights off", expect=r"Pad lights off: the strip shows the led module's mode\.", timeout=5)
    try:
        serial.collect(silence=0.5, limit=2)
        serial.command("$led status", expect=r"Source:\s+mode", timeout=5)
        serial.command("$pad lights maybe", expect=r"Usage: \$pad lights <on\|off>", timeout=5)
    finally:
        serial.command("$pad lights on", expect=r"Pad lights on: the strip follows the temperature\.", timeout=5)
    serial.collect(silence=0.5, limit=2)
    serial.command("$led status", expect=r"Source:\s+fill", timeout=5)   # PadWeb owns the strip again


def test_offline_sensor_fail_safe(serial):
    if os.environ.get("XEWE_TEST_MLX_PRESENT"):
        pytest.skip("a sensor is attached: the fail-safe path is not taken")
    # mlx90614 offline -> on_temperature(NaN) -> fan curve at the last point (default curve: 100 %)
    serial.collect(silence=1.5, limit=6)
    serial.command("$fan status", expect=r"Temperature: offline \(source\)", timeout=5)
    serial.command("$fan status", expect=r"Curve target: 100 %", timeout=5)
    serial.command("$pad status", expect=r"Strip: lit #FF0000", timeout=5)   # NaN counts as hot


def test_settings_survive_restart(serial):
    serial.command("$pad set_hot #FF8000", expect=r"Hot color set to #FF8000", timeout=5)
    try:
        serial.send("$system restart")
        m = wait_for_banner(serial, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", 90, reset=False)
        assert m[0] == BOOT_READY, "board came back unprovisioned"
        serial.collect(silence=0.5)
        serial.command("$pad status", expect=r"Hot colour: #FF8000", timeout=5)
    finally:
        serial.command("$pad set_hot #FF0000", expect=r"Hot color set to #FF0000", timeout=5)


def test_closed_loop(serial):
    if not os.environ.get("XEWE_TEST_PAD_HARDWARE"):
        pytest.skip("requires hardware: the assembled pad (sensor, fans with tach); XEWE_TEST_PAD_HARDWARE=1")
    serial.command("$mlx90614 read", expect=r"Object: -?\d+\.\d+ C", timeout=10)
    serial.collect(silence=2.5, limit=5)
    serial.command("$fan status", expect=r"Temperature: -?\d+\.\d+ C \(source\)", timeout=5)
    serial.command("$fan status", expect=r"\d+ RPM", timeout=5)
