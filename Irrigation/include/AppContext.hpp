#pragma once

#include "WifiController.hpp"
#include "PumpController.hpp"
#include "SensorController.hpp"
#include "StorageController.hpp"
#include "UVModule.hpp"
#include "PowerModule.hpp"
#include "WaterLevelModule.hpp"
#include "SoilMoistureModule.hpp"

class TaskScheduler;

// ---------------------------------------------------------------------------
// RunReport — collects warnings and a single fatal error for the current
// wakeup cycle. Transmitted to master at the end of each cycle.
// ---------------------------------------------------------------------------
struct RunReport {
  bool        has_fatal_error = false;
  const char* fatal_error_msg = nullptr;
  const char* warnings[4]     = {};
  uint8_t     warning_count   = 0;

  void clear() {
    has_fatal_error = false;
    fatal_error_msg = nullptr;
    warning_count   = 0;
  }

  void set_error(const char* msg) {
    has_fatal_error = true;
    fatal_error_msg = msg;
  }

  bool add_warning(const char* msg) {
    if (warning_count >= 4) return false;
    warnings[warning_count++] = msg;
    return true;
  }
};

/**
 * @brief AppContext holds instances of all the main components of the application,
 *        such as sensor controllers, storage, and the task scheduler. It is passed
 *        to all tasks so they can access and manipulate these components as needed.
 * 
 */
struct AppContext {
  WifiController     wifi;
  PumpController     pump;
  UVModule           uv;
  PowerModule        power;
  WaterLevelModule   water;
  SoilMoistureModule moisture;
  
  SensorController   sensor; 
  StorageController  storage;
  TaskScheduler*     scheduler;

  RunReport          report;
};