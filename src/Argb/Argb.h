// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/Argb/Argb.h
#pragma once

#include <XeWeCore.h>

#include <string>
#include <vector>

class CLEDController;
struct CRGB;


// Strips the module sets up on its first boot (no stored settings yet). 255 = skip.
struct ArgbConfig {
    std::vector<uint8_t>        default_pins                {};
    uint16_t                    default_length              = 12;
    float                       glide_speed                 = 8.0f;     // exponential approach, 1/s
};

// Stored settings (NVS namespace "argb", key "data"). Append only; bump `schema` on a change.
struct ArgbStrip : xewe::FlexData<ArgbStrip> {
    uint8_t                     pin                         = 0;
    uint16_t                    length                      = 12;
    bool                        state                       = false;
    uint8_t                     r                           = 255;
    uint8_t                     g                           = 255;
    uint8_t                     b                           = 255;

    static constexpr auto fields() {
        return std::make_tuple(fld("pin",    &ArgbStrip::pin),
                               fld("length", &ArgbStrip::length),
                               fld("state",  &ArgbStrip::state),
                               fld("r",      &ArgbStrip::r),
                               fld("g",      &ArgbStrip::g),
                               fld("b",      &ArgbStrip::b));
    }
};

struct ArgbStore : xewe::FlexData<ArgbStore> {
    static constexpr uint8_t    SCHEMA                      = 1;
    uint8_t                     schema                      = SCHEMA;
    std::vector<ArgbStrip>      strips;

    static constexpr auto fields() {
        return std::make_tuple(fld("schema", &ArgbStore::schema),
                               fld("strips", &ArgbStore::strips));
    }
};


// WS2812B strips on FastLED (one RMT channel per strip), each with on/off state and a target colour
// that the strip glides to.
class Argb : public xewe::Module {
public:
    explicit                    Argb                        (xewe::Os& host, ArgbConfig config = {});
                                ~Argb                       () override;

    void                        begin_routines_required     ()                              override;
    void                        loop                        ()                              override;
    void                        reset                       (const bool verbose      = false,
                                                             const bool do_restart   = true,
                                                             const bool keep_enabled = true) override;
    std::string                 status                      (const bool verbose = false)    const override;

    bool                        add                         (uint8_t pin, uint16_t length = 0);    // 0 = default length
    bool                        remove                      (uint8_t pin);
    bool                        set_length                  (uint8_t pin, uint16_t length);
    // persist = false for control loops (FanCurve): no NVS write per call
    bool                        set_state                   (uint8_t pin, bool state, bool persist = true);
    bool                        set_rgb                     (uint8_t pin, uint8_t r, uint8_t g, uint8_t b, bool persist = true);
    bool                        set_all_state               (bool state, bool persist = true);
    bool                        set_all_rgb                 (uint8_t r, uint8_t g, uint8_t b, bool persist = true);
    size_t                      count                       ()                              const { return strips.size(); }

    // [{"pin","length","state","r","g","b"}]
    std::string                 get_json                    ()                              const;

    static constexpr uint16_t   MAX_LENGTH                  = 300;

private:
    struct StripData {
        ArgbStrip           cfg;
        float               current_r       = 0.0f;
        float               current_g       = 0.0f;
        float               current_b       = 0.0f;
        bool                transitioning   = false;
        CRGB*               pixels          = nullptr;
        CLEDController*     driver          = nullptr;
    };

    StripData*                  get_strip                   (uint8_t pin)                   const;
    StripData*                  create_strip                (const ArgbStrip& cfg);
    void                        free_strip                  (StripData* s);
    void                        clear_strips                ();
    void                        show                        (StripData* s);
    size_t                      max_strips                  ()                              const;

    void                        load                        ();
    void                        save                        ();

    void                        add_cmd                     (xewe::span<const std::string> args);
    void                        remove_cmd                  (xewe::span<const std::string> args);
    void                        set_length_cmd              (xewe::span<const std::string> args);
    void                        set_state_cmd               (xewe::span<const std::string> args);
    void                        set_rgb_cmd                 (xewe::span<const std::string> args);

    ArgbConfig                  config;
    std::vector<StripData*>     strips;
    uint32_t                    last_frame_ms               {0};

    static constexpr uint8_t    NO_PIN                      = 255;
};
