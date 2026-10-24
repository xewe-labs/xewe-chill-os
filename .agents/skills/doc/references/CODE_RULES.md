# Code rules

## Sketch and Config.h

`xewe-chill-os.ino` includes `"Config.h"` first and `<XeWeCore.h>` second, which is the
umbrella and the only library include. It declares `XeWeOs os({...})` before any module.
`src/Modules.h` comes after that, then the project-local modules in dependency order:
`Fan fan(os);`, `Mlx90614 mlx90614(os);`, `PadWeb pad_web(os, fan, mlx90614, led);`.
`setup()` calls `pad_web.attach(web_interface)` before `os.begin()`.

The project URL is the `.url` field on the `XeWeOs os({...})` line.

`Config.h` includes nothing and sets no build macros. `xewe build` puts the generated
`XeWeBuildInfo.h`, then `Config.h`, then the generated `XeWeOverrides.h` in front of every
source file with `-include`. `Config.h` holds the core block and one block per selected
module, each framed by start and end lines. Every generated line is a comment, so uncomment a
`#define` to override a value. The only active lines are the pad's wiring overrides, such as
the strip's data pin per chip, and `tests/unit/test_config.py` fails on any other. First-boot
lines stay commented out, and no `INJECTED_FROM_DOTENV` line is active, because the release
script refuses one. The format is specified in `doc/CONFIG.md` of xewe-os-core.

The version is `[project] version` in `xewe.toml`. Never write it as a literal in code.

## Project-local modules

`src/Fan/`, `src/Mlx90614/` and `src/PadWeb/` must compile against the current core with no
warnings.

A project-local module keeps its values in `src/<Folder>/Config.h`, in the module form of the
core's `doc/CONFIG.md`: two sections, `#ifndef` groups, `/* */` comments, and `_PROMPT` only on
a value asked at first boot. Names are `XEWE_<ID>_*` with the module id in upper case:
`XEWE_FAN_*`, `XEWE_MLX90614_*` and `XEWE_PAD_*` (PadWeb's id is `pad`). The user edits the
file in place, and `xewe modules generate` never touches it.

The module contract applies to these modules too: the constructor parameter is named `host`,
lambdas capture `[this]`, nothing calls `cli(`, and ids are at most 15 characters. It lives in
`doc/CONTRACT.md` of xewe-os-modules.

## The strip

`PadWeb` drives the strip only through the led module's `sync_state`, `sync_mode` and
`sync_color`. The led module stores each of them in NVS, so send one only on a change and rate
limited by `XEWE_PAD_COLOR_MIN_MS`, never once per frame.

## Comments and docs

A comment says what the code does or why. It never names an agent, a session, a date, a
decision or finding id, a run, or the code's past. Source files keep their SPDX and path
header lines.

A change to the first-boot flow, the commands, the wiring or the layout updates `README.md` and
the matching page in `doc/` in the same change.

The xewe-labs organization guidelines at `https://github.com/xewe-labs/.github` (`AGENTS.md`
and `guidelines/`) apply where this skill is silent. Where the two differ, the guidelines win
and this file is corrected.
