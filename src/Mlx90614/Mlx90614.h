// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/Mlx90614/Mlx90614.h
//
// An MLX90614 contactless I2C temperature sensor (object and ambient), polled every
// XEWE_MLX90614_POLL_MS (XEWE_MLX90614_OFFLINE_POLL_MS while it does not answer). Error reads
// (short read, the sensor's error flag) are reported as offline, never as a temperature. Listeners
// (Mlx90614Listener) get every poll result; PadWeb feeds them to the fan curve. NVS namespace
// `mlx90614`: the settings rows `addr`, `sda`, `scl`.
#pragma once

#include <cmath>
#include <string>

#include <XeWeCore.h>

#include "Config.h"
#include "Convert.h"

// Poll results. Called from the main loop after every poll (500 ms online, 5 s offline) and after
// `read`/`set_pins`/`set_addr`. Offline: online = false and both temperatures NaN. Keep it short.
// A reading has no origin (nobody but the sensor causes it), so the core's origin argument is
// omitted.
struct Mlx90614Listener {
    virtual ~Mlx90614Listener() = default;
    virtual void on_temperature(float object_c, float ambient_c, bool online) = 0;
};

class Mlx90614 : public xewe::Module {
public:
    explicit Mlx90614(xewe::Os& host);

    void begin_routines_required() override;
    void loop() override;
    void reset(const bool verbose = false, const bool do_restart = true,
        const bool keep_enabled = true) override;
    std::string status(const bool verbose = false) const override;
    xewe::Settings settings() const override;

    // object temperature in °C; NaN while disabled or offline (never an error value such as 1037 C)
    float get_temp() const;
    float get_ambient_temp() const;
    bool is_online() const;
    uint32_t get_error_count() const {
        return read_errors;
    }
    bool set_i2c_address(uint8_t new_address);
    bool set_pins(uint8_t sda, uint8_t scl);
    // {"module","online","object_temp"|null,"ambient_temp"|null,"i2c_address","sda_pin","scl_pin","read_errors"}
    std::string get_json() const;

    // up to XEWE_MLX90614_LISTENERS_MAX, no heap (core ListenerSet): listeners.add(&l) / remove(&l)
    xewe::ListenerSet<Mlx90614Listener, XEWE_MLX90614_LISTENERS_MAX> listeners;
    // the same as listeners.add / listeners.remove
    bool add_listener(Mlx90614Listener* l) {
        return listeners.add(l);
    }
    bool remove_listener(Mlx90614Listener* l) {
        return listeners.remove(l);
    }

protected:
    // `addr`: re-poll; `sda`/`scl`: restart the bus on the new pins (claims follow)
    void on_setting_changed(const xewe::SettingDef& def) override;

private:
    bool pins_configured() const;
    void start_bus();
    float read_i2c_temp(uint8_t register_address);
    void poll();
    void notify();
    void release_pins();
    int scan();

    void read_cmd(xewe::span<const std::string> args);
    void scan_cmd(xewe::span<const std::string> args);
    void set_addr_cmd(xewe::span<const std::string> args);
    void set_pins_cmd(xewe::span<const std::string> args);

    float cached_object_temp{NAN};
    float cached_ambient_temp{NAN};
    bool sensor_online{false};
    bool bus_started{false};
    uint32_t last_read_time{0};
    uint32_t read_errors{0}; // error flag or short read (not a missing sensor)

    uint8_t sda_pin{255}; // table rows `sda`, `scl`, `addr`
    uint8_t scl_pin{255};
    uint8_t i2c_address{XEWE_MLX90614_ADDR};
    uint8_t bus_sda{255}; // the pins claimed for the running bus
    uint8_t bus_scl{255};
    bool batch{false}; // set_pins: one bus restart

    static constexpr uint8_t MLX_RAM_TA = 0x06;
    static constexpr uint8_t MLX_RAM_TOBJ1 = 0x07;
    static constexpr uint8_t UNSET_PIN = 255;
};
