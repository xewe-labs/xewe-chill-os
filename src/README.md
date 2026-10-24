# src/

This folder holds the pad's own modules, one folder per module, each an `xewe::Module` declared in `xewe-chill-os.ino`. `./setup.sh` also writes the generated, git-ignored `src/Modules.h`, which declares the wifi, web-interface and led modules from xewe-os-modules.

- [Fan/](Fan/README.md) drives the fans and holds the temperature curve.
- [Mlx90614/](Mlx90614/README.md) reads the sensor.
- [PadWeb/](PadWeb/README.md) links the sensor to the curve, colours the strip and serves the pad page.

Each module keeps its build-time values in its own `Config.h`, prefixed `XEWE_<ID>_`. [doc/WRITING_A_MODULE.md](../doc/WRITING_A_MODULE.md) explains the contract.

```sh
build/tools/.venv/bin/python -m xewe build --chip c3
```
