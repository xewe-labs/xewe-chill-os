// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-laptop-cooling-pad/Config.h
//
// Build settings. `xewe build` generates <XeWeBuildInfo.h> (PROJECT_NAME, BUILD_VERSION,
// BUILD_TIMESTAMP, BUILD_CHIP and every `--define KEY=VALUE`); it wins over the defaults below.
// The version lives in xewe.toml [project]; do not edit it here.
#pragma once

// Unconditional on purpose: arduino-cli only adds a library to the build when an #include of it
// fails, so a __has_include() guard would silently drop the generated header. Plain Arduino IDE
// (no xewe): delete this line; the defaults below then apply.
#include <XeWeBuildInfo.h>

#ifndef PROJECT_NAME
#define PROJECT_NAME "xewe-os-laptop-cooling-pad"
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION "0.0.0"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

#ifndef BUILD_CHIP
#define BUILD_CHIP "unknown"
#endif

// Printed in the boot header. Override: xewe build --define 'PROJECT_URL="https://..."'
#ifndef PROJECT_URL
#define PROJECT_URL "https://github.com/xewe-labs/xewe-os-laptop-cooling-pad"
#endif

// Serial console baud rate (the tools' `xewe serial` defaults to 115200 as well).
#ifndef SERIAL_BAUD_RATE
#define SERIAL_BAUD_RATE 115200
#endif

// Cooling pad wiring: the pin defaults are build defines of the modules, not of this file (a module
// is compiled as a library and never sees Config.h). Their per-chip defaults are the pad's wiring;
// override with `xewe build --define KEY=VALUE` (README "Pins per chip"):
//   fan:      FAN1_PWM FAN1_TACH FAN2_PWM FAN2_TACH   (C3/C6 3 0 10 1, S3 4 5 6 7)
//   mlx90614: MLX90614_SDA MLX90614_SCL MLX90614_ADDR (C3/C6 4 5, S3 8 9; 0x5A)
//   led:      LED_PIN_DATA LED_COUNT                  (the pad: --define LED_PIN_DATA=6 --define LED_COUNT=12)
// They apply on a module's first boot; afterwards the settings live in NVS and change with commands.
