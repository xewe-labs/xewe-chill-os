// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/Mlx90614/Config.h
/*
The values of Mlx90614 (id mlx90614, prefix XEWE_MLX90614_), in the module form of XeWeCore's
doc/config.md. A --define or a line in the project's Config.h wins. The pin defaults are the
cooling pad's C3/C6 wiring; an S3 build sets its own pins in the project's Config.h.
*/
#pragma once

/* ---- start default runtime values ---- */

/* I2C address of the sensor (mlx90614/addr), 1 to 127 */
#ifndef XEWE_MLX90614_ADDR
#define XEWE_MLX90614_ADDR 0x5A
#endif

/* I2C SDA GPIO (mlx90614/sda), 0 to 255, 255 = none (S3 wiring: 8) */
#ifndef XEWE_MLX90614_SDA
#define XEWE_MLX90614_SDA 4
#endif

/* I2C SCL GPIO (mlx90614/scl), 0 to 255, 255 = none (S3 wiring: 9) */
#ifndef XEWE_MLX90614_SCL
#define XEWE_MLX90614_SCL 5
#endif

/* ---- end default runtime values ---- */

/* ---- start compile time defined constants ---- */

/* Listener slots of the temperature fan-out (xewe::ListenerSet, no heap) */
#ifndef XEWE_MLX90614_LISTENERS_MAX
#define XEWE_MLX90614_LISTENERS_MAX 4
#endif

/* Poll period while the sensor answers, ms */
#ifndef XEWE_MLX90614_POLL_MS
#define XEWE_MLX90614_POLL_MS 500
#endif

/* Poll period while no sensor answers, ms */
#ifndef XEWE_MLX90614_OFFLINE_POLL_MS
#define XEWE_MLX90614_OFFLINE_POLL_MS 5000
#endif

/* Time budget of `$mlx90614 scan`, ms */
#ifndef XEWE_MLX90614_SCAN_BUDGET_MS
#define XEWE_MLX90614_SCAN_BUDGET_MS 1500
#endif

/* Wire timeout per probe during `$mlx90614 scan`, ms */
#ifndef XEWE_MLX90614_SCAN_PROBE_MS
#define XEWE_MLX90614_SCAN_PROBE_MS 10
#endif

/* ---- end compile time defined constants ---- */
