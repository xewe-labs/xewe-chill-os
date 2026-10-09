"""Fan Curve module (project code, src/FanCurve/) unit tests:
pure logic on the developer machine, no board and no build.
Run: `build/tools/.venv/bin/python -m xewe test --unit-only`.
"""
from pathlib import Path

import pytest

ID = "fan_curve"
NAME = "Fan Curve"     # module.properties name == C++ name argument
MODULE_DIR = Path(__file__).resolve().parents[2] / "src" / "FanCurve"


@pytest.mark.unit
def test_properties_match_source():
    props = dict(l.split("=", 1) for l in (MODULE_DIR / "module.properties").read_text().splitlines() if "=" in l)
    cpp = (MODULE_DIR / f"{props['folder']}.cpp").read_text()
    assert props["id"] == ID and f'"{ID}"' in cpp and f'"{NAME}"' in cpp
    assert len(props["id"]) <= 15
