#pragma once

#include "SoilMoistureSensor.hpp"
#include "UVSensor.hpp"
#include "Pump.hpp"
#include "Wifi.hpp"
#include "SystemConfig.hpp"

class TaskScheduler;

// ---------------------------------------------------------------------------
// AppContext — holds all components and shared state for the application
// ---------------------------------------------------------------------------
struct AppContext {
  SoilMoistureSensor moisture;
  UVSensor           uv;
  Pump               pump;
  WifiModule         wifi;
  ConfigManager      config;
  TaskScheduler*     scheduler;
};