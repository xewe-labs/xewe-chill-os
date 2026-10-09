// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/Mlx90614/Mlx90614.cpp

#include "Mlx90614.h"

#include <ArduinoJson.h>
#include <Wire.h>
#include <driver/gpio.h>

#include <cmath>
#include <cstdlib>
#include <utility>


Mlx90614::Mlx90614(xewe::Os& host, Mlx90614Config config)
      : xewe::Module(host,
               /* id                  */ "mlx90614",
               /* name                */ "MLX90614",
               /* description         */ "Contactless I2C temperature sensor (object and ambient)",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
      , config(std::move(config))
{
    register_command({
        "read",
        "Read the sensor now",
        "$mlx90614 read",
        0,
        [this](xewe::span<const std::string> args){ read_cmd(args); }
    });

    register_command({
        "scan",
        "Scan the I2C bus for devices",
        "$mlx90614 scan",
        0,
        [this](xewe::span<const std::string> args){ scan_cmd(args); }
    });

    register_command({
        "set_addr",
        "Set the sensor's I2C address (hex, 0x01-0x7F)",
        "$mlx90614 set_addr 0x5A",
        1,
        [this](xewe::span<const std::string> args){ set_addr_cmd(args); }
    });

    register_command({
        "set_pins",
        "Set the I2C pins and restart the bus: <sda> <scl>",
        "$mlx90614 set_pins 4 5",
        2,
        [this](xewe::span<const std::string> args){ set_pins_cmd(args); }
    });

    register_command({
        "print_json",
        "Print the sensor data as JSON",
        "$mlx90614 print_json",
        0,
        [this](xewe::span<const std::string>){ os.serial.print(get_json()); }
    });
}

void Mlx90614::begin_routines_required() {
    Mlx90614Store store;
    if (os.nvs.read_flex(id, "data", store) && store.schema == Mlx90614Store::SCHEMA) {
        i2c_address = store.i2c_address;
        sda_pin     = store.sda_pin;
        scl_pin     = store.scl_pin;
    } else {
        // first boot (or unreadable settings): the project's defaults, then store them
        i2c_address = config.i2c_address;
        sda_pin     = config.sda_pin;
        scl_pin     = config.scl_pin;
        save();
    }

    if (!pins_configured()) {
        os.serial.print("MLX90614: I2C pins not configured; use $mlx90614 set_pins <sda> <scl>");
        return;
    }
    start_bus();
    poll();
    char buf[96];
    if (sensor_online) {
        snprintf(buf, sizeof(buf), "MLX90614: online at 0x%02X, %.2f C", i2c_address, cached_object_temp);
    } else {
        snprintf(buf, sizeof(buf), "MLX90614: no sensor at 0x%02X (SDA %u, SCL %u)", i2c_address, sda_pin, scl_pin);
    }
    os.serial.print(buf);
}

void Mlx90614::loop() {
    if (is_disabled() || !bus_started) return;
    const uint32_t interval = sensor_online ? config.poll_interval_ms : config.offline_poll_interval_ms;
    if (millis() - last_read_time < interval) return;
    poll();
}

void Mlx90614::reset(const bool verbose, const bool do_restart, const bool keep_enabled) {
    cached_object_temp  = 0.0f;
    cached_ambient_temp = 0.0f;
    sensor_online       = false;
    if (bus_started) Wire.end();
    bus_started         = false;
    sda_pin             = NO_PIN;
    scl_pin             = NO_PIN;
    // wipes the "mlx90614" namespace: the next boot uses the Config.h defaults again
    Module::reset(verbose, do_restart, keep_enabled);
}

std::string Mlx90614::status(const bool verbose) const {
    std::string s = Module::status(false);
    char buf[200];
    if (pins_configured()) {
        snprintf(buf, sizeof(buf), "\n  Pins: SDA %u, SCL %u\n  Address: 0x%02X\n  Online: %s",
                 sda_pin, scl_pin, i2c_address, sensor_online ? "yes" : "no");
    } else {
        snprintf(buf, sizeof(buf), "\n  Pins: not configured\n  Address: 0x%02X\n  Online: no", i2c_address);
    }
    s += buf;
    if (sensor_online) {
        snprintf(buf, sizeof(buf), "\n  Object: %.2f C\n  Ambient: %.2f C", cached_object_temp, cached_ambient_temp);
    } else {
        snprintf(buf, sizeof(buf), "\n  Object: %.1f C (error value while offline)", config.error_temp);
    }
    s += buf;
    if (verbose) os.serial.print(s);
    return s;
}

// ---- API ----------------------------------------------------------------------------------------

float Mlx90614::get_temp() const {
    return (!is_disabled() && sensor_online) ? cached_object_temp : config.error_temp;
}

float Mlx90614::get_ambient_temp() const {
    return (!is_disabled() && sensor_online) ? cached_ambient_temp : config.error_temp;
}

bool Mlx90614::is_online() const { return sensor_online; }

bool Mlx90614::set_i2c_address(uint8_t new_address) {
    if (is_disabled() || new_address == 0 || new_address > 0x7F) return false;
    i2c_address = new_address;
    save();
    if (bus_started) poll();
    return true;
}

bool Mlx90614::set_pins(uint8_t sda, uint8_t scl) {
    if (is_disabled() || sda == scl || !GPIO_IS_VALID_OUTPUT_GPIO(sda) || !GPIO_IS_VALID_OUTPUT_GPIO(scl)) return false;
    sda_pin = sda;
    scl_pin = scl;
    save();
    start_bus();
    poll();
    return true;
}

std::string Mlx90614::get_json() const {
    if (is_disabled()) return "{}";
    JsonDocument doc;
    doc["module"] = "MLX90614";
    doc["online"] = sensor_online;
    if (sensor_online) {
        doc["object_temp"]  = cached_object_temp;
        doc["ambient_temp"] = cached_ambient_temp;
    } else {
        doc["object_temp"]  = nullptr;
        doc["ambient_temp"] = nullptr;
    }
    doc["i2c_address"] = i2c_address;
    doc["sda_pin"]     = sda_pin;
    doc["scl_pin"]     = scl_pin;

    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---- internals ----------------------------------------------------------------------------------

bool Mlx90614::pins_configured() const {
    return sda_pin != NO_PIN && scl_pin != NO_PIN;
}

void Mlx90614::start_bus() {
    if (!pins_configured()) return;
    if (bus_started) Wire.end();
    bus_started = Wire.begin(sda_pin, scl_pin);
}

void Mlx90614::poll() {
    last_read_time = millis();
    const float obj = read_i2c_temp(MLX_RAM_TOBJ1);
    const float amb = std::isnan(obj) ? NAN : read_i2c_temp(MLX_RAM_TA);
    sensor_online = !std::isnan(obj) && !std::isnan(amb);
    if (sensor_online) {
        cached_object_temp  = obj;
        cached_ambient_temp = amb;
    }
}

float Mlx90614::read_i2c_temp(uint8_t register_address) const {
    if (!bus_started) return NAN;

    Wire.beginTransmission(i2c_address);
    Wire.write(register_address);
    if (Wire.endTransmission(false) != 0) return NAN;

    // all 3 bytes (LSB, MSB, PEC) or nothing: a short read leaves stale bytes for the next call
    if (Wire.requestFrom(i2c_address, static_cast<uint8_t>(3)) != 3 || Wire.available() < 3) {
        while (Wire.available()) Wire.read();
        return NAN;
    }
    const uint16_t lsb = Wire.read();
    const uint16_t msb = Wire.read();
    Wire.read();    // PEC, not checked
    // bit 15 is the sensor's error flag (0xFFFF on a glitched bus reads as 1037 C): offline, not hot
    if (msb & 0x80) return NAN;
    return (static_cast<float>((msb << 8) | lsb) * 0.02f) - 273.15f;
}

void Mlx90614::save() const {
    Mlx90614Store store;
    store.i2c_address = i2c_address;
    store.sda_pin     = sda_pin;
    store.scl_pin     = scl_pin;
    os.nvs.write_flex(id, "data", store);
}

int Mlx90614::scan() {
    int found = 0;
    std::string results;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "  Found device at 0x%02X%s\n", address, (address == 0x5A) ? " (default MLX90614)" : "");
            results += buf;
            found++;
        }
    }
    if (found == 0) {
        os.serial.print("No I2C devices found. Check wiring and pull-up resistors.");
    } else {
        results += "Scan complete. Found " + std::to_string(found) + " device(s).";
        os.serial.print(results);
    }
    return found;
}

// ---- commands -----------------------------------------------------------------------------------

void Mlx90614::read_cmd(xewe::span<const std::string>) {
    if (is_disabled(true)) return;
    if (!bus_started) {
        os.serial.print("Cannot read: pins not configured. Run $mlx90614 set_pins <sda> <scl>");
        return;
    }
    poll();
    if (!sensor_online) {
        os.serial.print("Failed to read sensor! Check I2C wiring or address.");
        return;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "Object: %.2f C, ambient: %.2f C", cached_object_temp, cached_ambient_temp);
    os.serial.print(buf);
}

void Mlx90614::scan_cmd(xewe::span<const std::string>) {
    if (is_disabled(true)) return;
    if (!bus_started) {
        os.serial.print("Cannot scan: pins not configured. Run $mlx90614 set_pins <sda> <scl>");
        return;
    }
    os.serial.print("Scanning I2C bus...");
    scan();
}

void Mlx90614::set_addr_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    // hex with or without 0x; the whole token must parse
    const std::string& a = args[0];
    char* end = nullptr;
    const unsigned long v = a.empty() ? 0 : strtoul(a.c_str(), &end, 16);
    if (!a.empty() && end && *end == '\0' && v <= 0xFF && set_i2c_address(static_cast<uint8_t>(v))) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Target address updated to 0x%02X and saved to NVS.", static_cast<unsigned>(v));
        os.serial.print(buf);
    } else {
        os.serial.print("Invalid I2C address provided. Use hex 0x01-0x7F (e.g., 0x5A).");
    }
}

void Mlx90614::set_pins_cmd(xewe::span<const std::string> args) {
    if (is_disabled(true)) return;
    uint8_t sda, scl;
    if (!xewe::str::parse_int(args[0], sda) || !xewe::str::parse_int(args[1], scl) || !set_pins(sda, scl)) {
        os.serial.print("Invalid pins. Expected: $mlx90614 set_pins <sda> <scl> (two different output-capable GPIOs)");
        return;
    }
    os.serial.print("Pins updated to SDA=" + std::to_string(sda_pin) + " SCL=" + std::to_string(scl_pin) + ".");
    if (sensor_online) os.serial.print("Sensor online on the new pins.");
    else               os.serial.print("Warning: bus started, but no valid data read from the sensor.");
}
