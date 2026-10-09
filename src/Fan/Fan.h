// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/Fan/Fan.h
#pragma once

#include <XeWeCore.h>

#include <string>
#include <vector>


// One fan the module sets up on its first boot (no stored settings yet). pwm 255 = skip,
// tach 255 = no tachometer.
struct FanDefault {
    uint8_t                     pwm                         = 255;
    uint8_t                     tach                        = 255;
};

struct FanConfig {
    std::vector<FanDefault>     defaults                    {};
    float                       ema_alpha                   = 0.3f;
    uint32_t                    absolute_max_rpm            = 10000;
    uint32_t                    ui_rounding                 = 10;
};

// Stored settings (NVS namespace "fan", key "data"). Layout is frozen once flashed: append only,
// bump `schema` on a change.
struct FanEntry : xewe::FlexData<FanEntry> {
    uint8_t                     pwm                         = 0;
    uint8_t                     tach                        = 255;      // 255 = no tachometer
    uint8_t                     speed                       = 0;        // 0-255 duty

    static constexpr auto fields() {
        return std::make_tuple(fld("pwm",   &FanEntry::pwm),
                               fld("tach",  &FanEntry::tach),
                               fld("speed", &FanEntry::speed));
    }
};

struct FanStore : xewe::FlexData<FanStore> {
    static constexpr uint8_t    SCHEMA                      = 1;
    uint8_t                     schema                      = SCHEMA;
    std::vector<FanEntry>       fans;

    static constexpr auto fields() {
        return std::make_tuple(fld("schema", &FanStore::schema),
                               fld("fans",   &FanStore::fans));
    }
};


class Fan : public xewe::Module {
public:
    explicit                    Fan                         (xewe::Os& host, FanConfig config = {});
                                ~Fan                        () override;

    void                        begin_routines_required     ()                              override;
    void                        loop                        ()                              override;
    void                        reset                       (const bool verbose      = false,
                                                             const bool do_restart   = true,
                                                             const bool keep_enabled = true) override;
    std::string                 status                      (const bool verbose = false)    const override;

    bool                        add                         (uint8_t pwm_pin, uint8_t tach_pin = 255);
    bool                        remove                      (uint8_t pwm_pin);
    bool                        set                         (uint8_t pwm_pin, uint8_t speed, bool persist = true);
    // persist = false for control loops (FanCurve): no NVS write per call
    bool                        set_all                     (uint8_t speed, bool persist = true);
    uint32_t                    get_rpm                     (uint8_t pwm_pin)               const;
    size_t                      count                       ()                              const { return fans.size(); }

    // {"fans":[{"pin_pwm","has_tach","speed"(0-100),"pin_tach","displayed_rpm","ema_rpm"}]}
    std::string                 get_json                    ()                              const;

private:
    struct FanData {
        uint8_t             pin_pwm         = 0;
        uint8_t             pin_tach        = 255;
        bool                has_tach        = false;
        uint8_t             speed           = 0;

        volatile uint32_t   pulse_count     = 0;
        uint32_t            last_calc_time  = 0;

        uint32_t            raw_history[3]  = {0, 0, 0};
        uint8_t             history_idx     = 0;
        uint8_t             history_count   = 0;
        uint32_t            ema_rpm         = 0;
        uint32_t            displayed_rpm   = 0;
    };

    static void                 tach_isr_handler            (void* arg);

    FanData*                    get_fan                     (uint8_t pwm_pin)               const;
    void                        free_fan                    (FanData* fan);
    FanData*                    create_fan                  (uint8_t pwm, uint8_t tach, uint8_t speed);
    void                        clear_fans                  ();

    void                        load                        ();
    void                        save                        ();

    void                        add_cmd                     (xewe::span<const std::string> args);
    void                        add_w_tach_cmd              (xewe::span<const std::string> args);
    void                        set_cmd                     (xewe::span<const std::string> args);
    void                        set_all_cmd                 (xewe::span<const std::string> args);
    void                        remove_cmd                  (xewe::span<const std::string> args);

    FanConfig                   config;
    std::vector<FanData*>       fans;

    static constexpr uint32_t   PWM_FREQ                    = 25000;
    static constexpr uint8_t    PWM_RES                     = 8;
    static constexpr uint8_t    NO_PIN                      = 255;
};
