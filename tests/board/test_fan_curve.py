"""Fan Curve module tests (project code, src/FanCurve/). Run from the project: `python -m xewe test`.

Hardware preconditions: a provisioned board with fan and mlx90614 enabled (fan_curve requires
both). Nothing needs to be wired: without a sensor the curve sees 255.0 C and drives every fan at
the last point's speed (fail-safe), which `test_offline_sensor_fail_safe` checks. The closed loop
(temperature -> PWM -> RPM) needs the real pad (`requires hardware`).
"""
import os

import pytest

from xewe.serialio import BOOT_READY, BOOT_UNPROVISIONED, wait_for_banner

ID = "fan_curve"
NAME = "Fan Curve"     # module.properties name == C++ name argument


def test_compiles(compiled):
    assert compiled.is_file()


def test_status(serial):
    serial.command(f"${ID} status", expect=rf"{NAME} module (enabled|disabled)", timeout=5)


def test_add_remove_point(serial):
    serial.command("$fan_curve add_point 99.5 42", expect=r"Curve point added\.", timeout=5)
    try:
        serial.command("$fan_curve status", expect=r"99\.50 C -> 42 % \(PWM 107\)", timeout=5)
        serial.command("$fan_curve add_point 99.5 43", expect=r"Curve point added\.", timeout=5)   # replaces
        serial.command("$fan_curve print_json", expect=r'\{"temp":99\.5,"speed":43\}', timeout=5)
        serial.command("$fan_curve add_point 50 101", expect=r"Failed to add curve point\.", timeout=5)
    finally:
        serial.command("$fan_curve remove_point 99.5", expect=r"Curve point removed\.", timeout=5)
    serial.command("$fan_curve remove_point 99.5", expect=r"Failed to remove curve point\.", timeout=5)


def test_colours(serial):
    serial.command("$fan_curve set_cold #12abef", expect=r"Cold color set to #12ABEF", timeout=5)
    serial.command("$fan_curve set_cold #00FFFF", expect=r"Cold color set to #00FFFF", timeout=5)
    serial.command("$fan_curve set_hot red", expect=r"Failed to set hot color\. Use #RRGGBB\.", timeout=5)


def test_offline_sensor_fail_safe(serial):
    if os.environ.get("XEWE_TEST_MLX_PRESENT"):
        pytest.skip("a sensor is attached: the fail-safe path is not taken")
    # no sensor -> 255.0 C -> last curve point; the default curve ends at 100 %
    serial.collect(silence=1.5, limit=3)
    serial.command("$fan_curve status", expect=r"Current target: 100 %", timeout=5)


def test_settings_survive_restart(serial):
    serial.command("$fan_curve add_point 88.25 33", expect=r"Curve point added\.", timeout=5)
    try:
        serial.send("$system restart")
        m = wait_for_banner(serial, f"{BOOT_READY}|{BOOT_UNPROVISIONED}", 90, reset=False)
        assert m[0] == BOOT_READY, "board came back unprovisioned"
        serial.collect(silence=0.5)
        serial.command("$fan_curve status", expect=r"88\.25 C -> 33 %", timeout=5)
    finally:
        serial.command("$fan_curve remove_point 88.25", expect=r"Curve point removed\.", timeout=5)


def test_closed_loop(serial):
    if not os.environ.get("XEWE_TEST_PAD_HARDWARE"):
        pytest.skip("requires hardware: the assembled pad (sensor, fans with tach); XEWE_TEST_PAD_HARDWARE=1")
    serial.command("$mlx90614 read", expect=r"Object: -?\d+\.\d+ C", timeout=10)
    serial.collect(silence=2.5, limit=5)
    serial.command("$fan status", expect=r"\d+ RPM", timeout=5)
