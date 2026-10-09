// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/Mlx90614/Mlx90614.h
#pragma once

#include <XeWeCore.h>

#include <string>


// Used on the first boot only (no stored settings yet). 255 = pin not configured.
struct Mlx90614Config {
    uint8_t                     sda_pin                     = 255;
    uint8_t                     scl_pin                     = 255;
    uint8_t                     i2c_address                 = 0x5A;
    uint32_t                    poll_interval_ms            = 500;
    uint32_t                    offline_poll_interval_ms    = 5000;     // back off while no sensor answers
    float                       error_temp                  = 255.0f;   // reported while offline: fans go full speed
};

// Stored settings (NVS namespace "mlx90614", key "data"). Append only; bump `schema` on a change.
struct Mlx90614Store : xewe::FlexData<Mlx90614Store> {
    static constexpr uint8_t    SCHEMA                      = 1;
    uint8_t                     schema                      = SCHEMA;
    uint8_t                     i2c_address                 = 0x5A;
    uint8_t                     sda_pin                     = 255;
    uint8_t                     scl_pin                     = 255;

    static constexpr auto fields() {
        return std::make_tuple(fld("schema",      &Mlx90614Store::schema),
                               fld("i2c_address", &Mlx90614Store::i2c_address),
                               fld("sda_pin",     &Mlx90614Store::sda_pin),
                               fld("scl_pin",     &Mlx90614Store::scl_pin));
    }
};


class Mlx90614 : public xewe::Module {
public:
    explicit                    Mlx90614                    (xewe::Os& host, Mlx90614Config config = {});

    void                        begin_routines_required     ()                              override;
    void                        loop                        ()                              override;
    void                        reset                       (const bool verbose      = false,
                                                             const bool do_restart   = true,
                                                             const bool keep_enabled = true) override;
    std::string                 status                      (const bool verbose = false)    const override;

    // object temperature in °C; error_temp (255.0) while disabled or offline
    float                       get_temp                    ()                              const;
    float                       get_ambient_temp            ()                              const;
    bool                        is_online                   ()                              const;
    bool                        set_i2c_address             (uint8_t new_address);
    bool                        set_pins                    (uint8_t sda, uint8_t scl);
    std::string                 get_json                    ()                              const;

private:
    bool                        pins_configured             ()                              const;
    void                        start_bus                   ();
    float                       read_i2c_temp               (uint8_t register_address)      const;
    void                        poll                        ();
    void                        save                        ()                              const;
    int                         scan                        ();

    void                        read_cmd                    (xewe::span<const std::string> args);
    void                        scan_cmd                    (xewe::span<const std::string> args);
    void                        set_addr_cmd                (xewe::span<const std::string> args);
    void                        set_pins_cmd                (xewe::span<const std::string> args);

    Mlx90614Config              config;

    float                       cached_object_temp          {0.0f};
    float                       cached_ambient_temp         {0.0f};
    bool                        sensor_online               {false};
    bool                        bus_started                 {false};
    uint32_t                    last_read_time              {0};

    uint8_t                     sda_pin                     {255};
    uint8_t                     scl_pin                     {255};
    uint8_t                     i2c_address                 {0x5A};

    static constexpr uint8_t    MLX_RAM_TA                  = 0x06;
    static constexpr uint8_t    MLX_RAM_TOBJ1               = 0x07;
    static constexpr uint8_t    NO_PIN                      = 255;
};
