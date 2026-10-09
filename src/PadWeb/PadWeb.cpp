// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/PadWeb/PadWeb.cpp

#include "PadWeb.h"

#include <ArduinoJson.h>

// page assets, copied unchanged from the 0.0.12 firmware
#include "index_html.h"
#include "script_js.h"
#include "styles_css.h"


PadWeb::PadWeb(Fan& fan_ref, Mlx90614& mlx90614_ref, Argb& argb_ref, FanCurve& fan_curve_ref)
    : fan(fan_ref), mlx90614(mlx90614_ref), argb(argb_ref), fan_curve(fan_curve_ref) {}

void PadWeb::attach(WebServer& srv) {
    server = &srv;

    server->on("/pad", HTTP_GET, [this]() {
        server->send_P(200, "text/html", INDEX_HTML);
    });
    server->on("/static/styles.css", HTTP_GET, [this]() {
        server->send_P(200, "text/css", STYLES_CSS);
    });
    server->on("/static/script.js", HTTP_GET, [this]() {
        server->send_P(200, "application/javascript", SCRIPT_JS);
    });

    server->on("/api/state", HTTP_GET, [this]() {
        send_json("{\"fan_data\":"    + fan.get_json() +
                  ",\"argb_data\":"   + argb.get_json() +
                  ",\"sensor_data\":" + mlx90614.get_json() +
                  ",\"ui_config\":"   + fan_curve.get_json() + "}");
    });
    server->on("/fan/data",      HTTP_GET, [this]() { send_json(fan.get_json()); });
    server->on("/argb/data",     HTTP_GET, [this]() { send_json(argb.get_json()); });
    server->on("/mlx90614/data", HTTP_GET, [this]() { send_json(mlx90614.get_json()); });
    server->on("/ui/config",     HTTP_GET, [this]() { send_json(fan_curve.get_json()); });
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

    // every change in RAM first, then one NVS write for the whole request (the page posts colours and
    // curve together on every save; three writes of the same blob per click wore the flash 3x)
    bool ok = true;
    JsonObjectConst colors = doc["curve_edge_colors"];
    if (!colors.isNull()) {
        if (colors["start"].is<const char*>()) ok &= fan_curve.set_cold_color(colors["start"].as<const char*>(), false);
        if (colors["end"].is<const char*>())   ok &= fan_curve.set_hot_color(colors["end"].as<const char*>(), false);
    }

    JsonArrayConst curve = doc["temp_curve"];
    if (!curve.isNull()) {
        std::vector<CurvePoint> points;
        for (JsonVariantConst v : curve) {
            if (v["temp"].isNull() || v["speed"].isNull()) continue;
            CurvePoint p;
            p.temp  = v["temp"].as<float>();
            const int s = v["speed"].as<int>();
            p.speed = static_cast<uint8_t>(s < 0 ? 0 : (s > 100 ? 100 : s));
            points.push_back(p);
        }
        ok &= fan_curve.set_curve(std::move(points), false);
    }
    if (fan_curve.is_enabled()) fan_curve.save_settings();

    send_json(std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"ui_config\":" + fan_curve.get_json() + "}");
}
