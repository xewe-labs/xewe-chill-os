# Local sources

`./setup.sh` fetches the refs in `xewe.toml`. To build against working trees instead, set the
source variables:

```sh
XEWE_TOOLS_SOURCE=../xewe-os-tools XEWE_CORE_SOURCE=../xewe-os-core \
XEWE_MODULES_SOURCE=../xewe-os-modules ./setup.sh </dev/null
```

`--core-source DIR` and `--modules-source DIR` are the flag forms of the last two. The core is
copied into `build/libraries/XeWeCore/`, so rerun `./setup.sh` after each core edit. The
modules source is used where it is. `XEWE_HOME` moves the shared toolchain from
`~/.xewe-os/build-tools/`.

## Harness

Compile and test core and module changes in a copy of the xewe-os template, called a harness.
Set the copy up with the local sources above and select the modules under test. Never edit the
tools or a reference clone to make a change work.

This firmware is checked with its own selection: run `./setup.sh </dev/null`, then
`xewe build --chip c3`.
