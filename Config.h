// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-laptop-cooling-pad/Config.h
//
// Build settings. `xewe build` generates <XeWeBuildInfo.h> (PROJECT_NAME, BUILD_VERSION,
// BUILD_TIMESTAMP, BUILD_CHIP and every `--define KEY=VALUE`); it wins over the defaults below.
// The version lives in xewe.lock [project]; do not edit it here.
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

// ---------------------------------------------------------------------------------------------
// Cooling pad wiring defaults. Applied once, on the first boot of each pad module (when it has no
// stored settings); after that the pins live in NVS and change with the modules' commands
// ($fan add/remove, $mlx90614 set_pins/set_addr, $argb add/remove/set_length).
// The defaults are the original ESP32-C3 pad (also valid on the C6). GPIO 0 and 3 are strapping
// pins on the S3, so an S3 build gets free, non-strapping GPIOs below. Any PAD_* can be overridden
// with `xewe build --define PAD_X=N`; README.md "Pins per chip" has the exact lines.
// (`xewe test` builds without --define, which is why the S3 set is a default here, not only a define.)
// 255 means "not connected".
// ---------------------------------------------------------------------------------------------
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#ifndef PAD_FAN1_PWM
#define PAD_FAN1_PWM 4
#endif
#ifndef PAD_FAN1_TACH
#define PAD_FAN1_TACH 5
#endif
#ifndef PAD_FAN2_PWM
#define PAD_FAN2_PWM 6
#endif
#ifndef PAD_FAN2_TACH
#define PAD_FAN2_TACH 7
#endif
#ifndef PAD_MLX_SDA
#define PAD_MLX_SDA 8
#endif
#ifndef PAD_MLX_SCL
#define PAD_MLX_SCL 9
#endif
#ifndef PAD_ARGB1_PIN
#define PAD_ARGB1_PIN 10
#endif
#ifndef PAD_ARGB2_PIN
#define PAD_ARGB2_PIN 11
#endif
#endif // CONFIG_IDF_TARGET_ESP32S3

#ifndef PAD_FAN1_PWM
#define PAD_FAN1_PWM 3
#endif
#ifndef PAD_FAN1_TACH
#define PAD_FAN1_TACH 0
#endif
#ifndef PAD_FAN2_PWM
#define PAD_FAN2_PWM 10
#endif
#ifndef PAD_FAN2_TACH
#define PAD_FAN2_TACH 1
#endif

#ifndef PAD_MLX_SDA
#define PAD_MLX_SDA 4
#endif
#ifndef PAD_MLX_SCL
#define PAD_MLX_SCL 5
#endif
#ifndef PAD_MLX_ADDR
#define PAD_MLX_ADDR 0x5A
#endif

#ifndef PAD_ARGB1_PIN
#define PAD_ARGB1_PIN 6
#endif
#ifndef PAD_ARGB2_PIN
#define PAD_ARGB2_PIN 7
#endif
#ifndef PAD_ARGB_LEN
#define PAD_ARGB_LEN 12
#endif
