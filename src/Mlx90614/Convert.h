// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/Mlx90614/Convert.h
// No Arduino or Wire here: tests/unit/test_convert.cpp builds this file with the host g++.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

namespace mlx90614_fx {

constexpr float KELVIN_STEP = 0.02f; // one LSB of a RAM temperature register, K
constexpr float KELVIN_ZERO = 273.15f;
constexpr uint8_t DEFAULT_ADDR = 0x5A;

// Bit 15 is the error flag: 0xFFFF from a glitched bus would read as 1037.5 °C.
inline bool raw_to_celsius(uint8_t lsb, uint8_t msb, float& out) {
    if (msb & 0x80)
        return false;
    const uint16_t raw = static_cast<uint16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
    out = static_cast<float>(raw) * KELVIN_STEP - KELVIN_ZERO;
    return true;
}

// 0x00 is the general call.
inline bool valid_address(unsigned value) {
    return value >= 0x01 && value <= 0x7F;
}

inline bool parse_address(const std::string& text, uint8_t& out) {
    if (text.empty() || text.size() > 4)
        return false;
    char* end = nullptr;
    const unsigned long v = std::strtoul(text.c_str(), &end, 16);
    if (end == text.c_str() || *end != '\0' || text[0] == '-' || text[0] == '+' || text[0] == ' ')
        return false;
    if (!valid_address(static_cast<unsigned>(v)))
        return false;
    out = static_cast<uint8_t>(v);
    return true;
}

// A bare bus would take tens of seconds, so a scan stops after this many errors in a row.
constexpr uint8_t MAX_BUS_ERRORS_IN_A_ROW = 3;

// Wire::endTransmission() results: 0 ack, 2 nack (no device), 4 bus error, 5 timeout.
inline uint8_t scan_error_streak(uint8_t streak, uint8_t result) {
    return (result == 0 || result == 2 || result == 3) ? 0 : static_cast<uint8_t>(streak + 1);
}

} // namespace mlx90614_fx
