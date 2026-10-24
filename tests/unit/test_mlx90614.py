# SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
# xewe-chill-os/tests/unit/test_mlx90614.py
"""Source checks of src/Mlx90614/, and the value mapping of Convert.h built with g++."""

import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]  # the project: tests/unit/ -> tests/ -> root
SRC = ROOT / "src" / "Mlx90614"
UNIT = Path(__file__).resolve().parent
ID = "mlx90614"
NAME = "MLX90614"
PREFIX = "XEWE_MLX90614_"
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
    return (SRC / "Mlx90614.cpp").read_text()


def mlx_celsius(lsb: int, msb: int):
    """Python mirror of mlx90614_fx::raw_to_celsius: None for an error reply."""
    if msb & 0x80:
        return None
    return ((msb << 8) | lsb) * 0.02 - 273.15


@pytest.mark.unit
def test_sketch_declares_mlx90614():
    ino = (ROOT / "xewe-chill-os.ino").read_text()
    assert '#include "src/Mlx90614/Mlx90614.h"' in ino
    declared = re.search(r"^Mlx90614\s+mlx90614\s*\(\s*os\s*\)\s*;", ino, re.MULTILINE)
    assert declared, "Mlx90614 mlx90614(os); not declared"
    # the constructor takes the Os only; the values come from src/Mlx90614/Config.h
    assert re.search(
        r"explicit\s+Mlx90614\s*\(\s*xewe::Os&\s*host\s*\)\s*;",
        (SRC / "Mlx90614.h").read_text(),
    )


@pytest.mark.unit
def test_id_and_name_match_source():
    cpp = _cpp()
    assert ID_ARG.findall(cpp) == [ID]
    assert NAME_ARG.findall(cpp) == [NAME]
    assert "#include <ArduinoJson.h>" in cpp
    assert re.search(
        r"^ArduinoJson\s*=", (ROOT / "xewe.toml").read_text(), re.MULTILINE
    )


@pytest.mark.unit
def test_config_h_module_form():
    text = (SRC / "Config.h").read_text()
    lines = text.splitlines()
    assert lines[2] == "// xewe-chill-os/src/Mlx90614/Config.h"
    assert "#pragma once" in lines
    assert not re.search(r"^\s*#\s*include\b", text, re.MULTILINE)
    assert [line for line in lines if line in SECTIONS] == SECTIONS
    defines = dict(DEFINE.findall(text))
    assert set(defines) == {
        "XEWE_MLX90614_ADDR",
        "XEWE_MLX90614_SDA",
        "XEWE_MLX90614_SCL",
        "XEWE_MLX90614_LISTENERS_MAX",
        "XEWE_MLX90614_POLL_MS",
        "XEWE_MLX90614_OFFLINE_POLL_MS",
        "XEWE_MLX90614_SCAN_BUDGET_MS",
        "XEWE_MLX90614_SCAN_PROBE_MS",
    }
    assert all(name.startswith(PREFIX) for name in defines)
    # every runtime default names its settings key as mlx90614/<key>
    runtime = text.split(SECTIONS[0])[1].split(SECTIONS[1])[0]
    assert set(re.findall(rf"\({ID}/(\w+)\)", runtime)) == set(
        SETTING_KEY.findall(_cpp())
    )


@pytest.mark.unit
def test_source_reads_prefixed_values():
    cpp, header = _cpp(), (SRC / "Mlx90614.h").read_text()
    for name in (
        "XEWE_MLX90614_ADDR",
        "XEWE_MLX90614_SDA",
        "XEWE_MLX90614_SCL",
        "XEWE_MLX90614_POLL_MS",
        "XEWE_MLX90614_OFFLINE_POLL_MS",
        "XEWE_MLX90614_SCAN_BUDGET_MS",
        "XEWE_MLX90614_SCAN_PROBE_MS",
    ):
        assert name in cpp, name
    assert "XEWE_MLX90614_LISTENERS_MAX" in header
    both = cpp + header
    assert not re.search(r"(?<!XEWE_)\bMLX90614_(SDA|SCL|ADDR)\b", both)
    assert "XEWE_MODULE_MLX90614_" not in both
    assert "NO_PIN" not in both and "UNSET_PIN" in both


@pytest.mark.unit
def test_setting_keys_fit_nvs():
    keys = SETTING_KEY.findall(_cpp())
    assert keys == ["addr", "sda", "scl"]
    assert all(1 <= len(k) <= 15 for k in keys)


@pytest.mark.unit
def test_value_mapping():
    assert mlx_celsius(0xFF, 0xFF) is None  # glitched bus: never 1037.5 C
    assert abs(mlx_celsius(0x8D, 0x3A) - 26.63) < 0.01
    assert abs(mlx_celsius(0xFF, 0x7F) - 382.19) < 0.01
    assert (
        max(mlx_celsius(lo, hi) for hi in range(0x80) for lo in (0, 0xFF)) < 383
    )  # no valid word reads as > 383 C


@pytest.mark.unit
def test_convert_unit_gpp(tmp_path):
    """tests/unit/test_convert.cpp: raw -> C with the error flag, address validation, scan error streak."""
    gpp = shutil.which("g++")
    if gpp is None:
        pytest.skip("g++ not installed")
    exe = tmp_path / "test_convert"
    subprocess.run(
        [
            gpp,
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-o",
            str(exe),
            str(UNIT / "test_convert.cpp"),
        ],
        check=True,
    )
    run = subprocess.run(
        [str(exe)], capture_output=True, text=True, timeout=30, check=False
    )
    assert run.returncode == 0, run.stdout + run.stderr
    summary = re.search(r"PASSED: (\d+) check\(s\), 0 failure\(s\)", run.stdout)
    assert summary, run.stdout
    assert int(summary[1]) >= 28  # minimum check count: a silently emptied test fails


@pytest.mark.unit
def test_source_uses_pure_header():
    """Convert.h stays pure; the .cpp maps values through it and bounds the scan."""
    includes = re.findall(r"#include\s*[<\"]([^>\"]+)", (SRC / "Convert.h").read_text())
    assert includes and all(re.fullmatch(r"c[a-z]+|string", i) for i in includes), (
        includes
    )
    cpp = _cpp()
    for needle in (
        "mlx90614_fx::raw_to_celsius(",
        "mlx90614_fx::parse_address(",
        "mlx90614_fx::scan_error_streak(",
        "XEWE_MLX90614_SCAN_BUDGET_MS",
        "Wire.setTimeOut(XEWE_MLX90614_SCAN_PROBE_MS)",
        "Wire.setTimeOut(saved_timeout)",
    ):
        assert needle in cpp, needle
    assert "1037" not in cpp.replace(
        "1037 C", ""
    )  # only the comment that explains the error flag
