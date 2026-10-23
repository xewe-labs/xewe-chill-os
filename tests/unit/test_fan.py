# SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
# xewe-chill-os/tests/unit/test_fan.py
"""Unit tests of the project-local Fan module (src/Fan/): source facts and the pure curve maths
(src/Fan/Curve.h, built with the host g++), no board and no build. The sketch declares it."""

import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]  # the project: tests/unit/ -> tests/ -> root
SRC = ROOT / "src" / "Fan"
UNIT = Path(__file__).resolve().parent
ID = "fan"
NAME = "Fan"
PREFIX = "XEWE_FAN_"
SECTIONS = [
    "/* ---- start default runtime values ---- */",
    "/* ---- end default runtime values ---- */",
    "/* ---- start compile time defined constants ---- */",
    "/* ---- end compile time defined constants ---- */",
]
ID_ARG = re.compile(r'/\*\s*id\s*\*/\s*"([^"]*)"')
NAME_ARG = re.compile(r'/\*\s*name\s*\*/\s*"([^"]*)"')
SETTING_KEY = re.compile(r'xewe::setting<[^>]*>\s*\(\s*"([^"]*)"')
DEFINE = re.compile(r"^#define\s+(\w+)\s+(.*)$", re.MULTILINE)


def _cpp():
    return (SRC / "Fan.cpp").read_text()


@pytest.mark.unit
def test_sketch_declares_fan():
    ino = (ROOT / "xewe-chill-os.ino").read_text()
    assert '#include "src/Fan/Fan.h"' in ino
    assert re.search(r"^Fan\s+fan\s*\(\s*os\s*\)\s*;", ino, re.MULTILINE), "Fan fan(os); not declared"
    # the constructor takes the Os only; the values come from src/Fan/Config.h
    assert re.search(r"explicit\s+Fan\s*\(\s*xewe::Os&\s*host\s*\)\s*;", (SRC / "Fan.h").read_text())


@pytest.mark.unit
def test_id_and_name_match_source():
    cpp = _cpp()
    assert ID_ARG.findall(cpp) == [ID]
    assert NAME_ARG.findall(cpp) == [NAME]
    # ArduinoJson is pinned in the manifest
    assert "#include <ArduinoJson.h>" in cpp
    assert re.search(r"^ArduinoJson\s*=", (ROOT / "xewe.toml").read_text(), re.MULTILINE)


@pytest.mark.unit
def test_config_h_module_form():
    text = (SRC / "Config.h").read_text()
    lines = text.splitlines()
    assert lines[2] == "// xewe-chill-os/src/Fan/Config.h"
    assert "#pragma once" in lines
    assert not re.search(r"^\s*#\s*include\b", text, re.MULTILINE)
    assert [line for line in lines if line in SECTIONS] == SECTIONS
    defines = dict(DEFINE.findall(text))
    assert set(defines) == {
        "XEWE_FAN_CURVE_MS",
        "XEWE_FAN_STALE_MS",
        "XEWE_FAN_FAN1_PWM",
        "XEWE_FAN_FAN1_TACH",
        "XEWE_FAN_FAN2_PWM",
        "XEWE_FAN_FAN2_TACH",
        "XEWE_FAN_EMA_PCT",
        "XEWE_FAN_MAX_RPM",
        "XEWE_FAN_RPM_ROUNDING",
    }
    assert all(name.startswith(PREFIX) for name in defines)
    # every runtime default names its settings key as fan/<key>
    runtime = text.split(SECTIONS[0])[1].split(SECTIONS[1])[0]
    assert set(re.findall(rf"\({ID}/(\w+)\)", runtime)) == set(SETTING_KEY.findall(_cpp()))


@pytest.mark.unit
def test_source_reads_prefixed_values():
    cpp, header = _cpp(), (SRC / "Fan.h").read_text()
    for name in ("XEWE_FAN_FAN1_PWM", "XEWE_FAN_FAN1_TACH", "XEWE_FAN_FAN2_PWM", "XEWE_FAN_FAN2_TACH",
                 "XEWE_FAN_EMA_PCT", "XEWE_FAN_MAX_RPM", "XEWE_FAN_RPM_ROUNDING"):
        assert name in cpp, name
    for name in ("XEWE_FAN_CURVE_MS", "XEWE_FAN_STALE_MS"):
        assert name in cpp and name in header, name
    both = cpp + header
    assert not re.search(r"(?<!XEWE_FAN_)\bFAN[12]_(PWM|TACH)\b", both)
    assert "NO_PIN" not in both and "UNSET_PIN" in both


@pytest.mark.unit
def test_setting_keys_fit_nvs():
    keys = SETTING_KEY.findall(_cpp())
    assert keys == ["curve_ms", "stale_ms"]
    assert all(1 <= len(k) <= 15 for k in keys)


@pytest.mark.unit
def test_curve_unit_gpp(tmp_path):
    """tests/unit/test_curve.cpp: interpolation, clamps, validation, schema, `curve set` parsing,
    hysteresis, the never-received fail-safe and colours of src/Fan/Curve.h."""
    gpp = shutil.which("g++")
    if gpp is None:
        pytest.skip("g++ not installed")
    exe = tmp_path / "test_curve"
    subprocess.run([gpp, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-o", str(exe), str(UNIT / "test_curve.cpp")],
                   check=True)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30, check=False)
    assert run.returncode == 0, run.stdout + run.stderr
    summary = re.search(r"PASSED: (\d+) check\(s\), 0 failure\(s\)", run.stdout)
    assert summary, run.stdout
    assert int(summary[1]) >= 78      # minimum check count: a silently emptied test fails


@pytest.mark.unit
def test_curve_header_is_pure():
    """Curve.h builds with g++ alone, and Fan.cpp calls it instead of keeping its own copy."""
    includes = re.findall(r"#include\s*[<\"]([^>\"]+)", (SRC / "Curve.h").read_text())
    assert includes and all(re.fullmatch(r"c[a-z]+|string|vector", i) for i in includes), includes
    cpp = _cpp()
    for needle in ("curve_math::schema_ok(stored.schema)", "curve_math::validate_points(",
                   "curve_math::speed_to_pwm(", "curve_math::target_speed(", "curve_math::parse_curve_spec(",
                   "curve_math::never_received("):
        assert needle in cpp, needle
    assert "namespace curve_math {" not in cpp


@pytest.mark.unit
def test_tach_maths_shared_pins_and_foreign_blob():
    """64-bit tach maths, shared-pin refusal, a schema mismatch never overwritten at boot."""
    cpp = _cpp()
    assert "static_cast<uint64_t>(pulses) * 30000u" in cpp
    assert "f->pin_tach == pwm_pin" in cpp and "f->pin_pwm == tach_pin" in cpp
    load = cpp[cpp.index("void Fan::load()"):cpp.index("void Fan::save()")]
    assert "if (foreign)" in load and load.index("if (foreign)") < load.rindex("save();")
    # the curve loop never writes NVS
    run = cpp[cpp.index("void Fan::run_curve"):cpp.index("bool Fan::curve_add")]
    assert "set_all(" in run and ", false)" in run and "save" not in run
