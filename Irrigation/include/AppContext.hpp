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
// AppContext — holds all components and shared state for the application
// ---------------------------------------------------------------------------
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
};