#pragma once

#include "SoilMoistureSensor.hpp"
#include "UVSensor.hpp"
#include "WaterLevelSensor.hpp"
#include "PumpController.hpp"
#include "Wifi.hpp"
#include "PowerSensor.hpp"
#include "SystemConfig.hpp"
#include "ADCController.hpp"
#include "PlantStatus.hpp"
#include "TemperatureSensor.hpp"

class TaskScheduler;

// ---------------------------------------------------------------------------
// AppContext — holds all components and shared state for the application
// ---------------------------------------------------------------------------
struct AppContext {
  SoilMoistureSensor moisture;
  UVSensor           uv;
  WaterLevelSensor   water;
  TemperatureSensor  temperature;
  PumpController     pump;
  WifiModule         wifi;
  PowerSensor        power;
  ConfigManager      config;
  ADCController      adc; 
  TaskScheduler*     scheduler;
  PlantStatus        plant_status = {};
};