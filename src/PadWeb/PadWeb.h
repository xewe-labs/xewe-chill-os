// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/PadWeb/PadWeb.h
#pragma once

#include <string>
#include <vector>

#include <WebServer.h>
#include <XeWeCore.h>
#include <XeWeModules.h>

#include "../Fan/Fan.h"
#include "../Mlx90614/Mlx90614.h"
#include "Config.h"

// Append only; bump `schema` on a change.
struct PadStore : xewe::FlexData<PadStore> {
    static constexpr uint8_t SCHEMA = 1;
    uint8_t schema = SCHEMA;
    std::string cold_color = "#00FFFF";
    std::string hot_color = "#FF0000";
    bool lights = true; // strip follows the curve

    static constexpr auto fields() {
        return std::make_tuple(xewe::fld("schema", &PadStore::schema),
            xewe::fld("cold_color", &PadStore::cold_color),
            xewe::fld("hot_color", &PadStore::hot_color), xewe::fld("lights", &PadStore::lights));
    }
};

class PadWeb : public xewe::Module, public Mlx90614Listener {
public:
    PadWeb(xewe::Os& host, Fan& fan_ref, Mlx90614& mlx90614_ref, Led& led_ref);

    void begin_routines_required() override;
    void loop() override;
    void reset(const bool verbose = false, const bool do_restart = true,
        const bool keep_enabled = true) override;
    std::string status(const bool verbose = false) const override;

    void on_temperature(float object_c, float ambient_c, bool online) override;

    // call once from setup(), before os.begin()
    void attach(WebInterface& web);

    // an unchanged colour is not written again; persist = false keeps it in RAM
    bool set_cold_color(const std::string& hex, bool persist = true);
    bool set_hot_color(const std::string& hex, bool persist = true);
    void set_lights(bool on);

    std::string get_ui_json() const;
    std::string get_lights_json() const;

private:
    void save() const;
    void update_lights(uint32_t now);
    bool may_change(uint32_t now) const;
    void send_json(const std::string& body, int code = 200);
    void handle_ui_config_post();

    void set_cold_cmd(xewe::span<const std::string> args);
    void set_hot_cmd(xewe::span<const std::string> args);
    void lights_cmd(xewe::span<const std::string> args);

    Fan& fan;
    Mlx90614& mlx90614;
    Led& led;

    PadStore data;
    WebServer* server{nullptr};
    float temperature{NAN}; // last reading, NaN offline
    static constexpr uint8_t SOLID_MODE = 0; // the led module's Solid mode id
    bool lit{true}; // strip state, with hysteresis
    uint32_t last_update_ms{0};
    uint32_t last_change_ms{0};
    bool changed_once{false}; // no rate limit before the first change
};
