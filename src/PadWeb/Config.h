// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/PadWeb/Config.h
/*
The values of PadWeb (id pad, prefix XEWE_PAD_), in the module form of XeWeCore's doc/config.md.
A --define or a line in the project's Config.h wins.
*/
#pragma once

/* ---- start default runtime values ---- */

/* ---- end default runtime values ---- */

/* ---- start compile time defined constants ---- */

/* Period of the strip check against the curve, ms */
#ifndef XEWE_PAD_UPDATE_MS
#define XEWE_PAD_UPDATE_MS 250
#endif

/* Shortest time between two strip changes (each colour change is an NVS write in the led module),
 * ms */
#ifndef XEWE_PAD_COLOR_MIN_MS
#define XEWE_PAD_COLOR_MIN_MS 2000
#endif

/* Smallest per-channel colour difference worth a strip change, 0 to 255 */
#ifndef XEWE_PAD_COLOR_STEP
#define XEWE_PAD_COLOR_STEP 8
#endif

/* ---- end compile time defined constants ---- */
