"""MLX90614 module tests (project-local module, src/Mlx90614/). Run from the project: `python -m xewe test`.

Hardware preconditions: a provisioned board. The address test needs nothing wired and restores the
address it found. The bare-bus scan moves the I2C bus to XEWE_TEST_MLX_SDA / XEWE_TEST_MLX_SCL (free
GPIOs with nothing attached) and moves it back afterwards. The temperature test needs a real
MLX90614 on the configured pins.
"""
import os
import re
from pathlib import Path

import pytest

ID = "mlx90614"
NAME = "MLX90614"      # module.properties name == C++ name argument
MODULE_DIR = Path(__file__).resolve().parents[1] / "src" / "Mlx90614"


def _current(serial):
    """(address, sda, scl) from `$mlx90614 status`; sda/scl None when not configured."""
    serial.send("$mlx90614 status")
    addr = None
    sda = scl = None
    m = serial.expect(r"Pins: (?:SDA (\d+), SCL (\d+)|not configured)", timeout=5)
    if m[1] is not None:
        sda, scl = m[1], m[2]
    addr = serial.expect(r"Address: (0x[0-9A-F]{2})", timeout=5)[1]
    return addr, sda, scl


def test_compiles(compiled):
    assert compiled.is_file()


def test_status(serial):
    serial.command(f"${ID} status", expect=rf"{NAME} module (enabled|disabled)", timeout=5)


def test_set_addr_validation(serial):
    addr, _, _ = _current(serial)
    # 0x00 and anything above 0x7F are rejected; so is a non-hex token
    for bad in ("0x00", "0x80", "0xFF", "zz"):
        serial.command(f"$mlx90614 set_addr {bad}", expect=r"Invalid I2C address provided", timeout=10)
    serial.command("$mlx90614 set_addr 0x5B", expect=r"Target address updated to 0x5B and saved to NVS\.", timeout=10)
    serial.command(f"$mlx90614 set_addr {addr}", expect=rf"Target address updated to {addr} ", timeout=10)


def test_scan_bare_bus(serial):
    sda = os.environ.get("XEWE_TEST_MLX_SDA")
    scl = os.environ.get("XEWE_TEST_MLX_SCL")
    if not (sda and scl):
        pytest.skip("needs XEWE_TEST_MLX_SDA and XEWE_TEST_MLX_SCL (free GPIOs, nothing attached)")
    _, old_sda, old_scl = _current(serial)
    try:
        serial.command(f"$mlx90614 set_pins {sda} {scl}", expect=rf"Pins updated to SDA={sda} SCL={scl}\.", timeout=15)
        serial.expect(r"Warning: bus started, but no valid data read from the sensor\.", timeout=15)
        # without pull-ups every probe can run into the I2C timeout: allow a slow scan
        serial.command("$mlx90614 scan", expect=r"No I2C devices found", timeout=60)
        serial.command("$mlx90614 status", expect=r"Online: no", timeout=5)
        serial.command("$mlx90614 status", expect=r"Object: 255\.0 C \(error value while offline\)", timeout=5)
    finally:
        if old_sda is not None:
            serial.command(f"$mlx90614 set_pins {old_sda} {old_scl}", expect=r"Pins updated to", timeout=15)


def test_set_pins_rejects_same_pin(serial):
    serial.command("$mlx90614 set_pins 4 4", expect=r"Invalid pins\.", timeout=5)


def test_read_temperature(serial):
    if not os.environ.get("XEWE_TEST_MLX_PRESENT"):
        pytest.skip("requires hardware: an MLX90614 on the configured pins (set XEWE_TEST_MLX_PRESENT=1)")
    m = serial.command("$mlx90614 read", expect=r"Object: (-?\d+\.\d+) C, ambient: (-?\d+\.\d+) C", timeout=10)
    assert -40.0 < float(m[1]) < 125.0 and -40.0 < float(m[2]) < 125.0


@pytest.mark.host
def test_properties_match_source():
    props = dict(l.split("=", 1) for l in (MODULE_DIR / "module.properties").read_text().splitlines() if "=" in l)
    cpp = (MODULE_DIR / f"{props['folder']}.cpp").read_text()
    assert props["id"] == ID and f'"{ID}"' in cpp and f'"{NAME}"' in cpp
    assert re.fullmatch(r"[a-z][a-z0-9_]{0,14}", props["id"])
