"""Fan module tests (project-local module, src/Fan/). Run from the project: `python -m xewe test`.

Hardware preconditions: a provisioned board (first boot done; `xewe provision` answers the
"Would you like to enable ...?" prompts). `test_add_set_remove` needs a free output GPIO that is
not one of the Config.h fan pins: XEWE_TEST_FAN_PIN (PWM into nothing is harmless). The RPM test
needs a real 4-wire fan: XEWE_TEST_FAN_PWM_PIN + XEWE_TEST_FAN_TACH_PIN wired to it.
"""
import os
import re
from pathlib import Path

import pytest

ID = "fan"
NAME = "Fan"          # module.properties name == C++ name argument
MODULE_DIR = Path(__file__).resolve().parents[1] / "src" / "Fan"


def test_compiles(compiled):
    # build of the project firmware (Fan declared in the .ino) for the session chip
    assert compiled.is_file()


def test_status(serial):
    serial.command(f"${ID} status", expect=rf"{NAME} module (enabled|disabled)", timeout=5)


def test_add_set_remove(serial):
    pin = os.environ.get("XEWE_TEST_FAN_PIN")
    if not pin:
        pytest.skip("needs XEWE_TEST_FAN_PIN (a free output GPIO, not a Config.h fan pin)")
    serial.command(f"$fan add {pin}", expect=r"Fan added\.", timeout=5)
    serial.command(f"$fan set {pin} 128", expect=r"Speed updated\.", timeout=5)
    # fan_curve (when enabled) rewrites every fan speed once a second, so the speed is not checked
    serial.command("$fan status", expect=rf"PWM pin {pin}, speed \d+, no tach", timeout=5)
    serial.command(f"$fan add {pin}", expect=r"Failed to add fan\.", timeout=5)      # duplicate
    serial.command(f"$fan remove {pin}", expect=r"Fan removed\.", timeout=5)
    serial.command(f"$fan set {pin} 10", expect=r"Failed to set fan speed\.", timeout=5)


def test_set_rejects_out_of_range(serial):
    serial.command("$fan set 4 256", expect=r"Failed to set fan speed\.", timeout=5)


def test_argc_error(serial):
    serial.command("$fan set 1", expect=r"Argument count mismatch for '\$fan set'; expected 2, got 1", timeout=5)


def test_rpm_reading(serial):
    pwm = os.environ.get("XEWE_TEST_FAN_PWM_PIN")
    tach = os.environ.get("XEWE_TEST_FAN_TACH_PIN")
    if not (pwm and tach):
        pytest.skip("requires hardware: a 4-wire fan on XEWE_TEST_FAN_PWM_PIN / XEWE_TEST_FAN_TACH_PIN")
    serial.command(f"$fan set {pwm} 255", expect=r"Speed updated\.", timeout=5)
    m = None
    for _ in range(10):  # the RPM filter needs a few 1 s windows
        serial.send("$fan status")
        m = serial.expect(rf"PWM pin {pwm}, speed 255, tach pin {tach}, (\d+) RPM", timeout=5)
        if int(m[1]) > 0:
            break
        serial.collect(silence=1.5, limit=2)   # next 1 s RPM window
    assert m and int(m[1]) > 0, "fan reports 0 RPM at full speed"


@pytest.mark.host
def test_properties_match_source():
    props = dict(l.split("=", 1) for l in (MODULE_DIR / "module.properties").read_text().splitlines() if "=" in l)
    cpp = (MODULE_DIR / f"{props['folder']}.cpp").read_text()
    assert props["id"] == ID and f'"{ID}"' in cpp and f'"{NAME}"' in cpp
    assert re.fullmatch(r"[a-z][a-z0-9_]{0,14}", props["id"])
