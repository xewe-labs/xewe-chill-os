# AGENTS.md: xewe-os (firmware template)

Organization rules ([`.github/AGENTS.md`](https://github.com/xewe-labs/.github/blob/main/AGENTS.md))
win where this file is silent. Never run git write commands (commit, tag, push, init) or publish
unless the user asks.

## Running things

From the project root, after `./setup.sh`, always as `build/.venv/bin/python -m xewe ...`
(the bare `xewe` script breaks when the project is moved; re-run `./setup.sh` then).

- Inspect first: `xewe build --chip c3 --dry-run` prints the arduino-cli command
  (works before the toolchain is installed once `build/.venv` exists; in a fresh project run
  `./setup.sh` first).
- Build and test: `xewe build [--chip C | --all-chips]`, `xewe test [--host-only] [--module SLUG]`.
- Modules: `xewe modules list`, `xewe modules validate [PATH]`, `xewe modules select LIST|all|none`
  (edits `xewe.lock`: only when asked). Zero modules is a valid project.
- Also safe: `xewe doctor`, `xewe lock show`, `xewe boards --no-probe`.
- No board is normal (D22): `flash`, `run` and `test` exit 0 with
  `compiled, not run: no board attached (...)`; grep for that, it is not an error.
- Do not pass `--require-board` or `--erase`, or flash a board, unless the user asks.
- Never run arduino-cli directly and never touch `~/.arduino15` or `~/Arduino`: the tools use an
  isolated environment under `build/`.
- `--define` string values carry their own quotes: `--define 'PROJECT_URL="https://..."'`.

## Never commit

`build/`, `src/modules/` (with `Modules.h` and `modules.lock`), venvs, editor files. The repo
holds only `xewe-os.ino`, `Config.h`, `xewe.lock`, the scripts, docs and
`static/firmware/releases/` (written by `xewe release`).

## Code rules

- The sketch includes `<XeWeCore.h>` (the umbrella). Including only `XeWeCore/<Part>.h` does not
  build: arduino-cli discovers libraries from top-level headers only.
- `Config.h` includes `<XeWeBuildInfo.h>` unconditionally. Never wrap it in `__has_include`.
- `XeWeOs os({...})` is declared before any module; `src/modules/Modules.h` is included after it.
- Modules live in the modules repo (its `CONTRACT.md` and `AGENTS.md`): constructor Os parameter
  named `host` (member stays `os`), handlers capture `[this]` only, never write `cli(` (macro).
- Never edit `src/modules/` or `build/` by hand; they are regenerated.
- The version is `[project] version` in `xewe.lock`, never a literal in code.
