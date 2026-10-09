// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/FanCurve/FanCurve.h
#pragma once

#include <XeWeCore.h>
#include "../Fan/Fan.h"
#include "../Mlx90614/Mlx90614.h"
#include "../Argb/Argb.h"
#include "CurveMath.h"

#include <string>
#include <vector>


struct FanCurveConfig {
    uint32_t                    update_interval_ms          = 1000;
};

struct CurvePoint : xewe::FlexData<CurvePoint> {
    float                       temp                        = 0.0f;     // °C
    uint8_t                     speed                       = 0;        // 0-100 %

    static constexpr auto fields() {
        return std::make_tuple(fld("temp",  &CurvePoint::temp),
                               fld("speed", &CurvePoint::speed));
    }
};

// Stored settings (NVS namespace "fan_curve", key "data"). Append only; bump `schema` on a change.
struct FanCurveStore : xewe::FlexData<FanCurveStore> {
    static constexpr uint8_t    SCHEMA                      = curve_math::SCHEMA;
    uint8_t                     schema                      = SCHEMA;
    std::vector<CurvePoint>     points;
    std::string                 cold_color                  = "#00FFFF";
    std::string                 hot_color                   = "#FF0000";

    static constexpr auto fields() {
        return std::make_tuple(fld("schema",     &FanCurveStore::schema),
                               fld("points",     &FanCurveStore::points),
                               fld("cold_color", &FanCurveStore::cold_color),
                               fld("hot_color",  &FanCurveStore::hot_color));
    }
};

// The curve maths (interpolation, clamps, validation, LED hysteresis, colours, schema) live in
// CurveMath.h as pure functions, host-tested by tests/host/test_curve_math.cpp.


// The pad's control loop: every second, sensor temperature -> curve -> all fans; LED colour follows
// the temperature between the cold and hot colour (strips off below the first point).
// Requires fan and mlx90614; the LEDs are used only while argb is enabled.
class FanCurve : public xewe::Module {
public:
                                FanCurve                    (xewe::Os&      host,
                                                             Fan&           fan_ref,
                                                             Mlx90614&      mlx90614_ref,
                                                             Argb&          argb_ref,
                                                             FanCurveConfig config = {});

    void                        begin_routines_required     ()                              override;
    void                        loop                        ()                              override;
    void                        reset                       (const bool verbose      = false,
                                                             const bool do_restart   = true,
                                                             const bool keep_enabled = true) override;
    std::string                 status                      (const bool verbose = false)    const override;

    bool                        add_point                   (float temp, uint8_t speed_pct);
    bool                        remove_point                (float temp);
    // replaces the whole curve with one NVS write (web UI POST /ui/config)
    // persist = false: change in RAM only, then save_settings() once (one NVS write per request)
    bool                        set_curve                   (std::vector<CurvePoint> points, bool persist = true);
    // "#RRGGBB" or "RRGGBB"; an unchanged colour is not written again
    bool                        set_cold_color              (const std::string& hex, bool persist = true);
    bool                        set_hot_color               (const std::string& hex, bool persist = true);
    void                        save_settings               ()                              { save(); }
    uint8_t                     get_target_speed            (float temp)                    const;
    uint8_t                     get_last_speed              ()                              const { return last_speed_pct; }

    // {"curve_edge_colors":{"start","end"},"temp_curve":[{"temp","speed"}]} (the web UI's shape)
    std::string                 get_json                    ()                              const;

    static constexpr size_t     MAX_POINTS                  = curve_math::MAX_POINTS;

private:
    void                        apply_defaults              ();
    void                        save                        ();
    void                        update_leds                 (float temp);

    void                        add_point_cmd               (xewe::span<const std::string> args);
    void                        remove_point_cmd            (xewe::span<const std::string> args);
    void                        set_cold_cmd                (xewe::span<const std::string> args);
    void                        set_hot_cmd                 (xewe::span<const std::string> args);

    Fan&                        fan;
    Mlx90614&                   mlx90614;
    Argb&                       argb;
    FanCurveConfig              config;

    FanCurveStore               data;
    uint32_t                    last_update_ms              {0};
    uint8_t                     last_speed_pct              {0};
    bool                        leds_lit                    {true};     // LED state, with hysteresis
};
