// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/src/Fan/Fan.h
#pragma once

#include <string>
#include <vector>

#include <XeWeCore.h>

#include "Config.h"
#include "Curve.h"

// Called in the caller's task (the curve target from loop()), so keep them short.
struct FanListener {
    virtual ~FanListener() = default;
    // celsius is NaN when the source is offline or stale
    virtual void on_curve_target(uint8_t, float, const void*) {}
    virtual void on_curve_changed(const void*) {}
};

// The stored layout is frozen once flashed: append only, and bump `schema` on a change.
struct FanEntry : xewe::FlexData<FanEntry> {
    uint8_t pwm = 0;
    uint8_t tach = 255; // 255 = no tachometer
    uint8_t speed = 0; // 0-255 duty

    static constexpr auto fields() {
        return std::make_tuple(xewe::fld("pwm", &FanEntry::pwm), xewe::fld("tach", &FanEntry::tach),
            xewe::fld("speed", &FanEntry::speed));
    }
};

struct FanStore : xewe::FlexData<FanStore> {
    static constexpr uint8_t SCHEMA = 1;
    uint8_t schema = SCHEMA;
    std::vector<FanEntry> fans;

    static constexpr auto fields() {
        return std::make_tuple(
            xewe::fld("schema", &FanStore::schema), xewe::fld("fans", &FanStore::fans));
    }
};

struct FanCurvePoint : xewe::FlexData<FanCurvePoint> {
    float temp = 0.0f; // °C
    uint8_t speed = 0; // 0-100 %

    static constexpr auto fields() {
        return std::make_tuple(
            xewe::fld("temp", &FanCurvePoint::temp), xewe::fld("speed", &FanCurvePoint::speed));
    }
};

// Append only; bump `schema` on a change.
struct FanCurveStore : xewe::FlexData<FanCurveStore> {
    static constexpr uint8_t SCHEMA = curve_math::SCHEMA;
    uint8_t schema = SCHEMA;
    std::vector<FanCurvePoint> points;

    static constexpr auto fields() {
        return std::make_tuple(xewe::fld("schema", &FanCurveStore::schema),
            xewe::fld("points", &FanCurveStore::points));
    }
};

class Fan : public xewe::Module {
public:
    explicit Fan(xewe::Os& host);
    ~Fan() override;

    void begin_routines_required() override;
    void loop() override;
    void reset(const bool verbose = false, const bool do_restart = true,
        const bool keep_enabled = true) override;
    std::string status(const bool verbose = false) const override;
    xewe::Settings settings() const override;
    void schema_extra(xewe::SchemaOut& out) const override;

    xewe::ListenerSet<FanListener> listeners;

    bool add(uint8_t pwm_pin, uint8_t tach_pin = 255);
    bool remove(uint8_t pwm_pin);
    // persist = false for control loops, so there is no NVS write per call
    bool set(uint8_t pwm_pin, uint8_t speed, bool persist = true);
    bool set_all(uint8_t speed, bool persist = true);
    uint32_t get_rpm(uint8_t pwm_pin) const;
    size_t count() const {
        return fans.size();
    }

    // NaN counts as hot. The first call activates the curve; silence for stale_ms means offline.
    void set_temperature(float celsius, const void* origin = nullptr);
    float get_temperature() const {
        return temperature;
    }
    bool curve_active() const;
    bool curve_failsafe() const;
    uint8_t get_curve_target() const {
        return curve_target_pct;
    }
    const std::vector<FanCurvePoint>& get_curve() const {
        return curve.points;
    }
    bool curve_add(float temp, uint8_t speed_pct, const void* origin = nullptr);
    bool curve_remove(float temp, const void* origin = nullptr);
    // persist = false keeps the change in RAM until save_curve()
    bool set_curve(
        std::vector<FanCurvePoint> points, bool persist = true, const void* origin = nullptr);
    void save_curve();

    std::string get_json() const;
    std::string get_curve_json() const;

    static constexpr size_t MAX_CURVE_POINTS = curve_math::MAX_POINTS;

private:
    struct FanData {
        uint8_t pin_pwm = 0;
        uint8_t pin_tach = 255;
        bool has_tach = false;
        uint8_t speed = 0;

        volatile uint32_t pulse_count = 0;
        uint32_t last_calc_time = 0;

        uint32_t raw_history[3] = {0, 0, 0};
        uint8_t history_idx = 0;
        uint8_t history_count = 0;
        uint32_t ema_rpm = 0;
        uint32_t displayed_rpm = 0;
    };

    static void tach_isr_handler(void* arg);

    bool pins_ok(uint8_t pwm, uint8_t tach) const;
    FanData* get_fan(uint8_t pwm_pin) const;
    void free_fan(FanData* fan);
    FanData* create_fan(uint8_t pwm, uint8_t tach, uint8_t speed);
    void clear_fans();

    void load();
    void save();
    void load_curve();
    void default_curve();
    void run_curve(uint32_t now, bool force = false);
    void curve_changed(const void* origin);
    std::string curve_lines() const;

    void add_cmd(xewe::span<const std::string> args);
    void add_w_tach_cmd(xewe::span<const std::string> args);
    void set_cmd(xewe::span<const std::string> args);
    void set_all_cmd(xewe::span<const std::string> args);
    void remove_cmd(xewe::span<const std::string> args);
    void temp_cmd(xewe::span<const std::string> args);
    void curve_cmd(xewe::span<const std::string> args);

    std::vector<FanData*> fans;

    FanCurveStore curve;
    float temperature{NAN};
    bool temperature_seen{false};
    uint32_t temperature_ms{0};
    const void* temperature_origin{nullptr};
    uint32_t begin_ms{0};
    uint32_t last_curve_ms{0};
    uint8_t curve_target_pct{0};
    uint16_t curve_ms{XEWE_FAN_CURVE_MS}; // table row: curve -> fans period
    uint32_t stale_ms{XEWE_FAN_STALE_MS}; // table row: no update this long -> NaN

    static constexpr uint32_t PWM_FREQ = 25000;
    static constexpr uint8_t PWM_RES = 8;
    static constexpr uint8_t UNSET_PIN = 255;
};
