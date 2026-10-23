# SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
# SPDX-License-Identifier: GPL-3.0-only
# xewe-chill-os/tests/unit/test_pad.py
"""Unit tests of the cooling pad's glue (src/PadWeb/, id `pad`) and of the project's files: source
facts only, no board and no build. Fan and Mlx90614 have their own unit tests here; the repo modules
(wifi, web-interface, led) bring theirs."""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]  # the project: tests/unit/ -> tests/ -> root
SRC = ROOT / "src" / "PadWeb"
UNIT = Path(__file__).resolve().parent
SECTIONS = [
    "/* ---- start default runtime values ---- */",
    "/* ---- end default runtime values ---- */",
    "/* ---- start compile time defined constants ---- */",
    "/* ---- end compile time defined constants ---- */",
]
ID_ARG = re.compile(r'/\*\s*id\s*\*/\s*"([^"]*)"')
NAME_ARG = re.compile(r'/\*\s*name\s*\*/\s*"([^"]*)"')
DEFINE = re.compile(r"^#define\s+(\w+)\s+(.*)$", re.MULTILINE)


def _cpp():
    return (SRC / "PadWeb.cpp").read_text()


def _update_lights():
    cpp = _cpp()
    return cpp[cpp.index("void PadWeb::update_lights") : cpp.index("// ---- settings")]


@pytest.mark.unit
def test_id_and_name_match_source():
    cpp = _cpp()
    assert ID_ARG.findall(cpp) == ["pad"]
    assert NAME_ARG.findall(cpp) == ["Pad"]


@pytest.mark.unit
def test_config_h_module_form():
    text = (SRC / "Config.h").read_text()
    lines = text.splitlines()
    assert lines[2] == "// xewe-chill-os/src/PadWeb/Config.h"
    assert "#pragma once" in lines
    assert not re.search(r"^\s*#\s*include\b", text, re.MULTILINE)
    assert [line for line in lines if line in SECTIONS] == SECTIONS
    assert dict(DEFINE.findall(text)) == {
        "XEWE_PAD_UPDATE_MS": "250",
        "XEWE_PAD_COLOR_MIN_MS": "2000",
        "XEWE_PAD_COLOR_STEP": "8",
    }
    # no settings table: no runtime defaults
    runtime = text.split(SECTIONS[0])[1].split(SECTIONS[1])[0]
    assert "#define" not in runtime
    assert "xewe::setting<" not in _cpp()


@pytest.mark.unit
def test_glue_wiring():
    """The sensor -> curve link and the strip colour live in PadWeb, through the modules' public APIs."""
    cpp = _cpp()
    assert "mlx90614.listeners.add(this)" in cpp
    assert "fan.set_temperature(temperature, this)" in cpp
    assert (
        "temperature = online ? object_c : NAN" in cpp
    )  # offline -> NaN -> fail-safe hot
    assert (
        "curve_math::leds_on(" in cpp
        and "curve_math::led_colour(" in cpp
        and "fan.get_curve()" in cpp
    )
    assert "XEWE_PAD_UPDATE_MS" in cpp


@pytest.mark.unit
def test_strip_driven_through_led_sync_calls():
    """The strip: on/off, Solid mode and the curve colour through led.sync_state/sync_mode/sync_color."""
    assert re.search(r"SOLID_MODE\s*=\s*0\s*;", (SRC / "PadWeb.h").read_text())
    frame = _update_lights()
    assert "led.sync_state(lit, this)" in frame
    assert "led.sync_mode(SOLID_MODE, this)" in frame
    assert "led.sync_color(target, this)" in frame
    # each call only on a real change
    assert "led.get_state() != lit" in frame
    assert "led.get_current_mode_id() != SOLID_MODE" in frame
    assert "moved(led.get_rgb(), target, XEWE_PAD_COLOR_STEP, edge)" in frame


@pytest.mark.unit
def test_strip_changes_rate_limited():
    """Every led sync call is stored by the led module: at most one change per XEWE_PAD_COLOR_MIN_MS,
    and no direct NVS write from the frame loop."""
    cpp = _cpp()
    may = cpp[
        cpp.index("bool PadWeb::may_change") : cpp.index("void PadWeb::update_lights")
    ]
    assert "XEWE_PAD_COLOR_MIN_MS" in may
    frame = _update_lights()
    assert (
        "!may_change(now)" in frame.split("{", 1)[1].split(";", 1)[0]
    )  # the first statement
    assert "last_change_ms = now" in frame
    for persisted in (
        "led.set_state",
        "led.set_color",
        "led.set_brightness",
        "save(",
        "write_flex",
    ):
        assert persisted not in frame, persisted


@pytest.mark.unit
def test_post_writes_each_blob_at_most_once():
    cpp = _cpp()
    post = cpp[
        cpp.index("void PadWeb::handle_ui_config_post") : cpp.index("// ---- commands")
    ]
    assert post.count("save();") == 1 and post.count("fan.save_curve()") == 1
    assert 'set_cold_color(colors["start"].as<const char*>(), false)' in post
    assert "fan.set_curve(std::move(points), false, this)" in post


@pytest.mark.unit
def test_sketch_declares_project_modules():
    ino = (ROOT / "xewe-chill-os.ino").read_text()
    declares = re.findall(r"^(\w+)\s+\w+\s*\(.*\);\s*$", ino, re.MULTILINE)
    assert declares == ["XeWeOs", "Fan", "Mlx90614", "PadWeb"], declares
    assert "PadWeb pad_web(os, fan, mlx90614, led);" in ino
    assert "pad_web.attach(web_interface);" in ino
    # the repo modules (led, web_interface) come from src/Modules.h, before PadWeb
    assert ino.index('#include "src/Modules.h"') < ino.index("PadWeb pad_web(")
    toml = (ROOT / "xewe.toml").read_text()
    assert 'selected = ["wifi", "web-interface", "led"]' in toml


@pytest.mark.unit
def test_project_files_are_gpl():
    files = [ROOT / "xewe-chill-os.ino", ROOT / "Config.h", *UNIT.glob("*.cpp")]
    for folder in ("Fan", "Mlx90614", "PadWeb"):
        files += [
            *(ROOT / "src" / folder).glob("*.h"),
            *(ROOT / "src" / folder).glob("*.cpp"),
        ]
    for f in files:
        head = f.read_text().splitlines()[:3]
        assert "// SPDX-License-Identifier: GPL-3.0-only" in head, f
        assert head[2].startswith(
            f"// xewe-chill-os/{f.relative_to(ROOT).as_posix()}"
        ), f
    assert "GNU GENERAL PUBLIC LICENSE" in (ROOT / "LICENSE.txt").read_text()[:200]
    assert (
        not (ROOT / "LICENSE.md").exists() and not (ROOT / "LICENSE-NO-AI.md").exists()
    )
