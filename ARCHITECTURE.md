# Architecture

Why XeWe OS is shaped the way it is. The [README](README.md) says how to use it; this file says why
it is four repositories, why the sketch has exactly one include, why modules carry an explicit
declare line, and what is decided versus still open. Written 2026-10-08, at the end of phase 1.

## 1. Purpose and philosophy

- **A template you clone and vibecode.** `xewe-os` is not a firmware you configure; it is a
  starting point you own. `git clone` (or GitHub "Use this template"), `./setup.sh`, `./run.sh`,
  then write your device in `xewe-os.ino` and `Config.h`. The repository holds only your code and
  `xewe.lock`; everything else is fetched at pinned refs, so a project stays small and builds the
  same way later.
- **Modules are code figured out once.** WiFi reconnects, NTP and timezones, debounced buttons, an
  HTTP command endpoint: each took effort to get right on an ESP32. A module captures that once, in
  one repo, behind one contract, so the next project (or the next agent) selects it instead of
  solving it again.
- **Agents are first-class users.** Every action is a non-interactive command with stable output
  and exit codes (`xewe build`, `xewe test`, `xewe modules validate`). Rules that break silently
  are written down in each repo's `AGENTS.md` and enforced by validators where possible. A human
  and an agent run the same commands.
- **Hardware in the loop, and still useful with no board.** Tests flash a real board and talk to
  it over serial. Without a board every command still compiles, then reports
  `compiled, not run: no board attached (...)` and exits 0. Both paths are first class (D6, D22).

Target hardware is ESP32-C3, -C6 and -S3 only (D1), flashed over serial only (no OTA, D2), built
with arduino-cli (D5).

## 2. The four repos and their boundaries

| Repo | Job | Boundary |
|---|---|---|
| [`xewe-os`](https://github.com/xewe-labs/xewe-os) | the firmware template users clone | holds only its own code, `xewe.lock`, two thin scripts and released firmware |
| [`xewe-os-core`](https://github.com/xewe-labs/xewe-os-core) | `XeWeCore`, the one Arduino library | reusable code every firmware needs; publishable on its own; depends only on ArduinoJson |
| [`xewe-os-modules`](https://github.com/xewe-labs/xewe-os-modules) | every module, `modules/<slug>/` | features some firmware needs; no toolchain of its own; built and tested through an `xewe-os` checkout (D21) |
| [`xewe-os-tools`](https://github.com/xewe-labs/xewe-os-tools) | `xewe`, one Python package | everything that runs on the host: setup, build, flash, serial, test, release |

[`publish-arduino-library`](https://github.com/xewe-labs/publish-arduino-library) stays a separate,
generic tool that checks and releases `XeWeCore` (D18).

```
                        xewe-os  (template)
                        xewe.lock pins one ref each
             ┌──────────────────┼──────────────────┐
             v                  v                  v
      xewe-os-core       xewe-os-modules      xewe-os-tools
      (XeWeCore lib) <── each module has      (xewe: setup, build,
             ^           requires_core         flash, serial, test)
             │                                     │
  publish-arduino-library                fetches core, modules, ArduinoJson,
  (check + release XeWeCore)             arduino-cli and the esp32 core
                                         into the project's build/
```

A project after `./setup.sh --modules wifi,time`:

```
my-fw/
├── xewe-os.ino              committed   #include <XeWeCore.h>; XeWeOs os({...}); your setup()/loop()
├── Config.h                 committed   your defaults; includes the generated <XeWeBuildInfo.h>
├── xewe.lock                committed   [project] [core] [modules] [tools] [libraries]
├── setup.sh  run.sh         committed   bootstrap build/.venv, then call `python -m xewe`
├── static/firmware/releases/  committed  written by `xewe release`
├── src/modules/             generated   Wifi/  Time/  Modules.h (includes + declare lines)  modules.lock
└── build/                   generated   .venv/  bin/arduino-cli  arduino15/ (esp32 core)
                                         libraries/{XeWeCore,ArduinoJson}/  xewe-os-modules/
                                         gen/<chip>/XeWeBuildInfo/  out/<chip>/*.bin
```

Nothing generated is committed and there are no submodules (D11). `build/` and `src/modules/` can
be deleted at any time; `./setup.sh` rebuilds them from `xewe.lock`.

## 3. XeWeCore shape

One library (D8), utils inside it (D16), laid out as a facade plus standalone components (D17):

```
src/
  XeWeCore.h                 umbrella: the only top-level header
  XeWeCore/
    XeWeOs.h/.cpp            xewe::Os (global alias XeWeOs) and xewe::System
    Module.h/.cpp            xewe::Module, the base class of every module
    Serial.h/.cpp            xewe::SerialPort
    Cli.h/.cpp               xewe::Cli: "$group command args", $help
    Nvs.h/.cpp/.tpp          xewe::Nvs: typed key-value storage
    FlexData.h               xewe::FlexData
    Utils.h + Utils/*.h      header-only: String, AsyncTimer, Span, Color, LockGuard, ...
```

```cpp
#include <XeWeCore.h>
XeWeOs os({.project_name = "my-device", .version = "0.1.0"});
void setup() { os.begin(); os.serial.println("hi"); }
void loop()  { os.loop(); }   // os.cli, os.nvs, os.system.restart()
```

The facade owns `os.serial`, `os.cli`, `os.nvs` and `os.system`. The components also work alone
(`xewe::SerialPort`, `xewe::Cli`, `xewe::Nvs`, `xewe::FlexData`, `xewe::str`) with no `XeWeOs`.
Everything is in `namespace xewe`; the include direction is Utils ← Serial ← Cli, FlexData ← Nvs,
all ← Module ← XeWeOs.

**Why `#include <XeWeCore.h>` is the only sketch include.** arduino-cli discovers which library to
compile by trying the sketch's includes against each library's top-level `src/` headers. A sketch
whose only include is `<XeWeCore/XeWeOs.h>` fails with "No such file": the sub-header is not a
top-level header, so the library is never added. This was verified with arduino-cli 1.5.1 during
the merge, and it amended the D17 sketch line. Sub-headers may be included *after* the umbrella.

Three names are shaped by the Arduino core's macros: the serial type is `xewe::SerialPort` (the
core defines `Serial` as a macro), the member `os.cli` is brace-initialised and never followed by
`(` (the core defines `cli()`), and a standalone `xewe::Cli` object is conventionally `xewe_cli`.

## 4. Module contract

The full contract is
[`xewe-os-modules/CONTRACT.md`](https://github.com/xewe-labs/xewe-os-modules/blob/main/CONTRACT.md).
In short: a module is `modules/<slug>/` with `module.properties`, `src/<Folder>/<Folder>.{h,cpp}`,
`tests/test_<slug>.py` and a README. `id` (at most 15 characters) is both the CLI group and the NVS
namespace and never changes. The class derives from `xewe::Module`, lives in the global namespace,
includes `<XeWeCore.h>`, names its constructor's Os parameter `host` (a parameter named `os` would
hide the member), captures `[this]` only, and never writes `cli(`. Three tests are required:
`test_compiles`, `test_status` and one behaviour test. `tools/validate.py` must pass.

**Why the explicit declare line was kept (D13).** `declare=Time time_module(os, wifi);` is copied
verbatim into the generated `Modules.h`. The alternative, generating the declaration from
constructor signatures, would need a C++ parser in the tools and would hide the one line that
says how a module is wired. Explicit lines are readable in the generated file, let a module choose
its variable name and dependency arguments, and are cheap to check: the validator enforces unique
slug, id, folder, class and variable, that the type is the folder, that the first argument is `os`,
and that every other argument is a declared dependency. Validation replaced human review.

## 5. Tools

`xewe` (from `xewe-os-tools`, called as `build/.venv/bin/python -m xewe` or through `./run.sh`):

| Command | Does |
|---|---|
| `setup` | arduino-cli, esp32 core, XeWeCore, libraries and modules into `build/`; generates `src/modules/` |
| `build [--chip C \| --all-chips] [--define K=V]` | compile into `build/out/<chip>/`, prints flash % |
| `flash`, `run`, `serial` | write the merged image at 0x0; build-flash-listen; timestamped console |
| `test [--module SLUG] [--host-only] [--all-chips]` | pytest over project and module tests |
| `boards`, `doctor`, `clean` | board discovery; environment check; delete generated output |
| `modules list\|select\|validate\|generate`, `lock show\|update` | modules and pins |
| `release --version X.Y.Z` | release matrix into `static/firmware/releases/`; prints git/gh commands |

**`xewe.lock`** is the project's whole dependency state: `[project]` (name, version, chip),
`[core]`, `[modules]` (with `selected`) and `[tools]` (repo + ref each) and `[libraries]`
(ArduinoJson). Only `lock update`, `modules select`, `setup --modules` and `release` write it;
`setup --latest` tries newer tags without editing it. The firmware version is
`[project] version`; builds never change it.

**No-board semantics (D22).** `flash`, `run` and `test` compile first, then report
`compiled, not run: no board attached (...)` and exit 0. `--require-board` turns that into exit 4
(1 for `test`). Agents grep for the line instead of treating it as an error.

**Test layers (D14).** Host-native C++ tests for pure logic run with an Arduino shim and no board
(`xewe-os-core/extras/host/`). Host-driven Python tests (`xewe test`) flash the firmware and assert
on serial output; without a board they report "compiled, not run", except `test_compiles`, which
really builds. Hardware tests on a provisioned board are the same Python tests with a board
attached. There are no on-device unit-test frameworks.

**One board at a time, compile cost (D15).** A build costs about a minute per chip, so `build` and
`test` compile only the selected chip; the c3/c6/s3 matrix runs with `--all-chips`. First setup
downloads ~1.7 GB and installs ~6 GB into `build/`; `--arduino-data DIR` reuses an installed core.
The tools never read or write `~/.arduino15` or `~/Arduino`.

## 6. Versioning and release policy (D12)

| Repo | Version | Tag | Released by |
|---|---|---|---|
| `xewe-os-core` | semver in `library.properties` | `X.Y.Z` (un-prefixed, Arduino registry rule) | `publish-arduino-library` check + release |
| `xewe-os-modules` | repo-wide | `vX.Y.Z` | tag; each module declares `requires_core=>=2.0.0,<3.0.0` |
| `xewe-os-tools` | `pyproject.toml` | `vX.Y.Z` | tag |
| `xewe-os` | `xewe.lock [project] version`, independent | `vX.Y.Z` (optional) | `xewe release`, binaries committed under `static/firmware/releases/` |

The template pins, in `xewe.lock`: core `2.0.1`, modules `v0.2.0`, tools `v0.1.1`, ArduinoJson
`v7.4.3`; the template itself is `2.0.0`. A new core or modules release reaches a project only when
its lock moves (`xewe lock update`).

## 7. Decisions log

Copied verbatim from the tracker (`priorities.md`, the canonical record; "above" in D17 refers to
its "XeWeCore shape" section, section 3 here). The one amendment since: the documented sketch
include is the umbrella `<XeWeCore.h>` (section 3).

| # | Decision | Notes |
|---|---|---|
| D1 | Target hardware is ESP32 **C3, C6, S3** only | Drop the 8-architecture claims in library metadata |
| D2 | No OTA | Keep `no_ota` partitions; flashing is serial only |
| D3 | Template UX is `git clone` → `./setup.sh` → `./run.sh` | Agentic layer comes later, not now |
| D4 | Modules live in **one repo** (`xewe-os-modules`) | The URL registry becomes the actual modules repo |
| D5 | Stay on **arduino-cli** | PlatformIO is not clearly better: C6 Arduino support only exists in the community pioarduino fork, and the test runner is custom code either way |
| D6 | Testing is hardware-in-the-loop with an attached board, and compile-only without one | Both paths are first class |
| D7 | Migration order: core → xewe-os template (tests + flashing) → consumers | Cooling pad before xewe-led-os as proving ground |
| D8 | **One library: `XeWeCore`** = utils + serial + cli + nvs + os | Resolves the `xewe-os` / `XeWeOS` name collision. ArduinoJson (header-only, the standard Arduino JSON library) is its one dependency |
| D9 | "Core" not "Kernel" | Kernel implies scheduling/memory management, which this does not do |
| D10 | **One tooling repo: `xewe-os-tools`** (setup + build + flash + serial + test) | Single Python package, shared port discovery and serial code |
| D11 | Template gets dependencies via **`setup.sh` + `xewe.lock`** | Pinned tags for core, modules, tools; `--latest` overrides; nothing generated is committed; no submodules |
| D12 | Versioning: core is semver and published; modules repo tagged as a whole, each module declares `requires_core`; template pins both in `xewe.lock` | Template version is independent |
| D13 | Module contract keeps the explicit `declare` line in `module.properties` | Plus an automated validator for uniqueness of slug, id, folder, variable name (replaces human review) |
| D14 | Tests are **Python, host-driven**: flash, send CLI commands over serial, assert on output | No on-device Unity tests. Host-native tests (Arduino shim, no board) for pure logic such as FlexData, CLI parser, utils |
| D15 | One board attached at a time; compile per board costs ~1 min | Tester compiles only the attached/selected chip by default; the full C3/C6/S3 matrix runs only on an explicit flag |
| D16 | Utils is **inside** `XeWeCore` | Header-only; everything depends on it |
| D17 | `XeWeCore` layout: **`XeWeOs` facade + standalone component headers** | See "XeWeCore shape" above. `ModuleController` + `System` → `XeWeOs`; `os.serial`, `os.cli`, `os.nvs`, `os.system` |
| D18 | `publish-arduino-library` **stays a separate repo** | Generic Arduino-library publishing tool; not folded into xewe-os-tools |
| D19 | Repo names are **prefixed**: `xewe-os-core`, `xewe-os-modules`, `xewe-os-tools`, `xewe-os` | Everything reads as part of xewe-os |
| D20 | `xewe-os` is a **GitHub template repository** and also plain-cloneable | "Use this template" gives fresh history; clone keeps working |
| D21 | Modules are built and tested **through a `xewe-os` checkout** as the harness | The modules repo carries no toolchain; CI clones `xewe-os` at the lock ref with the module selected. One build path, nothing to drift |
| D22 | **No board in phase 1.** Hardware tests are written but run compile-only; runner reports "compiled, not run" and exits 0 | Board tests executed by the user in step 5 |
| D23 | **Autonomous run.** Opus agents code, main session verifies and steers; mid-level issues resolved and logged, only critical issues interrupt | Policy detail in phase1-plan.md |

Open and deferred:

| Item | State |
|---|---|
| CI (compile matrix on push) | on hold by user decision; builds and tests are local through `xewe-os-tools` |
| laptop-cooling-pad migration | on hold; first real consumer once the backbone is published |
| xewe-led-os migration | out of scope for now; after the backbone is proven |
| Frontend builder v2 | later: prototype UIs in Flask/FastAPI under constraints that port to a web-interface module |
| Web flasher hosting | later: move the ESP Web Tools flasher into the org, fed by `static/firmware/releases/` |
| OTA | out of scope (D2) |
| Windows | deferred; the tools keep paths portable |
| Hardware test run | the 12 serial tests were written but never run on a board (phase 1 had none) |

## 8. Migration history

1. Before: five Arduino libraries at 1.0.0 (`xewe-library-utils`, `-serial`, `-cli`, `-nvs`, `-os`, the last named `XeWeOS`).
2. Six module repositories (`xewe-os-module-wifi`, `-web-interface`, `-time`, `-scheduler`, `-buttons`, `-pins`) listed in a URL registry.
3. A shared build toolchain (`xewe-os-build-toolchain`) with per-OS shell scripts, plus a hand-copied `validate.sh` in each module repo.
4. The framework existed in three diverged copies: the libraries, `xewe-led-os`, and the laptop-cooling-pad firmware.
5. Was → now: the five libraries merged into one `XeWeCore` 2.0.0; `ModuleController` + `System` became the `XeWeOs` facade; `xewe::os` became `xewe`; behaviour and NVS keys unchanged.
6. The six module repositories moved into `xewe-os-modules` 0.2.0 under one contract, validator and tests.
7. The toolchain and the `validate.sh` copies became the `xewe` Python package; the build counter became `[project] version`.
8. `xewe-os` became a template that fetches the rest from `xewe.lock`.
9. Step 5 (the user's): publish the four repos, tag, archive the replaced ones, run the hardware tests.
10. Next: after the cooling pad proves the path, xewe-led-os drops its copy of the framework and becomes a project built from this template, moving reusable LED code into modules.
