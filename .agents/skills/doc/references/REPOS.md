# Repos

XeWe OS is four repositories under github.com/xewe-labs, and this firmware is a product made from the `xewe-os` template. `xewe.toml` holds the refs it uses.

## Where a change goes

| Change | Repo |
|---|---|
| The sketch, `Config.h`, `xewe.toml`, `src/Fan/`, `src/Mlx90614/`, `src/PadWeb/`, the tests, `ci.yml` | `xewe-chill-os` (this repo) |
| `setup.sh`, `scripts/release.sh` and the two sample modules, which every product copies | `xewe-os`, the template |
| Code every firmware needs, the `XeWeOs` facade, `xewe::Module`, Cli, Nvs, Settings, Utils | `xewe-os-core`, the one Arduino library `XeWeCore` |
| A feature only some firmware needs | `xewe-os-modules`, as `modules/<slug>/` |
| Anything that runs on the host (setup, build, flash, serial, test, release, format, brand-lint, webui) and the reusable CI workflows | `xewe-os-tools`, one Python package in `src/xewe/` |

A module that only one product needs stays in that product's `src/` (D28). The core depends only on ArduinoJson and must stay publishable on its own.

## Boundaries

- The modules repo has no toolchain of its own. A module is built and tested through an `xewe-os` checkout (D21).
- The core's CI runs host tests, the examples and `arduino-lint` against its `doc/LIBRARY_RULES.md`, and the core is released by its own `scripts/release.sh` (D18).
- The tools carry their own spec in `doc/spec.md` and the webui exporter in `doc/webui.md`. The spec is the authority for the `build/` layout (its section 6).
- The contract is in `doc/CONTRACT.md` of the modules repo and the module index in its `doc/modules.md`.
- Every repo has `README.md` and `LICENSE.txt` at the root, long pages in `doc/`, the agent guide in `.agents/` and CI in `.github/workflows/` (D25).
- Nothing generated is committed and there are no submodules (D11). `build/`, `src/Modules.h` and `run.sh` can be deleted at any time and `./setup.sh` rebuilds them from `xewe.toml`.

`doc/XEWE_OS_ECOSYSTEM.md` covers what `./setup.sh` installs and the committed versus generated split. This repo's `static/` is output of `xewe release` and nothing in it is tracked.
