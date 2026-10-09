// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/Fan/Fan.cpp

#include "Fan.h"

#include <ArduinoJson.h>
#include <driver/gpio.h>

#include <algorithm>
#include <utility>

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif


void IRAM_ATTR Fan::tach_isr_handler(void* arg) {
    if (auto* fan = static_cast<FanData*>(arg)) fan->pulse_count = fan->pulse_count + 1;
}

Fan::Fan(xewe::Os& host, FanConfig config)
      : xewe::Module(host,
               /* id                  */ "fan",
               /* name                */ "Fan",
               /* description         */ "Drives 4-wire PWM fans (25 kHz) and reads their tachometers",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
      , config(std::move(config))
{
    register_command({
        "add",
        "Add a fan without a tachometer: <pwm_pin>",
        "$fan add 9",
        1,
        [this](xewe::span<const std::string> args){ add_cmd(args); }
    });

    register_command({
        "add_w_tach",
        "Add a fan with a tachometer: <pwm_pin> <tach_pin>",
        "$fan add_w_tach 9 10",
        2,
        [this](xewe::span<const std::string> args){ add_w_tach_cmd(args); }
    });

    register_command({
        "set",
        "Set the speed of one fan (0-255): <pwm_pin> <speed>",
        "$fan set 9 255",
        2,
        [this](xewe::span<const std::string> args){ set_cmd(args); }
    });

    register_command({
        "set_all",
        "Set the speed of every fan (0-255): <speed>",
        "$fan set_all 255",
        1,
        [this](xewe::span<const std::string> args){ set_all_cmd(args); }
    });

    register_command({
        "remove",
        "Remove a fan by its PWM pin: <pwm_pin>",
        "$fan remove 9",
        1,
        [this](xewe::span<const std::string> args){ remove_cmd(args); }
    });

    register_command({
        "print_json",
        "Print the fan data as JSON",
        "$fan print_json",
        0,
        [this](xewe::span<const std::string>){ os.serial.print(get_json()); }
    });
}

Fan::~Fan() {
    clear_fans();
}

void Fan::begin_routines_required() {
    load();
}

void Fan::loop() {
    if (is_disabled()) return;
    const uint32_t now = millis();

    for (auto* f : fans) {
        if (!f->has_tach) continue;

        const uint32_t dt = now - f->last_calc_time;
        if (dt < 1000) continue;

        noInterrupts();
        const uint32_t pulses = f->pulse_count;
        f->pulse_count = 0;
        interrupts();

        // 2 pulses per revolution: rpm = pulses / 2 * 60000 / dt (64-bit: pulses * 30000 wraps
        // 32 bits above 143 165 pulses, e.g. a floating or ringing tach line after a loop stall)
        const uint64_t rpm64       = (static_cast<uint64_t>(pulses) * 30000u) / dt;
        const uint32_t current_rpm = rpm64 > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(rpm64);
        f->last_calc_time = now;

        if (current_rpm > config.absolute_max_rpm) continue;   // noise spike

        f->raw_history[f->history_idx] = current_rpm;
        f->history_idx = (f->history_idx + 1) % 3;
        if (f->history_count < 3) f->history_count++;

        uint32_t median_rpm = current_rpm;
        if (f->history_count == 3) {
            const uint32_t a = f->raw_history[0], b = f->raw_history[1], c = f->raw_history[2];
            median_rpm = std::max(std::min(a, b), std::min(std::max(a, b), c));
        }

        f->ema_rpm = (f->ema_rpm == 0 && median_rpm > 0)
                   ? median_rpm
                   : static_cast<uint32_t>((config.ema_alpha * median_rpm) + ((1.0f - config.ema_alpha) * f->ema_rpm));

        const uint32_t r = config.ui_rounding;
        f->displayed_rpm = r ? ((f->ema_rpm + r / 2) / r) * r : f->ema_rpm;
    }
}

void Fan::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    clear_fans();
    // wipes the "fan" namespace: the next boot sets up the Config.h default fans again
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string Fan::status(const bool verbose) const {
    std::string s = Module::status(false);
    if (fans.empty()) {
        s += "\nNo fans configured";
    } else {
        for (const auto* f : fans) {
            s += "\n  PWM pin " + std::to_string(f->pin_pwm) + ", speed " + std::to_string(f->speed);
            if (f->has_tach) {
                s += ", tach pin " + std::to_string(f->pin_tach) + ", " + std::to_string(f->displayed_rpm) + " RPM";
            } else {
                s += ", no tach";
            }
        }
    }
    if (verbose) os.serial.print(s);
    return s;
}

std::string Fan::get_json() const {
    JsonDocument doc;
    JsonArray json_fans = doc["fans"].to<JsonArray>();

    for (const auto* f : fans) {
        JsonObject o = json_fans.add<JsonObject>();
        o["pin_pwm"]  = f->pin_pwm;
        o["has_tach"] = f->has_tach;
        // the web UI plots 0-100 %
        o["speed"]    = static_cast<uint8_t>((static_cast<uint16_t>(f->speed) * 100) / 255);
        if (f->has_tach) {
            o["pin_tach"]      = f->pin_tach;
            o["displayed_rpm"] = f->displayed_rpm;
            o["ema_rpm"]       = f->ema_rpm;
        }
    }

    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---- API ----------------------------------------------------------------------------------------

uint32_t Fan::get_rpm(uint8_t pwm_pin) const {
    const FanData* f = get_fan(pwm_pin);
    return (f && f->has_tach) ? f->displayed_rpm : 0;
}

bool Fan::add(uint8_t pwm_pin, uint8_t tach_pin) {
    if (is_disabled() || get_fan(pwm_pin)) return false;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pwm_pin)) return false;
    if (tach_pin != NO_PIN && (tach_pin == pwm_pin || !GPIO_IS_VALID_GPIO(tach_pin))) return false;
    // a pin already used by another fan (as PWM or tach) would re-route its LEDC output or replace
    // its tach interrupt
    for (const auto* f : fans) {
        if (f->has_tach && f->pin_tach == pwm_pin) return false;
        if (tach_pin != NO_PIN && (f->pin_pwm == tach_pin || (f->has_tach && f->pin_tach == tach_pin))) return false;
    }
    if (!create_fan(pwm_pin, tach_pin, 0)) return false;
    save();
    return true;
}

bool Fan::remove(uint8_t pwm_pin) {
    if (is_disabled()) return false;
    auto it = std::find_if(fans.begin(), fans.end(), [pwm_pin](FanData* f) { return f->pin_pwm == pwm_pin; });
    if (it == fans.end()) return false;

    free_fan(*it);
    fans.erase(it);
    save();
    return true;
}

bool Fan::set(uint8_t pwm_pin, uint8_t speed, bool persist) {
    if (is_disabled()) return false;
    FanData* f = get_fan(pwm_pin);
    if (!f) return false;
    f->speed = speed;
    ledcWrite(f->pin_pwm, speed);
    if (persist) save();
    return true;
}

bool Fan::set_all(uint8_t speed, bool persist) {
    if (is_disabled() || fans.empty()) return false;
    bool changed = false;
    for (auto* f : fans) {
        if (f->speed != speed) changed = true;
        f->speed = speed;
        ledcWrite(f->pin_pwm, speed);
    }
    if (persist && changed) save();
    return true;
}

// ---- internals ----------------------------------------------------------------------------------

Fan::FanData* Fan::get_fan(uint8_t pwm_pin) const {
    for (auto* f : fans) if (f->pin_pwm == pwm_pin) return f;
    return nullptr;
}

void Fan::free_fan(FanData* f) {
    if (f->has_tach) detachInterrupt(digitalPinToInterrupt(f->pin_tach));
    ledcWrite(f->pin_pwm, 0);
    ledcDetach(f->pin_pwm);
    delete f;
}

void Fan::clear_fans() {
    for (auto* f : fans) free_fan(f);
    fans.clear();
}

Fan::FanData* Fan::create_fan(uint8_t pwm, uint8_t tach, uint8_t speed) {
    if (!ledcAttach(pwm, PWM_FREQ, PWM_RES)) return nullptr;

    auto* f           = new FanData();
    f->pin_pwm        = pwm;
    f->pin_tach       = tach;
    f->has_tach       = (tach != NO_PIN);
    f->speed          = speed;
    f->last_calc_time = millis();

    if (f->has_tach) {
        pinMode(tach, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(tach), tach_isr_handler, f, FALLING);
    }
    ledcWrite(pwm, speed);
    fans.push_back(f);
    return f;
}

void Fan::load() {
    clear_fans();

    FanStore store;
    if (os.nvs.read_flex(id, "data", store) && store.schema == FanStore::SCHEMA) {
        for (const auto& e : store.fans) create_fan(e.pwm, e.tach, e.speed);
        return;
    }

    // first boot (or unreadable settings): the project's default fans, then store them
    for (const auto& d : config.defaults) {
        if (d.pwm == NO_PIN || get_fan(d.pwm) || !GPIO_IS_VALID_OUTPUT_GPIO(d.pwm)) continue;
        create_fan(d.pwm, d.tach, 0);
    }
    save();
}

void Fan::save() {
    FanStore store;
    for (const auto* f : fans) {
        FanEntry e;
        e.pwm   = f->pin_pwm;
        e.tach  = f->has_tach ? f->pin_tach : NO_PIN;
        e.speed = f->speed;
        store.fans.push_back(e);
    }
    os.nvs.write_flex(id, "data", store);
}

// ---- commands -----------------------------------------------------------------------------------

void Fan::add_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pwm;
    if (xewe::str::parse_int(args[0], pwm) && add(pwm)) os.serial.print("Fan added.");
    else                                                 os.serial.print("Failed to add fan.");
}

void Fan::add_w_tach_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pwm, tach;
    if (xewe::str::parse_int(args[0], pwm) && xewe::str::parse_int(args[1], tach) && tach != NO_PIN && add(pwm, tach))
        os.serial.print("Fan with tach added.");
    else
        os.serial.print("Failed to add fan.");
}

void Fan::set_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pwm, speed;
    if (xewe::str::parse_int(args[0], pwm) && xewe::str::parse_int(args[1], speed) && set(pwm, speed))
        os.serial.print("Speed updated.");
    else
        os.serial.print("Failed to set fan speed.");
}

void Fan::set_all_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t speed;
    if (xewe::str::parse_int(args[0], speed) && set_all(speed)) os.serial.print("All fans updated.");
    else                                                         os.serial.print("Failed to set fans.");
}

void Fan::remove_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t pwm;
    if (xewe::str::parse_int(args[0], pwm) && remove(pwm)) os.serial.print("Fan removed.");
    else                                                    os.serial.print("Failed to remove fan.");
}
