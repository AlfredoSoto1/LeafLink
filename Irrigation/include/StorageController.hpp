#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "UVModule.hpp"
#include "PowerModule.hpp"
#include "WaterLevelModule.hpp"
#include "SoilMoistureModule.hpp"
#include "WifiController.hpp"
#include "PumpController.hpp"

class StorageController {
public:
  static constexpr uint32_t MAGIC        = 0xA5B6C7D8u;
  static constexpr uint32_t FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

public:
  struct SystemConfig {
    WifiController::Config wifi_config;
    PumpController::Config pump_config;

    UVModule::Config uv_config;
    PowerModule::Config power_config;
    WaterLevelModule::Config water_config;
    SoilMoistureModule::Config soil_moisture_config;
  };

  struct SystemStates {
    WifiController::State wifi_state;
    PumpController::State pump_state;

    UVModule::State uv_state;
    PowerModule::State power_state;
    WaterLevelModule::State water_state;
    SoilMoistureModule::State soil_moisture_state;
  };

  struct FlashRecord {
    uint32_t     magic;
    SystemConfig config;
    SystemStates state;
  };

  enum class State {
    OK,
    NO_DATA,
    ERROR,
  };
  
public:
  FlashRecord flash;
  State state = State::NO_DATA;

public:
  /**
   * @brief Initializes the storage controller. This should be called once at startup before
   *        any load or save operations. It may perform any necessary setup for flash access.
   * 
   */
  void init();

  /**
   * @brief Load the system configuration and state from flash memory.
   *        This reads the flash sector into RAM, validates the magic number, and returns
   *        pointers to the config and state. If the magic number is invalid, it returns nullptr
   *        and zeroes out the internal record to prevent accidental use of invalid data.  
   * 
   */
  void load();

  /**
   * @brief Save the current system configuration and state to flash memory. This will erase the
   *        target flash sector and write the new data. It also calculates and stores a checksum
   *        to help validate the integrity of the data on future loads.
   * 
   */
  void save();
};
