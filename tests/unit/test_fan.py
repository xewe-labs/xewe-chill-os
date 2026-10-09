"""Fan module (project-local module, src/Fan/) unit tests:
pure logic on the developer machine, no board and no build.
Run: `build/tools/.venv/bin/python -m xewe test --unit-only`.
"""
import re
from pathlib import Path

import pytest

ID = "fan"
NAME = "Fan"          # module.properties name == C++ name argument
MODULE_DIR = Path(__file__).resolve().parents[2] / "src" / "Fan"


@pytest.mark.unit
def test_properties_match_source():
    props = dict(l.split("=", 1) for l in (MODULE_DIR / "module.properties").read_text().splitlines() if "=" in l)
    cpp = (MODULE_DIR / f"{props['folder']}.cpp").read_text()
    assert props["id"] == ID and f'"{ID}"' in cpp and f'"{NAME}"' in cpp
    assert re.fullmatch(r"[a-z][a-z0-9_]{0,14}", props["id"])
