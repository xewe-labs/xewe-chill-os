// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-laptop-cooling-pad/src/PadWeb/PadWeb.cpp

#include "PadWeb.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <utility>

// page assets, copied unchanged from the 0.0.12 firmware
#include "index_html.h"
#include "script_js.h"
#include "styles_css.h"


namespace {
// hex colours: the core's helpers (core 2.1, Utils/String.h): "#rrggbb" or "rrggbb", either case
curve_math::Rgb parse_or(const std::string& hex, curve_math::Rgb fallback) {
    curve_math::Rgb c = fallback;
    xewe::str::parse_hex_color(hex, c.r, c.g, c.b);     // outputs untouched on failure
    return c;
}

// any accepted form -> the stored form "#RRGGBB"; false (out untouched) when not a colour
bool normalize_hex(const std::string& hex, std::string& out) {
    uint8_t r, g, b;
    if (!xewe::str::parse_hex_color(hex, r, g, b)) return false;
    out = xewe::str::to_hex_color(r, g, b);
    return true;
}

bool same_curve(const std::vector<FanCurvePoint>& a, const std::vector<FanCurvePoint>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].temp != b[i].temp || a[i].speed != b[i].speed) return false;
    }
    return true;
}
} // namespace


PadWeb::PadWeb(xewe::Os& host, Fan& fan_ref, Mlx90614& mlx90614_ref, Led& led_ref, PadWebConfig config)
      : xewe::Module(host,
               /* id                  */ "pad",
               /* name                */ "Pad",
               /* description         */ "Cooling pad glue: sensor -> fan curve, strip colour along the curve, the /pad page",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ false,
               /* has_cli_cmds        */ true)
      , fan(fan_ref)
      , mlx90614(mlx90614_ref)
      , led(led_ref)
      , config(std::move(config))
{
    // no add_requirement(): the page and the strip keep working while fan or mlx90614 is disabled
    // the glue: every sensor reading (and every offline poll) goes to the fan curve
    mlx90614.listeners.add(this);

    register_command({
        "set_cold",
        "Set the strip colour at the curve's first point: <#RRGGBB|RRGGBB>",
        "$pad set_cold 00FFFF",
        1,
        [this](xewe::span<const std::string> args){ set_cold_cmd(args); }
    });

    register_command({
        "set_hot",
        "Set the strip colour at the curve's last point: <#RRGGBB|RRGGBB>",
        "$pad set_hot FF0000",
        1,
        [this](xewe::span<const std::string> args){ set_hot_cmd(args); }
    });

    register_command({
        "lights",
        "Strip follows the temperature (on) or the led module's modes (off): <on|off>",
        "$pad lights on",
        1,
        [this](xewe::span<const std::string> args){ lights_cmd(args); }
    });

    register_command({
        "print_json",
        "Print the curve colours and the strip state as JSON",
        "$pad print_json",
        0,
        [this](xewe::span<const std::string>){ os.serial.print(get_ui_json() + "\n" + get_lights_json()); }
    });
}

void PadWeb::begin_routines_required() {
    PadStore stored;
    if (!os.nvs.read_flex(id, "data", stored)) {
        data = PadStore{};                                  // first boot: store the defaults
        save();
    } else if (!stored.has("schema") || stored.schema != PadStore::SCHEMA) {
        // has(): a blob without a readable `schema` field is foreign too, not "schema 1 by default" (CC7)
        // another firmware's layout: never reinterpret it, never overwrite it at boot
        os.serial.print("Pad: stored settings have schema " + std::to_string(stored.schema) +
                        ", this firmware reads schema " + std::to_string(PadStore::SCHEMA) +
                        "; using the defaults (the stored blob is left untouched until the next change)");
        data = PadStore{};
    } else {
        data = std::move(stored);
        std::string norm;
        if (!normalize_hex(data.cold_color, norm)) data.cold_color = PadStore{}.cold_color;
        if (!normalize_hex(data.hot_color, norm))  data.hot_color  = PadStore{}.hot_color;
    }
    last_frame_ms = millis();
}

void PadWeb::loop() {
    const uint32_t now = millis();
    if (now - last_frame_ms < config.frame_ms) return;
    update_lights(now);
}

void PadWeb::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    data = PadStore{};
    if (filling) led.clear_fill();
    filling = false;
    // wipes the "pad" namespace: the next boot stores the default colours again
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string PadWeb::status(const bool verbose) const {
    std::string s = Module::status(false);
    s += std::string("\n  Lights: ") + (data.lights ? "follow the temperature" : "off (led modes)");
    s += "\n  Cold colour: " + data.cold_color + "\n  Hot colour: " + data.hot_color;
    char buf[64];
    if (std::isnan(temperature)) snprintf(buf, sizeof(buf), "\n  Temperature: offline");
    else                         snprintf(buf, sizeof(buf), "\n  Temperature: %.2f C", temperature);
    s += buf;
    if (data.lights) {
        s += "\n  Strip: " + std::string(lit ? "lit " : "dark ") +
             xewe::str::to_hex_color(pad_fx::to_byte(shown.r), pad_fx::to_byte(shown.g), pad_fx::to_byte(shown.b));
    }
    if (verbose) os.serial.print(s);
    return s;
}

// ---- glue ---------------------------------------------------------------------------------------

void PadWeb::on_temperature(float object_c, float, bool online) {
    temperature = online ? object_c : NAN;
    fan.set_temperature(temperature, this);
}

void PadWeb::update_lights(uint32_t now) {
    const float dt = (now - last_frame_ms) / 1000.0f;
    last_frame_ms = now;

    if (!data.lights) {
        if (filling) led.clear_fill();      // hand the strip back to the led module's mode
        filling = false;
        return;
    }

    const auto& points = fan.get_curve();
    lit = curve_math::leds_on(points, temperature, lit);
    pad_fx::Colour target;
    if (lit) {
        const curve_math::Rgb c = curve_math::led_colour(points, temperature,
                                                         parse_or(data.cold_color, {0, 255, 255}),
                                                         parse_or(data.hot_color,  {255, 0, 0}));
        target = {static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)};
    }
    pad_fx::glide_step(shown, target, pad_fx::glide_factor(config.glide_speed, dt));
    // every frame: a `$led mode set` or `$led fill off` ends the fill, the next frame takes it back
    led.fill({pad_fx::to_byte(shown.r), pad_fx::to_byte(shown.g), pad_fx::to_byte(shown.b)});
    filling = true;
}

// ---- settings -----------------------------------------------------------------------------------

bool PadWeb::set_cold_color(const std::string& hex, bool persist) {
    std::string norm;
    if (!normalize_hex(hex, norm)) return false;
    if (norm == data.cold_color) return true;
    data.cold_color = norm;
    if (persist) save();
    return true;
}

bool PadWeb::set_hot_color(const std::string& hex, bool persist) {
    std::string norm;
    if (!normalize_hex(hex, norm)) return false;
    if (norm == data.hot_color) return true;
    data.hot_color = norm;
    if (persist) save();
    return true;
}

void PadWeb::set_lights(bool on) {
    if (data.lights == on) return;
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
    colors["end"]   = data.hot_color;
    JsonArray arr = doc["temp_curve"].to<JsonArray>();
    for (const auto& p : fan.get_curve()) {
        JsonObject o = arr.add<JsonObject>();
        o["temp"]  = p.temp;
        o["speed"] = p.speed;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

std::string PadWeb::get_lights_json() const {
    JsonDocument doc;
    doc["lights"] = data.lights;
    doc["shown"]  = xewe::str::to_hex_color(pad_fx::to_byte(shown.r), pad_fx::to_byte(shown.g), pad_fx::to_byte(shown.b));
    if (std::isnan(temperature)) doc["temperature"] = nullptr;
    else                         doc["temperature"] = temperature;
    doc["led_brightness"] = led.get_brightness();
    doc["led_state"]      = led.get_state();
    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---- web ----------------------------------------------------------------------------------------

void PadWeb::attach(WebInterface& web) {
    server = &web.get_server();

    server->on("/pad", HTTP_GET, [this]() {
        server->send_P(200, "text/html", INDEX_HTML);
    });
    // the pad page owns `/` (web-interface root hook); the command console stays at /console
    web.set_root([this]() { server->send_P(200, "text/html", INDEX_HTML); });
    server->on("/static/styles.css", HTTP_GET, [this]() {
        server->send_P(200, "text/css", STYLES_CSS);
    });
    server->on("/static/script.js", HTTP_GET, [this]() {
        server->send_P(200, "application/javascript", SCRIPT_JS);
    });

    server->on("/api/state", HTTP_GET, [this]() {
        send_json("{\"fan_data\":"    + fan.get_json() +
                  ",\"argb_data\":"   + get_lights_json() +
                  ",\"sensor_data\":" + mlx90614.get_json() +
                  ",\"ui_config\":"   + get_ui_json() + "}");
    });
    server->on("/fan/data",      HTTP_GET, [this]() { send_json(fan.get_json()); });
    server->on("/led/data",      HTTP_GET, [this]() { send_json(get_lights_json()); });
    server->on("/mlx90614/data", HTTP_GET, [this]() { send_json(mlx90614.get_json()); });
    server->on("/ui/config",     HTTP_GET, [this]() { send_json(get_ui_json()); });
    server->on("/ui/config",     HTTP_POST, [this]() { handle_ui_config_post(); });
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
        if (colors["start"].is<const char*>()) ok &= set_cold_color(colors["start"].as<const char*>(), false);
        if (colors["end"].is<const char*>())   ok &= set_hot_color(colors["end"].as<const char*>(), false);
    }
    if (data.cold_color != before.cold_color || data.hot_color != before.hot_color) save();

    JsonArrayConst curve = doc["temp_curve"];
    if (!curve.isNull()) {
        std::vector<FanCurvePoint> points;
        for (JsonVariantConst v : curve) {
            if (v["temp"].isNull() || v["speed"].isNull()) continue;
            FanCurvePoint p;
            p.temp  = v["temp"].as<float>();
            const int s = v["speed"].as<int>();
            p.speed = static_cast<uint8_t>(s < 0 ? 0 : (s > 100 ? 100 : s));
            points.push_back(p);
        }
        const std::vector<FanCurvePoint> old = fan.get_curve();
        const bool set = fan.set_curve(std::move(points), false, this);
        ok &= set;
        if (set && !same_curve(old, fan.get_curve())) fan.save_curve();
    }

    send_json(std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"ui_config\":" + get_ui_json() + "}");
}

// ---- commands -----------------------------------------------------------------------------------

void PadWeb::set_cold_cmd(xewe::span<const std::string> args) {
    if (set_cold_color(args[0])) os.serial.print("Cold color set to " + data.cold_color);
    else                         os.serial.print("Failed to set cold color. Use #RRGGBB.");
}

void PadWeb::set_hot_cmd(xewe::span<const std::string> args) {
    if (set_hot_color(args[0])) os.serial.print("Hot color set to " + data.hot_color);
    else                        os.serial.print("Failed to set hot color. Use #RRGGBB.");
}

void PadWeb::lights_cmd(xewe::span<const std::string> args) {
    const std::string& a = args[0];
    if (a == "on" || a == "1")       set_lights(true);
    else if (a == "off" || a == "0") set_lights(false);
    else {
        os.serial.print("Usage: $pad lights <on|off>");
        return;
    }
    os.serial.print(std::string("Pad lights ") + (data.lights ? "on: the strip follows the temperature."
                                                              : "off: the strip shows the led module's mode."));
}
