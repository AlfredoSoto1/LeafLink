#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "UVModule.hpp"
#include "PowerModule.hpp"
#include "WaterLevelModule.hpp"
#include "SoilMoistureModule.hpp"

// ---------------------------------------------------------------------------
// Default connection parameters used on first boot (no config in flash yet)
// ---------------------------------------------------------------------------
namespace Defaults {
  static constexpr const char *WIFI_SSID     = "LeafLink_AP";
  static constexpr const char *WIFI_PASSWORD = "leaflink123";
  static constexpr const char *MASTER_HOST   = "192.168.4.1";
  static constexpr uint16_t    MASTER_PORT   = 8080;
}

class StorageController {
public:
  static constexpr uint32_t MAGIC        = 0xA5B6C7D8u;
  static constexpr uint32_t FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

public:
  struct SystemConfig {
    UVModule::Config uv_config;
    PowerModule::Config power_config;
    WaterLevelModule::Config water_config;
    SoilMoistureModule::Config soil_moisture_config;
  };

  struct SystemStates {
    UVModule::State uv_state;
    PowerModule::State power_state;
    WaterLevelModule::State water_state;
    SoilMoistureModule::State soil_moisture_state;
  };

  struct FlashRecord {
    uint32_t     magic;
    SystemConfig config;
    SystemStates state;
    uint32_t     checksum;
  };
};
