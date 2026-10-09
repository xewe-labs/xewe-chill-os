// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-os-laptop-cooling-pad/src/PadWeb/PadWeb.h
//
// The cooling pad's own code, everything else is repo modules (fan, mlx90614, led, wifi,
// web-interface). PadWeb is a project-local module (id `pad`, NVS namespace `pad`) that
//   1. links the sensor to the fan curve: it is an Mlx90614Listener and passes every reading to
//      fan.set_temperature() (offline -> NaN -> the curve's fail-safe: fans at the last point);
//   2. lights the strip along the same curve: below the first point off (0.5 C hysteresis), cold
//      colour at the first point, hot at the last, a blend in between; the colour glides (Glide.h)
//      and is shown with led.fill() (RAM only, no NVS write per frame). `$pad lights off` hands the
//      strip back to the led module's modes;
//   3. serves the pad page at /pad and its JSON API on the web-interface module's server
//      (attach(web_interface.get_server()), MIGRATION.md CP5).
#pragma once

#include <WebServer.h>
#include <XeWeCore.h>
#include <XeWeModules.h>

#include <string>
#include <vector>

#include "Glide.h"


struct PadWebConfig {
    float                       glide_speed                 = 8.0f;     // exponential approach, 1/s
    uint32_t                    frame_ms                    = 20;       // 50 fps, the led render rate
};

// Stored settings (NVS namespace "pad", key "data"). Append only; bump `schema` on a change.
struct PadStore : xewe::FlexData<PadStore> {
    static constexpr uint8_t    SCHEMA                      = 1;
    uint8_t                     schema                      = SCHEMA;
    std::string                 cold_color                  = "#00FFFF";
    std::string                 hot_color                   = "#FF0000";
    bool                        lights                      = true;     // strip follows the curve

    static constexpr auto fields() {
        return std::make_tuple(fld("schema",     &PadStore::schema),
                               fld("cold_color", &PadStore::cold_color),
                               fld("hot_color",  &PadStore::hot_color),
                               fld("lights",     &PadStore::lights));
    }
};


class PadWeb : public xewe::Module, public Mlx90614Listener {
public:
                                PadWeb                      (xewe::Os&    host,
                                                             Fan&         fan_ref,
                                                             Mlx90614&    mlx90614_ref,
                                                             Led&         led_ref,
                                                             PadWebConfig config = {});

    void                        begin_routines_required     ()                              override;
    void                        loop                        ()                              override;
    void                        reset                       (const bool verbose      = false,
                                                             const bool do_restart   = true,
                                                             const bool keep_enabled = true) override;
    std::string                 status                      (const bool verbose = false)    const override;

    // the sensor -> curve link (Mlx90614Listener)
    void                        on_temperature              (float object_c, float ambient_c, bool online) override;

    // registers the page and the JSON routes; call once, before or after the server begins
    void                        attach                      (WebServer& server);

    // "#RRGGBB" or "RRGGBB"; an unchanged colour is not written again. persist = false: RAM only
    bool                        set_cold_color              (const std::string& hex, bool persist = true);
    bool                        set_hot_color               (const std::string& hex, bool persist = true);
    void                        set_lights                  (bool on);

    // {"curve_edge_colors":{"start","end"},"temp_curve":[{"temp","speed"}]} (the page's shape)
    std::string                 get_ui_json                 ()                              const;
    // {"lights","shown":"#RRGGBB","temperature"|null,"led_brightness","led_state"}
    std::string                 get_lights_json             ()                              const;

private:
    void                        save                        ()                              const;
    void                        update_lights               (uint32_t now);
    void                        send_json                   (const std::string& body, int code = 200);
    void                        handle_ui_config_post       ();

    void                        set_cold_cmd                (xewe::span<const std::string> args);
    void                        set_hot_cmd                 (xewe::span<const std::string> args);
    void                        lights_cmd                  (xewe::span<const std::string> args);

    Fan&                        fan;
    Mlx90614&                   mlx90614;
    Led&                        led;
    PadWebConfig                config;

    PadStore                    data;
    WebServer*                  server                      {nullptr};
    float                       temperature                 {NAN};      // last reading, NaN offline
    bool                        lit                         {true};     // strip state, with hysteresis
    bool                        filling                     {false};    // led.fill() owned by us
    pad_fx::Colour              shown                       {};
    uint32_t                    last_frame_ms               {0};
};
