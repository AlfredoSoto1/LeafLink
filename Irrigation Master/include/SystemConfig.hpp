#pragma once

#include <cstdint>

// Wire protocol magic + version (sent at the start of every config packet)
static constexpr uint32_t CONFIG_MAGIC   = 0xEAF0001;
static constexpr uint8_t  PROTO_VERSION  = 1;

struct IrrigationNodeConfig {
    // Moisture
    uint16_t moisture_dry_cal       = 3000;
    uint16_t moisture_wet_cal       = 1500;
    float    moisture_threshold_pct = 30.0f;
    uint32_t moisture_sample_count  = 16;
    uint32_t moisture_warmup_ms     = 500;

    // UV
    float    uv_alert_threshold     = 6.0f;
    uint32_t uv_sample_count        = 16;
    uint32_t uv_warmup_ms           = 100;

    // Pump
    uint32_t pump_run_duration_ms = 5000;

    // Water level
    uint16_t water_dry_cal          = 0;
    uint16_t water_wet_cal          = 3500;
    uint32_t water_sample_count     = 8;
    uint32_t water_warmup_ms        = 100;
    uint32_t water_tank_oz          = 128;  // total tank capacity in fluid ounces

    // Power / battery monitor
    uint8_t  battery_pct            = 50; // 0–100%
    uint8_t  battery_low_pct        = 20; // when to trigger low battery alert
    uint32_t power_sample_count     = 8;
};

// ----------------------------------------------------------------------------
// IrrigationStatus  – what the Pico sends back to the ESP32
// ----------------------------------------------------------------------------
struct IrrigationNodeStatus {
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
};
