#pragma once

#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// SystemConfig
// Shared POD struct that the ESP32 sends to the Pico and the Pico persists
// to flash. Must stay layout-compatible on both sides (both are 32-bit LE).
// ─────────────────────────────────────────────────────────────────────────────
struct SystemConfig {
    // Plant identity
    char     plant_name[32]    = "LeafLink Plant";

    // Moisture thresholds  (percent, 0–100)
    float    moisture_low      = 30.0f;   // below this → pump on
    float    moisture_high     = 70.0f;   // above this → pump off

    // UV alert threshold  (UV index)
    float    uv_alert          = 6.0f;

    // Water tank capacity (fluid ounces)
    float    tank_capacity_oz  = 128.0f;  // 1 gallon default

    // Pump run duration (milliseconds per cycle)
    uint32_t pump_duration_ms  = 5000;

    // Sensor read interval (milliseconds)
    uint32_t sensor_interval_ms = 10000;

    // Battery low threshold (percent)
    float    battery_low       = 20.0f;

    uint8_t  _pad[4]           = {};      // keep struct size a multiple of 4
};

// Wire protocol magic + version (sent at the start of every config packet)
static constexpr uint32_t CONFIG_MAGIC   = 0xEAF0001;
static constexpr uint8_t  PROTO_VERSION  = 1;

// ----------------------------------------------------------------------------
// PlantStatus  – what the Pico sends back to the ESP32
// ----------------------------------------------------------------------------
struct PlantStatus {
    // Moisture
    uint16_t moisture_raw     = 0;
    float    moisture_percent = 0.0f;
    bool     needs_water      = false;

    // UV
    uint16_t uv_raw           = 0;
    float    uv_index         = 0.0f;
    bool     uv_alert         = false;

    // Water level
    uint16_t water_raw        = 0;
    float    water_percent    = 0.0f;
    float    water_oz         = 0.0f;

    // Power
    uint16_t power_raw        = 0;
    float    battery_voltage  = 0.0f;
    float    battery_percent  = 0.0f;

    // Pump
    bool     pump_active      = false;

    uint32_t timestamp_ms     = 0;        // millis() on Pico side
    uint8_t  _pad[3]          = {};
};
