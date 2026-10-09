// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-laptop-cooling-pad/src/PadWeb/Glide.h
//
// The strip colour glide of the pad (ported from the old Argb module), as pure functions: no Arduino.
// PadWeb.cpp calls these; tests/unit/test_glide.cpp builds them with the host g++.
#pragma once

#include <cmath>
#include <cstdint>

namespace pad_fx {

struct Colour {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

constexpr float MAX_DT_S = 0.1f;   // no visual jump after a loop stall
constexpr float SNAP     = 0.5f;   // closer than this on every channel: take the target

// Fraction of the remaining distance covered in one step: glide_speed (1/s) * dt (s), clamped to
// (0, 1]. k > 1 overshoots (k > 2 diverges) and a negative float -> uint8_t cast is undefined, so a
// NaN, zero or negative product jumps straight to the target (R1 clamp fix).
inline float glide_factor(float glide_speed, float dt_s) {
    if (dt_s > MAX_DT_S) dt_s = MAX_DT_S;
    float k = glide_speed * dt_s;
    if (!(k > 0.0f) || k > 1.0f) k = 1.0f;
    return k;
}

// One frame-rate independent exponential step towards `target`; true once it has arrived.
inline bool glide_step(Colour& current, const Colour& target, float k) {
    const float dr = target.r - current.r, dg = target.g - current.g, db = target.b - current.b;
    if (std::fabs(dr) < SNAP && std::fabs(dg) < SNAP && std::fabs(db) < SNAP) {
        current = target;
        return true;
    }
    current.r += dr * k;
    current.g += dg * k;
    current.b += db * k;
    return false;
}

// 0..255 with rounding; anything outside (or NaN) is clamped, never cast undefined.
inline uint8_t to_byte(float v) {
    if (!(v > 0.0f)) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

} // namespace pad_fx
