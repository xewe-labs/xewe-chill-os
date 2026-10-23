// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/Fan/Config.h
/*
The values of Fan (id fan, prefix XEWE_FAN_), in the module form of XeWeCore's doc/config.md.
A --define or a line in the project's Config.h wins. The pin defaults are the cooling pad's
C3/C6 wiring; an S3 build sets its own pins in the project's Config.h.
*/
#pragma once

/* ---- start default runtime values ---- */

/* Curve -> fans period (fan/curve_ms), ms, 100 to 60000 */
#ifndef XEWE_FAN_CURVE_MS
#define XEWE_FAN_CURVE_MS 1000
#endif

/* No temperature for this long counts as offline (fan/stale_ms), ms, 1000 to 600000 */
#ifndef XEWE_FAN_STALE_MS
#define XEWE_FAN_STALE_MS 10000
#endif

/* ---- end default runtime values ---- */

/* ---- start compile time defined constants ---- */

/* First fan PWM GPIO, set up on the first boot, 255 = none (S3 wiring: 4) */
#ifndef XEWE_FAN_FAN1_PWM
#define XEWE_FAN_FAN1_PWM 3
#endif

/* First fan tachometer GPIO, 255 = none (S3 wiring: 5) */
#ifndef XEWE_FAN_FAN1_TACH
#define XEWE_FAN_FAN1_TACH 0
#endif

/* Second fan PWM GPIO, set up on the first boot, 255 = none (S3 wiring: 6) */
#ifndef XEWE_FAN_FAN2_PWM
#define XEWE_FAN_FAN2_PWM 10
#endif

/* Second fan tachometer GPIO, 255 = none (S3 wiring: 7) */
#ifndef XEWE_FAN_FAN2_TACH
#define XEWE_FAN_FAN2_TACH 1
#endif

/* RPM smoothing factor (EMA alpha), percent, 1 to 100 */
#ifndef XEWE_FAN_EMA_PCT
#define XEWE_FAN_EMA_PCT 30
#endif

/* Readings above this are noise, RPM */
#ifndef XEWE_FAN_MAX_RPM
#define XEWE_FAN_MAX_RPM 10000
#endif

/* Displayed RPM rounding step, RPM; 0 = none */
#ifndef XEWE_FAN_RPM_ROUNDING
#define XEWE_FAN_RPM_ROUNDING 10
#endif

/* ---- end compile time defined constants ---- */
