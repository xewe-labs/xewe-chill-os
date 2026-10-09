// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/Argb/Argb.cpp

#include "Argb.h"

#include <ArduinoJson.h>
#include <FastLED.h>
#include <soc/soc_caps.h>

#include <algorithm>
#include <cmath>
#include <utility>


namespace {

// FastLED takes the data pin as a template argument. Instantiate one WS2812B driver for every
// pin FastLED accepts on this chip (if constexpr skips the invalid ones, which would static_assert)
// and pick it at run time. Drivers are function-local statics, so adding the same pin again
// reuses its driver.
template <uint8_t PIN>
CLEDController* attach_driver(uint8_t pin, CRGB* data, int count) {
    if constexpr (PIN < SOC_GPIO_PIN_COUNT) {
        if constexpr (FastPin<PIN>::validpin()) {
            if (pin == PIN) return &FastLED.addLeds<WS2812B, PIN, GRB>(data, count);
        }
        return attach_driver<PIN + 1>(pin, data, count);
    } else {
        (void)pin; (void)data; (void)count;
        return nullptr;
    }
}

bool pin_supported(uint8_t pin) {
    return pin < 64 && ((_FL_VALID_PIN_MASK >> pin) & 1ULL);
}

} // namespace


Argb::Argb(xewe::Os& host, ArgbConfig config)
      : xewe::Module(host,
               /* id                  */ "argb",
               /* name                */ "ARGB",
               /* description         */ "WS2812B LED strips (FastLED) with on/off state and colour glide",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
      , config(std::move(config))
{
    register_command({
        "add",
        "Add a WS2812B strip with the default length: <pin>",
        "$argb add 6",
        1,
        [this](xewe::span<const std::string> args){ add_cmd(args); }
    });

    register_command({
        "remove",
        "Remove a strip by its pin: <pin>",
        "$argb remove 6",
        1,
        [this](xewe::span<const std::string> args){ remove_cmd(args); }
    });

    register_command({
        "set_length",
        "Set the number of LEDs of a strip (1-300): <pin> <length>",
        "$argb set_length 6 12",
        2,
        [this](xewe::span<const std::string> args){ set_length_cmd(args); }
    });

    register_command({
        "set_state",
        "Turn a strip on or off: <pin> <1|0>",
        "$argb set_state 6 1",
        2,
        [this](xewe::span<const std::string> args){ set_state_cmd(args); }
    });

    register_command({
        "set_rgb",
        "Set a strip's colour: <pin> <r> <g> <b>",
        "$argb set_rgb 6 255 0 0",
        4,
        [this](xewe::span<const std::string> args){ set_rgb_cmd(args); }
    });

    register_command({
        "print_json",
        "Print the strip data as JSON",
        "$argb print_json",
        0,
        [this](xewe::span<const std::string>){ os.serial.print(get_json()); }
    });
}

Argb::~Argb() {
    clear_strips();
}

void Argb::begin_routines_required() {
    load();
    last_frame_ms = millis();
}

void Argb::loop() {
    if (is_disabled()) return;

    // frame-rate independent exponential glide towards the target colour
    const uint32_t now = millis();
    float dt = (now - last_frame_ms) / 1000.0f;
    last_frame_ms = now;
    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f;   // no visual jump after a stall

    for (auto* s : strips) {
        if (!s->transitioning) continue;
        const float tr = s->cfg.state ? s->cfg.r : 0;
        const float tg = s->cfg.state ? s->cfg.g : 0;
        const float tb = s->cfg.state ? s->cfg.b : 0;
        const float dr = tr - s->current_r, dg = tg - s->current_g, db = tb - s->current_b;

        if (std::fabs(dr) < 0.5f && std::fabs(dg) < 0.5f && std::fabs(db) < 0.5f) {
            s->current_r = tr; s->current_g = tg; s->current_b = tb;
            s->transitioning = false;
        } else {
            // k > 1 overshoots (k > 2 diverges) and a negative float -> uint8_t cast is undefined:
            // clamp, and a NaN/negative glide_speed jumps straight to the target
            float k = config.glide_speed * dt;
            if (!(k > 0.0f) || k > 1.0f) k = 1.0f;
            s->current_r += dr * k; s->current_g += dg * k; s->current_b += db * k;
        }
        show(s);
    }
}

void Argb::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    clear_strips();
    // wipes the "argb" namespace: the next boot sets up the Config.h default strips again
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string Argb::status(const bool verbose) const {
    std::string s = Module::status(false);
    if (strips.empty()) {
        s += "\nNo LED strips configured";
    } else {
        for (const auto* l : strips) {
            s += "\n  Pin " + std::to_string(l->cfg.pin) + " (" + std::to_string(l->cfg.length) + " LEDs), " +
                 (l->cfg.state ? "on" : "off") + ", RGB (" + std::to_string(l->cfg.r) + ", " +
                 std::to_string(l->cfg.g) + ", " + std::to_string(l->cfg.b) + ")" +
                 (l->transitioning ? " [transitioning]" : "");
        }
    }
    if (verbose) os.serial.print(s);
    return s;
}

std::string Argb::get_json() const {
    if (is_disabled() || strips.empty()) return "[]";
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto* l : strips) {
        JsonObject o = arr.add<JsonObject>();
        o["pin"]    = l->cfg.pin;
        o["length"] = l->cfg.length;
        o["state"]  = l->cfg.state;
        o["r"]      = l->cfg.r;
        o["g"]      = l->cfg.g;
        o["b"]      = l->cfg.b;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---- API ----------------------------------------------------------------------------------------

bool Argb::add(uint8_t pin, uint16_t length) {
    if (is_disabled() || get_strip(pin) || !pin_supported(pin) || strips.size() >= max_strips()) return false;
    ArgbStrip cfg;
    cfg.pin    = pin;
    cfg.length = length ? length : config.default_length;
    if (cfg.length > MAX_LENGTH) return false;
    if (!create_strip(cfg)) return false;
    save();
    return true;
}

bool Argb::remove(uint8_t pin) {
    if (is_disabled()) return false;
    auto it = std::find_if(strips.begin(), strips.end(), [pin](StripData* s) { return s->cfg.pin == pin; });
    if (it == strips.end()) return false;
    free_strip(*it);
    strips.erase(it);
    save();
    return true;
}

bool Argb::set_length(uint8_t pin, uint16_t length) {
    if (is_disabled() || length == 0 || length > MAX_LENGTH) return false;
    StripData* s = get_strip(pin);
    if (!s) return false;
    ArgbStrip cfg = s->cfg;
    cfg.length = length;
    auto it = std::find(strips.begin(), strips.end(), s);
    free_strip(s);
    strips.erase(it);
    StripData* n = create_strip(cfg);
    save();
    return n != nullptr;
}

bool Argb::set_state(uint8_t pin, bool state, bool persist) {
    if (is_disabled()) return false;
    StripData* s = get_strip(pin);
    if (!s) return false;
    if (s->cfg.state != state) {
        s->cfg.state = state;
        s->transitioning = true;
        if (persist) save();
    }
    return true;
}

bool Argb::set_rgb(uint8_t pin, uint8_t r, uint8_t g, uint8_t b, bool persist) {
    if (is_disabled()) return false;
    StripData* s = get_strip(pin);
    if (!s) return false;
    if (s->cfg.r != r || s->cfg.g != g || s->cfg.b != b) {
        s->cfg.r = r; s->cfg.g = g; s->cfg.b = b;
        s->transitioning = true;
        if (persist) save();
    }
    return true;
}

bool Argb::set_all_state(bool state, bool persist) {
    if (is_disabled()) return false;
    bool changed = false;
    for (auto* s : strips) {
        if (s->cfg.state != state) {
            s->cfg.state = state;
            s->transitioning = true;
            changed = true;
        }
    }
    if (changed && persist) save();
    return true;
}

bool Argb::set_all_rgb(uint8_t r, uint8_t g, uint8_t b, bool persist) {
    if (is_disabled()) return false;
    bool changed = false;
    for (auto* s : strips) {
        if (s->cfg.r != r || s->cfg.g != g || s->cfg.b != b) {
            s->cfg.r = r; s->cfg.g = g; s->cfg.b = b;
            s->transitioning = true;
            changed = true;
        }
    }
    if (changed && persist) save();
    return true;
}

// ---- internals ----------------------------------------------------------------------------------

size_t Argb::max_strips() const {
    // one RMT TX channel per strip: 2 on C3/C6, 4 on S3
    return SOC_RMT_TX_CANDIDATES_PER_GROUP;
}

Argb::StripData* Argb::get_strip(uint8_t pin) const {
    for (auto* s : strips) if (s->cfg.pin == pin) return s;
    return nullptr;
}

Argb::StripData* Argb::create_strip(const ArgbStrip& cfg) {
    if (!pin_supported(cfg.pin) || cfg.length == 0 || cfg.length > MAX_LENGTH) return nullptr;
    auto* s   = new StripData();
    s->cfg    = cfg;
    s->pixels = new CRGB[cfg.length];
    s->driver = attach_driver<0>(cfg.pin, s->pixels, cfg.length);
    if (!s->driver) {
        delete[] s->pixels;
        delete s;
        return nullptr;
    }
    s->current_r = cfg.state ? cfg.r : 0.0f;
    s->current_g = cfg.state ? cfg.g : 0.0f;
    s->current_b = cfg.state ? cfg.b : 0.0f;
    strips.push_back(s);
    show(s);
    return s;
}

void Argb::free_strip(StripData* s) {
    if (s->driver) {
        fill_solid(s->pixels, s->cfg.length, CRGB::Black);
        s->driver->showLeds(255);
        // the driver stays registered with FastLED (it cannot be removed); detach its buffer
        s->driver->setLeds(nullptr, 0);
    }
    delete[] s->pixels;
    delete s;
}

void Argb::clear_strips() {
    for (auto* s : strips) free_strip(s);
    strips.clear();
}

void Argb::show(StripData* s) {
    if (!s->driver) return;
    fill_solid(s->pixels, s->cfg.length, CRGB(static_cast<uint8_t>(s->current_r),
                                             static_cast<uint8_t>(s->current_g),
                                             static_cast<uint8_t>(s->current_b)));
    s->driver->showLeds(255);
}

void Argb::load() {
    clear_strips();

    ArgbStore store;
    if (os.nvs.read_flex(id, "data", store) && store.schema == ArgbStore::SCHEMA) {
        for (const auto& c : store.strips) {
            if (strips.size() >= max_strips() || get_strip(c.pin)) continue;
            create_strip(c);
        }
        return;
    }

    // first boot (or unreadable settings): the project's default strips, then store them
    for (uint8_t pin : config.default_pins) {
        if (pin == NO_PIN || get_strip(pin) || strips.size() >= max_strips()) continue;
        ArgbStrip c;
        c.pin    = pin;
        c.length = config.default_length;
        create_strip(c);
    }
    save();
}

void Argb::save() {
    ArgbStore store;
    for (const auto* s : strips) store.strips.push_back(s->cfg);
    os.nvs.write_flex(id, "data", store);
}

// ---- commands -----------------------------------------------------------------------------------

void Argb::add_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pin;
    if (xewe::str::parse_int(args[0], pin) && add(pin)) os.serial.print("LED strip added.");
    else                                                 os.serial.print("Failed to add LED strip.");
}

void Argb::remove_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pin;
    if (xewe::str::parse_int(args[0], pin) && remove(pin)) os.serial.print("LED strip removed.");
    else                                                    os.serial.print("Failed to remove LED strip.");
}

void Argb::set_length_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pin;
    uint16_t len;
    if (xewe::str::parse_int(args[0], pin) && xewe::str::parse_int(args[1], len) && set_length(pin, len))
        os.serial.print("Length updated.");
    else
        os.serial.print("Failed to update length.");
}

void Argb::set_state_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pin, state;
    if (xewe::str::parse_int(args[0], pin) && xewe::str::parse_int(args[1], state) && state <= 1 && set_state(pin, state == 1))
        os.serial.print("State updated.");
    else
        os.serial.print("Failed to update LED state.");
}

void Argb::set_rgb_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pin, r, g, b;
    if (xewe::str::parse_int(args[0], pin) && xewe::str::parse_int(args[1], r) &&
        xewe::str::parse_int(args[2], g) && xewe::str::parse_int(args[3], b) && set_rgb(pin, r, g, b))
        os.serial.print("RGB updated.");
    else
        os.serial.print("Failed to update RGB.");
}
