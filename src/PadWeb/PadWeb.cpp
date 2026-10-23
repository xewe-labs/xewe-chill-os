// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/PadWeb/PadWeb.cpp

#include "PadWeb.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstdlib>
#include <utility>

// the pad page
#include "index_html.h"
#include "script_js.h"
#include "styles_css.h"

namespace {
// hex colours: the core's helpers (xewe::color): "#rrggbb" or "rrggbb", either case
curve_math::Rgb parse_or(const std::string& hex, curve_math::Rgb fallback) {
    curve_math::Rgb c = fallback;
    xewe::color::parse_hex_color(hex, c.r, c.g, c.b); // outputs untouched on failure
    return c;
}

// any accepted form -> the stored form "#RRGGBB"; false (out untouched) when not a colour
bool normalize_hex(const std::string& hex, std::string& out) {
    uint8_t r, g, b;
    if (!xewe::color::parse_hex_color(hex, r, g, b))
        return false;
    out = xewe::color::to_hex_color(r, g, b);
    return true;
}

// true when a channel differs by `step` or more, or `exact` and any channel differs (an edge colour
// is shown exactly, not left `step` short)
bool moved(const std::array<uint8_t, 3>& shown, const std::array<uint8_t, 3>& target, int step,
    bool exact) {
    for (size_t i = 0; i < 3; ++i) {
        const int d = std::abs(static_cast<int>(shown[i]) - static_cast<int>(target[i]));
        if (d >= step || (exact && d != 0))
            return true;
    }
    return false;
}

bool same_curve(const std::vector<FanCurvePoint>& a, const std::vector<FanCurvePoint>& b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].temp != b[i].temp || a[i].speed != b[i].speed)
            return false;
    }
    return true;
}
} // namespace

PadWeb::PadWeb(xewe::Os& host, Fan& fan_ref, Mlx90614& mlx90614_ref, Led& led_ref)
    : xewe::Module(host,
          /* id                  */ "pad",
          /* name                */ "Pad",
          /* description         */
          "Feeds the sensor to the fan curve, colours the strip by temperature, serves the pad "
          "page at /",
          /* requires_init_setup */ false,
          /* can_be_disabled     */ false,
          /* has_cli_cmds        */ true),
      fan(fan_ref), mlx90614(mlx90614_ref), led(led_ref) {
    // no add_requirement(): the page and the strip keep working while fan or mlx90614 is disabled
    // the glue: every sensor reading (and every offline poll) goes to the fan curve
    mlx90614.listeners.add(this);

    register_command(
        {"set_cold", "Set the strip colour at the curve's first point (cold): <RRGGBB|#RRGGBB>",
            "$pad set_cold 00FFFF", 1,
            [this](xewe::span<const std::string> args) { set_cold_cmd(args); }});

    register_command(
        {"set_hot", "Set the strip colour at the curve's last point (hot): <RRGGBB|#RRGGBB>",
            "$pad set_hot FF0000", 1,
            [this](xewe::span<const std::string> args) { set_hot_cmd(args); }});

    register_command(
        {"lights", "Strip follows the temperature (on) or is left to $led (off): <on|off>",
            "$pad lights on", 1, [this](xewe::span<const std::string> args) { lights_cmd(args); }});

    register_command({"print_json", "Print the colours, the curve and the strip state as JSON",
        "$pad print_json", 0, [this](xewe::span<const std::string>) {
            os.serial.print(get_ui_json() + "\n" + get_lights_json());
        }});
}

void PadWeb::begin_routines_required() {
    PadStore stored;
    if (!os.nvs.read_flex(id, "data", stored)) {
        data = PadStore{}; // first boot: store the defaults
        save();
    } else if (!stored.has("schema") || stored.schema != PadStore::SCHEMA) {
        // has(): a blob without a readable `schema` field is foreign too, not "schema 1 by default"
        // another firmware's layout: never reinterpret it, never overwrite it at boot
        os.serial.print("Pad: stored colours are from another firmware (schema " +
            std::to_string(stored.schema) + ", expected " + std::to_string(PadStore::SCHEMA) +
            "); using the defaults until the next change");
        data = PadStore{};
    } else {
        data = std::move(stored);
        std::string norm;
        if (!normalize_hex(data.cold_color, norm))
            data.cold_color = PadStore{}.cold_color;
        if (!normalize_hex(data.hot_color, norm))
            data.hot_color = PadStore{}.hot_color;
    }
    last_update_ms = millis();
}

void PadWeb::loop() {
    const uint32_t now = millis();
    if (now - last_update_ms < XEWE_PAD_UPDATE_MS)
        return;
    last_update_ms = now;
    update_lights(now);
}

void PadWeb::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    data = PadStore{};
    // wipes the "pad" namespace: the next boot stores the default colours again
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string PadWeb::status(const bool verbose) const {
    std::string s = Module::status(false);
    s += std::string("\n  Lights: ") +
        (data.lights ? "on (strip follows the temperature)" : "off (strip left to $led)");
    s += "\n  Cold color: " + data.cold_color + "\n  Hot color: " + data.hot_color;
    char buf[64];
    if (std::isnan(temperature))
        snprintf(buf, sizeof(buf), "\n  Temperature: offline (no sensor reading)");
    else
        snprintf(buf, sizeof(buf), "\n  Temperature: %.2f C", temperature);
    s += buf;
    if (data.lights) {
        const auto rgb = led.get_rgb();
        s += "\n  Strip: " + std::string(lit ? "lit " : "dark ") +
            xewe::color::to_hex_color(rgb[0], rgb[1], rgb[2]);
    }
    if (verbose)
        os.serial.print(s);
    return s;
}

// ---- glue ---------------------------------------------------------------------------------------

void PadWeb::on_temperature(float object_c, float, bool online) {
    temperature = online ? object_c : NAN;
    fan.set_temperature(temperature, this);
}

bool PadWeb::may_change(uint32_t now) const {
    return !changed_once || now - last_change_ms >= XEWE_PAD_COLOR_MIN_MS;
}

// The strip follows the curve: on/off with hysteresis, Solid mode, the curve colour. Every led call
// is stored by the led module, so each is sent only on a real change and rate limited.
void PadWeb::update_lights(uint32_t now) {
    if (!data.lights || !may_change(now))
        return;

    const auto& points = fan.get_curve();
    lit = curve_math::leds_on(points, temperature, lit);
    bool changed = false;
    if (led.get_state() != lit) {
        led.sync_state(lit, this);
        changed = true;
    }
    if (lit) {
        if (led.get_current_mode_id() != SOLID_MODE) {
            led.sync_mode(SOLID_MODE, this);
            changed = true;
        }
        const curve_math::Rgb cold = parse_or(data.cold_color, {0, 255, 255});
        const curve_math::Rgb hot = parse_or(data.hot_color, {255, 0, 0});
        const curve_math::Rgb c = curve_math::led_colour(points, temperature, cold, hot);
        const std::array<uint8_t, 3> target{c.r, c.g, c.b};
        const bool edge = (c.r == cold.r && c.g == cold.g && c.b == cold.b) ||
            (c.r == hot.r && c.g == hot.g && c.b == hot.b);
        if (moved(led.get_rgb(), target, XEWE_PAD_COLOR_STEP, edge)) {
            led.sync_color(target, this);
            changed = true;
        }
    }
    if (changed) {
        last_change_ms = now;
        changed_once = true;
    }
}

// ---- settings -----------------------------------------------------------------------------------

bool PadWeb::set_cold_color(const std::string& hex, bool persist) {
    std::string norm;
    if (!normalize_hex(hex, norm))
        return false;
    if (norm == data.cold_color)
        return true;
    data.cold_color = norm;
    if (persist)
        save();
    return true;
}

bool PadWeb::set_hot_color(const std::string& hex, bool persist) {
    std::string norm;
    if (!normalize_hex(hex, norm))
        return false;
    if (norm == data.hot_color)
        return true;
    data.hot_color = norm;
    if (persist)
        save();
    return true;
}

void PadWeb::set_lights(bool on) {
    if (data.lights == on)
        return;
    data.lights = on;
    save();
}

void PadWeb::save() const {
    os.nvs.write_flex(id, "data", data);
}

std::string PadWeb::get_ui_json() const {
    JsonDocument doc;
    JsonObject colors = doc["curve_edge_colors"].to<JsonObject>();
    colors["start"] = data.cold_color;
    colors["end"] = data.hot_color;
    JsonArray arr = doc["temp_curve"].to<JsonArray>();
    for (const auto& p : fan.get_curve()) {
        JsonObject o = arr.add<JsonObject>();
        o["temp"] = p.temp;
        o["speed"] = p.speed;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

std::string PadWeb::get_lights_json() const {
    JsonDocument doc;
    doc["lights"] = data.lights;
    const auto rgb = led.get_rgb();
    doc["shown"] = xewe::color::to_hex_color(rgb[0], rgb[1], rgb[2]);
    if (std::isnan(temperature))
        doc["temperature"] = nullptr;
    else
        doc["temperature"] = temperature;
    doc["led_brightness"] = led.get_brightness();
    doc["led_state"] = led.get_state();
    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---- web ----------------------------------------------------------------------------------------

void PadWeb::attach(WebInterface& web) {
    server = &web.get_server();

    server->on("/pad", HTTP_GET, [this]() { server->send_P(200, "text/html", INDEX_HTML); });
    // the pad page owns `/` (web-interface root hook); the command console stays at /console
    web.set_root([this]() { server->send_P(200, "text/html", INDEX_HTML); });
    server->on(
        "/static/styles.css", HTTP_GET, [this]() { server->send_P(200, "text/css", STYLES_CSS); });
    server->on("/static/script.js", HTTP_GET,
        [this]() { server->send_P(200, "application/javascript", SCRIPT_JS); });

    server->on("/api/state", HTTP_GET, [this]() {
        send_json("{\"fan_data\":" + fan.get_json() + ",\"argb_data\":" + get_lights_json() +
            ",\"sensor_data\":" + mlx90614.get_json() + ",\"ui_config\":" + get_ui_json() + "}");
    });
    server->on("/fan/data", HTTP_GET, [this]() { send_json(fan.get_json()); });
    server->on("/led/data", HTTP_GET, [this]() { send_json(get_lights_json()); });
    server->on("/mlx90614/data", HTTP_GET, [this]() { send_json(mlx90614.get_json()); });
    server->on("/ui/config", HTTP_GET, [this]() { send_json(get_ui_json()); });
    server->on("/ui/config", HTTP_POST, [this]() { handle_ui_config_post(); });
}

void PadWeb::send_json(const std::string& body, int code) {
    server->sendHeader("Access-Control-Allow-Origin", "*");
    server->send(code, "application/json", body.c_str());
}

void PadWeb::handle_ui_config_post() {
    String payload = server->arg("plain");
    if (payload.isEmpty()) {
        send_json("{\"error\":\"Empty payload\"}", 400);
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        send_json("{\"error\":\"Invalid JSON\"}", 400);
        return;
    }

    // every change in RAM first, then at most one NVS write per blob (`pad/data`, `fan/curve`), and
    // none for a blob that did not change (the page posts colours and curve together on every save)
    bool ok = true;
    const PadStore before = data;
    JsonObjectConst colors = doc["curve_edge_colors"];
    if (!colors.isNull()) {
        if (colors["start"].is<const char*>())
            ok &= set_cold_color(colors["start"].as<const char*>(), false);
        if (colors["end"].is<const char*>())
            ok &= set_hot_color(colors["end"].as<const char*>(), false);
    }
    if (data.cold_color != before.cold_color || data.hot_color != before.hot_color)
        save();

    JsonArrayConst curve = doc["temp_curve"];
    if (!curve.isNull()) {
        std::vector<FanCurvePoint> points;
        for (JsonVariantConst v : curve) {
            if (v["temp"].isNull() || v["speed"].isNull())
                continue;
            FanCurvePoint p;
            p.temp = v["temp"].as<float>();
            const int s = v["speed"].as<int>();
            p.speed = static_cast<uint8_t>(s < 0 ? 0 : (s > 100 ? 100 : s));
            points.push_back(p);
        }
        const std::vector<FanCurvePoint> old = fan.get_curve();
        const bool set = fan.set_curve(std::move(points), false, this);
        ok &= set;
        if (set && !same_curve(old, fan.get_curve()))
            fan.save_curve();
    }

    send_json(std::string("{\"ok\":") + (ok ? "true" : "false") +
        ",\"ui_config\":" + get_ui_json() + "}");
}

// ---- commands -----------------------------------------------------------------------------------

void PadWeb::set_cold_cmd(xewe::span<const std::string> args) {
    if (set_cold_color(args[0]))
        os.serial.print("Cold color set to " + data.cold_color);
    else
        os.serial.print("Failed to set cold color. Use RRGGBB or #RRGGBB.");
}

void PadWeb::set_hot_cmd(xewe::span<const std::string> args) {
    if (set_hot_color(args[0]))
        os.serial.print("Hot color set to " + data.hot_color);
    else
        os.serial.print("Failed to set hot color. Use RRGGBB or #RRGGBB.");
}

void PadWeb::lights_cmd(xewe::span<const std::string> args) {
    const std::string& a = args[0];
    if (a == "on" || a == "1")
        set_lights(true);
    else if (a == "off" || a == "0")
        set_lights(false);
    else {
        os.serial.print("Usage: $pad lights <on|off>");
        return;
    }
    os.serial.print(std::string("Pad lights ") +
        (data.lights ? "on: the strip follows the temperature."
                     : "off: the strip is left to $led."));
}
