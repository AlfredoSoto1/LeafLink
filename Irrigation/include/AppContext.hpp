#pragma once

#include "SoilMoistureSensor.hpp"
#include "UVSensor.hpp"
#include "WaterLevelSensor.hpp"
#include "Pump.hpp"
#include "Wifi.hpp"
#include "PowerSensor.hpp"
#include "SystemConfig.hpp"
#include "ADCController.hpp"
#include "PlantStatus.hpp"

class TaskScheduler;

// ---------------------------------------------------------------------------
// AppContext — holds all components and shared state for the application
// ---------------------------------------------------------------------------
struct AppContext {
  SoilMoistureSensor moisture;
  UVSensor           uv;
  WaterLevelSensor   water;
  Pump               pump;
  WifiModule         wifi;
  PowerSensor        power;
  ConfigManager      config;
  ADCController      adc; 
  TaskScheduler*     scheduler;
  PlantStatus        plant_status = {};
};