"""Fan Curve maths unit tests: src/FanCurve/CurveMath.h (pure, no Arduino) built with g++ on the
developer machine, no board and no Arduino build.
Run: `build/tools/.venv/bin/python -m xewe test --unit-only`.
"""
import re
import shutil
import subprocess
from pathlib import Path

import pytest

MODULE_DIR = Path(__file__).resolve().parents[2] / "src" / "FanCurve"
CURVE_MATH_H = MODULE_DIR / "CurveMath.h"
UNIT_TEST = Path(__file__).resolve().parent / "test_curve_math.cpp"


@pytest.mark.unit
def test_curve_math_unit_gpp(tmp_path):
    """tests/unit/test_curve_math.cpp: interpolation, clamps, validation, schema, hysteresis, colours."""
    gpp = shutil.which("g++")
    if gpp is None:
        pytest.skip("g++ not installed")
    exe = tmp_path / "test_curve_math"
    subprocess.run([gpp, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-o", str(exe), str(UNIT_TEST)], check=True)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "PASSED" in run.stdout
    checks = int(re.search(r"PASSED: (\d+) check", run.stdout)[1])
    assert checks >= 15


@pytest.mark.unit
def test_curve_math_is_pure():
    """The header stays buildable with g++ alone, and FanCurve.cpp uses it instead of its own copies."""
    text = CURVE_MATH_H.read_text()
    includes = re.findall(r"#include\s*[<\"]([^>\"]+)", text)
    assert includes and all(re.fullmatch(r"c[a-z]+|string|vector", i) for i in includes), includes
    cpp = (MODULE_DIR / "FanCurve.cpp").read_text()
    for needle in ("curve_math::schema_ok(stored.schema)", "curve_math::validate_points(",
                   "curve_math::speed_to_pwm(", "curve_math::leds_on(", "curve_math::led_colour("):
        assert needle in cpp, needle
    assert "namespace curve_math {" not in cpp
    assert "SCHEMA                      = curve_math::SCHEMA" in (MODULE_DIR / "FanCurve.h").read_text()
