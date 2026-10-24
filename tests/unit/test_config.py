"""Source checks of Config.h, whose shape the tools rely on when they append module blocks."""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]  # the project: tests/unit/ -> tests/ -> root
START = re.compile(r"^/\* ---- start ([a-z0-9-]+) ---- \*/$")
SECTIONS = (
    "/* ---- start default runtime values ---- */",
    "/* ---- end default runtime values ---- */",
    "/* ---- start compile time defined constants ---- */",
    "/* ---- end compile time defined constants ---- */",
)
SENTINEL = re.compile(r"^\s*#\s*define\s+\w+\s+INJECTED_FROM_DOTENV", re.MULTILINE)
# a product override: a define, or the per-chip guard around one, as the generated blocks write it
OVERRIDE = re.compile(
    r"^#(define XEWE_[A-Z0-9_]+ \S.*|(el)?if defined\(XEWE_CHIP_[A-Z0-9]+\)|else|endif)$"
)


def _text():
    return (ROOT / "Config.h").read_text()


def _bar(marker):
    return "/*" + "*" * (len(marker) - 4) + "*/"


def _blocks(lines):
    """(slug, first line, last line) of every block, from its opening bar to its closing bar."""
    blocks = []
    for i, line in enumerate(lines):
        match = START.match(line)
        if match:
            slug = match.group(1)
            blocks.append((slug, i - 1, lines.index(f"/* ---- end {slug} ---- */") + 1))
    return blocks


@pytest.mark.unit
def test_pragma_once_and_no_include():
    lines = _text().splitlines()
    assert "#pragma once" in lines
    assert not [l for l in lines if re.match(r"\s*#\s*include\b", l)]


@pytest.mark.unit
def test_core_block_first_and_blocks_framed():
    lines = _text().splitlines()
    blocks = _blocks(lines)
    assert blocks and blocks[0][0] == "core"
    assert len({slug for slug, _, _ in blocks}) == len(blocks)
    for slug, first, last in blocks:
        start, end = f"/* ---- start {slug} ---- */", f"/* ---- end {slug} ---- */"
        block = lines[first : last + 1]
        assert block[:3] == [_bar(start), start, _bar(start)]
        assert block[-3:] == [_bar(end), end, _bar(end)]
        assert [l for l in block if l in SECTIONS] == list(SECTIONS)


@pytest.mark.unit
def test_every_generated_line_is_a_comment():
    lines = _text().splitlines()
    for _, first, last in _blocks(lines):
        for line in lines[first : last + 1]:
            assert (
                not line.strip()
                or (line.startswith("/*") and line.endswith("*/"))
                or line.startswith("// ")
                or OVERRIDE.match(line)
            ), line


@pytest.mark.unit
def test_nothing_active_but_pragma_once_and_overrides():
    directives = [l for l in _text().splitlines() if re.match(r"\s*#", l)]
    assert directives[0] == "#pragma once"
    assert all(OVERRIDE.match(l) for l in directives[1:]), directives


@pytest.mark.unit
def test_no_prompt_build_macro_or_sentinel():
    text = _text()
    # only a module's own Config.h defines <NAME>_PROMPT
    assert "_PROMPT" not in text
    # build macros come from the generated XeWeBuildInfo.h, never from this file
    assert not re.search(r"\b(BUILD|PROJECT)_[A-Z0-9_]+", text)
    # a release image takes no value from a dotenv file
    assert not SENTINEL.search(text)
