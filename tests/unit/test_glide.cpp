// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-laptop-cooling-pad/tests/unit/test_glide.cpp
//
// Unit test of src/PadWeb/Glide.h (no Arduino): built and run by tests/unit/test_pad.py with
// g++ -std=c++17 -Wall -Wextra -Werror. One line per check, then "PASSED|FAILED: N check(s), F failure(s)".
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "../../src/PadWeb/Glide.h"

using namespace pad_fx;

static int failures = 0;
static int checks   = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        ++checks;                                                                  \
        if (cond) { std::printf("ok   %s\n", #cond); }                             \
        else      { std::printf("FAIL %s (line %d)\n", #cond, __LINE__); ++failures; } \
    } while (0)

int main() {
    const float nan = std::numeric_limits<float>::quiet_NaN();

    // ---- factor: clamped to (0, 1] ----
    CHECK(glide_factor(8.0f, 0.02f) > 0.159f && glide_factor(8.0f, 0.02f) < 0.161f);
    CHECK(glide_factor(8.0f, 0.5f) == glide_factor(8.0f, MAX_DT_S));      // stall capped at 0.1 s
    CHECK(glide_factor(20.0f, 0.1f) == 1.0f);                             // k = 2 would diverge: clamped
    CHECK(glide_factor(100.0f, 0.1f) == 1.0f);
    CHECK(glide_factor(-3.0f, 0.02f) == 1.0f);                            // negative: jump
    CHECK(glide_factor(nan, 0.02f) == 1.0f);                              // NaN: jump
    CHECK(glide_factor(8.0f, 0.0f) == 1.0f);                              // no time passed: jump (never 0)

    // ---- steps ----
    {
        Colour c{0, 0, 0};
        const Colour t{255, 0, 128};
        bool arrived = false;
        int steps = 0;
        float last_r = -1.0f;
        bool monotonic = true;
        while (!arrived && steps < 1000) {
            arrived = glide_step(c, t, glide_factor(8.0f, 0.02f));
            if (c.r < last_r) monotonic = false;
            last_r = c.r;
            ++steps;
        }
        CHECK(arrived && steps < 100);                                    // converges in ~0.8 s at 50 fps
        CHECK(monotonic);                                                 // never overshoots
        CHECK(c.r == 255.0f && c.g == 0.0f && c.b == 128.0f);             // snapped exactly
    }
    {
        Colour c{10, 10, 10};
        CHECK(glide_step(c, Colour{10.3f, 9.8f, 10.0f}, 0.5f));           // within SNAP: arrives
        Colour d{0, 0, 0};
        CHECK(!glide_step(d, Colour{200, 100, 50}, 1.0f) && d.r == 200.0f && d.b == 50.0f);   // k = 1: one jump
        CHECK(glide_step(d, Colour{200, 100, 50}, 1.0f));
    }

    // ---- bytes ----
    CHECK(to_byte(0.0f) == 0 && to_byte(-5.0f) == 0 && to_byte(nan) == 0);
    CHECK(to_byte(254.6f) == 255 && to_byte(300.0f) == 255);
    CHECK(to_byte(127.4f) == 127 && to_byte(127.5f) == 128);

    std::printf("%s: %d check(s), %d failure(s)\n", failures ? "FAILED" : "PASSED", checks, failures);
    return failures ? 1 : 0;
}
