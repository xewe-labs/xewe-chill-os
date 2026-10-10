# SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
"""Cooling pad project unit tests (src/PadWeb/): pure logic and source checks on the developer
machine, no board and no build. The fan, mlx90614 and led modules bring their own unit tests.
Run: `build/tools/.venv/bin/python -m xewe test --unit-only`.
"""
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src" / "PadWeb"
UNIT = Path(__file__).resolve().parent


@pytest.mark.unit
def test_glide_unit_gpp(tmp_path):
    """tests/unit/test_glide.cpp: glide factor clamp (R1), convergence without overshoot, byte rounding."""
    gpp = shutil.which("g++")
    if gpp is None:
        pytest.skip("g++ not installed")
    exe = tmp_path / "test_glide"
    subprocess.run([gpp, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-o", str(exe), str(UNIT / "test_glide.cpp")],
                   check=True)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    assert run.returncode == 0, run.stdout + run.stderr
    summary = re.search(r"PASSED: (\d+) check\(s\), 0 failure\(s\)", run.stdout)
    assert summary, run.stdout
    assert int(summary[1]) >= 16


@pytest.mark.unit
def test_glide_header_is_pure():
    includes = re.findall(r"#include\s*[<\"]([^>\"]+)", (SRC / "Glide.h").read_text())
    assert includes and all(re.fullmatch(r"c[a-z]+", i) for i in includes), includes


@pytest.mark.unit
def test_glue_wiring():
    """The sensor -> curve link and the strip colour live in PadWeb, through the modules' public APIs."""
    cpp = (SRC / "PadWeb.cpp").read_text()
    assert "mlx90614.listeners.add(this)" in cpp                    # core 2.1 ListenerSet
    assert "fan.set_temperature(temperature, this)" in cpp
    assert "temperature = online ? object_c : NAN" in cpp            # offline -> NaN -> fail-safe hot
    assert "curve_math::leds_on(" in cpp and "curve_math::led_colour(" in cpp and "fan.get_curve()" in cpp
    assert "pad_fx::glide_factor(" in cpp and "led.fill(" in cpp
    # no persisted led setters from the frame loop (set_state/set_color/set_brightness write NVS)
    frame = cpp[cpp.index("void PadWeb::update_lights"):cpp.index("// ---- settings")]
    for persisted in ("led.set_state", "led.set_color", "led.set_brightness", "save(", "write_flex"):
        assert persisted not in frame, persisted


@pytest.mark.unit
def test_post_writes_each_blob_at_most_once():
    cpp = (SRC / "PadWeb.cpp").read_text()
    post = cpp[cpp.index("void PadWeb::handle_ui_config_post"):cpp.index("// ---- commands")]
    assert post.count("save();") == 1 and post.count("fan.save_curve()") == 1
    assert "set_cold_color(colors[\"start\"].as<const char*>(), false)" in post
    assert "fan.set_curve(std::move(points), false, this)" in post


@pytest.mark.unit
def test_sketch_declares_only_padweb():
    ino = (ROOT / "xewe-os-laptop-cooling-pad.ino").read_text()
    declares = re.findall(r"^(\w+)\s+\w+\s*\(.*\);\s*$", ino, re.M)
    assert declares == ["PadWeb"], declares
    assert "PadWeb pad_web(os, fan, mlx90614, led);" in ino
    toml = (ROOT / "xewe.toml").read_text()
    assert 'selected = ["wifi", "web-interface", "fan", "mlx90614", "led"]' in toml


@pytest.mark.unit
def test_project_files_are_gpl():
    files = [ROOT / "xewe-os-laptop-cooling-pad.ino", ROOT / "Config.h", *SRC.glob("*.h"), *SRC.glob("*.cpp"),
             *UNIT.glob("*.cpp")]
    for f in files:
        head = f.read_text().splitlines()[:3]
        assert "// SPDX-License-Identifier: GPL-3.0-only" in head, f
    assert "GNU GENERAL PUBLIC LICENSE" in (ROOT / "LICENSE.txt").read_text()[:200]
    assert not (ROOT / "LICENSE.md").exists() and not (ROOT / "LICENSE-NO-AI.md").exists()
