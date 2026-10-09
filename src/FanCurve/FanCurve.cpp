// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/FanCurve/FanCurve.cpp

#include "FanCurve.h"

#include <ArduinoJson.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>


namespace {
bool by_temp(const CurvePoint& a, const CurvePoint& b) { return a.temp < b.temp; }
} // namespace


FanCurve::FanCurve(xewe::Os& host, Fan& fan_ref, Mlx90614& mlx90614_ref, Argb& argb_ref, FanCurveConfig config)
      : xewe::Module(host,
               /* id                  */ "fan_curve",
               /* name                */ "Fan Curve",
               /* description         */ "Sets the fan speed from the sensor temperature along a curve; LEDs follow",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
      , fan(fan_ref)
      , mlx90614(mlx90614_ref)
      , argb(argb_ref)
      , config(std::move(config))
{
    add_requirement(fan);
    add_requirement(mlx90614);
    // argb is optional: used while enabled, never a requirement (disabling the LEDs must not stop the fans)

    register_command({
        "add_point",
        "Add or replace a curve point: <temp_C> <speed_0-100>",
        "$fan_curve add_point 40.5 50",
        2,
        [this](xewe::span<const std::string> args){ add_point_cmd(args); }
    });

    register_command({
        "remove_point",
        "Remove the curve point at a temperature: <temp_C>",
        "$fan_curve remove_point 40.5",
        1,
        [this](xewe::span<const std::string> args){ remove_point_cmd(args); }
    });

    register_command({
        "set_cold",
        "Set the LED colour at the first point: <#RRGGBB|RRGGBB>",
        "$fan_curve set_cold #00FFFF",
        1,
        [this](xewe::span<const std::string> args){ set_cold_cmd(args); }
    });

    register_command({
        "set_hot",
        "Set the LED colour at the last point: <#RRGGBB|RRGGBB>",
        "$fan_curve set_hot #FF0000",
        1,
        [this](xewe::span<const std::string> args){ set_hot_cmd(args); }
    });

    register_command({
        "print_json",
        "Print the curve and colours as JSON",
        "$fan_curve print_json",
        0,
        [this](xewe::span<const std::string>){ os.serial.print(get_json()); }
    });
}

void FanCurve::begin_routines_required() {
    FanCurveStore stored;
    if (!os.nvs.read_flex(id, "data", stored)) {
        apply_defaults();                                   // first boot: store the defaults
        save();
    } else if (!curve_math::schema_ok(stored.schema)) {
        // another firmware's layout: never reinterpret it, never overwrite it at boot
        os.serial.print("Fan Curve: " + curve_math::schema_message(stored.schema));
        apply_defaults();
    } else {
        std::sort(stored.points.begin(), stored.points.end(), by_temp);
        const auto err = curve_math::validate_points(stored.points);
        if (err == curve_math::CurveError::ok) {
            data = std::move(stored);
        } else {
            os.serial.print(std::string("Fan Curve: stored curve rejected (") + curve_math::error_text(err) +
                            "); using and storing the default curve");
            apply_defaults();
            save();
        }
    }
    last_update_ms = millis();
}

void FanCurve::loop() {
    if (is_disabled() || data.points.empty()) return;
    const uint32_t now = millis();
    if (now - last_update_ms < config.update_interval_ms) return;
    last_update_ms = now;

    // sensor offline -> error temperature (255 C) -> last point -> fans at the top speed
    const float temp = mlx90614.get_temp();
    last_speed_pct = get_target_speed(temp);
    fan.set_all(curve_math::speed_to_pwm(last_speed_pct), false);
    if (argb.is_enabled()) update_leds(temp);
}

void FanCurve::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    apply_defaults();
    // wipes the "fan_curve" namespace: the next boot stores the default curve again
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string FanCurve::status(const bool verbose) const {
    std::string s = Module::status(false);
    if (data.points.empty()) {
        s += "\n  No curve points";
    } else {
        char buf[96];
        for (const auto& p : data.points) {
            snprintf(buf, sizeof(buf), "\n  %.2f C -> %u %% (PWM %u)", p.temp, p.speed,
                     static_cast<unsigned>(curve_math::speed_to_pwm(p.speed)));
            s += buf;
        }
    }
    s += "\n  Cold colour: " + data.cold_color + "\n  Hot colour: " + data.hot_color;
    s += "\n  Current target: " + std::to_string(last_speed_pct) + " %";
    if (verbose) os.serial.print(s);
    return s;
}

std::string FanCurve::get_json() const {
    JsonDocument doc;
    JsonObject colors = doc["curve_edge_colors"].to<JsonObject>();
    colors["start"] = data.cold_color;
    colors["end"]   = data.hot_color;
    JsonArray arr = doc["temp_curve"].to<JsonArray>();
    for (const auto& p : data.points) {
        JsonObject o = arr.add<JsonObject>();
        o["temp"]  = p.temp;
        o["speed"] = p.speed;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---- API ----------------------------------------------------------------------------------------

uint8_t FanCurve::get_target_speed(float temp) const {
    return curve_math::target_speed(data.points, temp);
}

bool FanCurve::add_point(float temp, uint8_t speed_pct) {
    if (is_disabled() || !curve_math::temp_ok(temp)) return false;
    if (speed_pct > 100) speed_pct = 100;
    auto& pts = data.points;
    pts.erase(std::remove_if(pts.begin(), pts.end(), [temp](const CurvePoint& p) {
        return std::fabs(p.temp - temp) < curve_math::SAME_TEMP;
    }), pts.end());
    if (pts.size() >= MAX_POINTS) return false;
    CurvePoint p;
    p.temp  = temp;
    p.speed = speed_pct;
    pts.push_back(p);
    std::sort(pts.begin(), pts.end(), by_temp);
    save();
    return true;
}

bool FanCurve::remove_point(float temp) {
    if (is_disabled()) return false;
    auto& pts = data.points;
    auto it = std::remove_if(pts.begin(), pts.end(), [temp](const CurvePoint& p) {
        return std::fabs(p.temp - temp) < curve_math::SAME_TEMP;
    });
    if (it == pts.end()) return false;
    pts.erase(it, pts.end());
    save();
    return true;
}

bool FanCurve::set_curve(std::vector<CurvePoint> points, bool persist) {
    if (is_disabled()) return false;
    for (auto& p : points) p.speed = curve_math::clamp_speed(p.speed);     // > 100 % counts as 100 %
    std::sort(points.begin(), points.end(), by_temp);
    // rejected: too many points, a temperature out of range, two points at the same temperature
    if (curve_math::validate_points(points) != curve_math::CurveError::ok) return false;
    data.points = std::move(points);
    if (persist) save();
    return true;
}

bool FanCurve::set_cold_color(const std::string& hex, bool persist) {
    std::string norm;
    if (is_disabled() || !curve_math::normalize_hex_color(hex, norm)) return false;
    if (norm == data.cold_color) return true;
    data.cold_color = norm;
    if (persist) save();
    return true;
}

bool FanCurve::set_hot_color(const std::string& hex, bool persist) {
    std::string norm;
    if (is_disabled() || !curve_math::normalize_hex_color(hex, norm)) return false;
    if (norm == data.hot_color) return true;
    data.hot_color = norm;
    if (persist) save();
    return true;
}

// ---- internals ----------------------------------------------------------------------------------

void FanCurve::apply_defaults() {
    data = FanCurveStore{};
    const float temps[]  = {22.0f, 27.0f, 32.0f};
    const uint8_t speeds[] = {0, 100, 100};
    for (size_t i = 0; i < 3; ++i) {
        CurvePoint p;
        p.temp  = temps[i];
        p.speed = speeds[i];
        data.points.push_back(p);
    }
}

void FanCurve::save() {
    os.nvs.write_flex(id, "data", data);
}

void FanCurve::update_leds(float temp) {
    // off below the first point (with hysteresis), not persisted
    leds_lit = curve_math::leds_on(data.points, temp, leds_lit);
    if (!leds_lit) {
        argb.set_all_state(false, false);
        return;
    }
    argb.set_all_state(true, false);

    curve_math::Rgb cold{0, 255, 255}, hot{255, 0, 0};
    curve_math::parse_hex_color(data.cold_color, cold.r, cold.g, cold.b);
    curve_math::parse_hex_color(data.hot_color,  hot.r,  hot.g,  hot.b);
    const curve_math::Rgb c = curve_math::led_colour(data.points, temp, cold, hot);
    argb.set_all_rgb(c.r, c.g, c.b, false);
}

// ---- commands -----------------------------------------------------------------------------------

void FanCurve::add_point_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    float temp;
    uint8_t speed;
    if (xewe::str::parse_float(args[0], temp) && xewe::str::parse_int(args[1], speed) && speed <= 100 && add_point(temp, speed))
        os.serial.print("Curve point added.");
    else
        os.serial.print("Failed to add curve point.");
}

void FanCurve::remove_point_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    float temp;
    if (xewe::str::parse_float(args[0], temp) && remove_point(temp)) os.serial.print("Curve point removed.");
    else                                                             os.serial.print("Failed to remove curve point.");
}

void FanCurve::set_cold_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    if (set_cold_color(args[0])) os.serial.print("Cold color set to " + data.cold_color);
    else                         os.serial.print("Failed to set cold color. Use #RRGGBB.");
}

void FanCurve::set_hot_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    if (set_hot_color(args[0])) os.serial.print("Hot color set to " + data.hot_color);
    else                        os.serial.print("Failed to set hot color. Use #RRGGBB.");
}
