#pragma once

#include "EventQueue.hpp"
#include "TaskScheduler.hpp"
#include "SystemConfig.hpp"
#include <ESPAsyncWebServer.h>

// ----------------------------------------------------------------------------
// AppContext
// Owns (or references) every subsystem. Passed by reference into every Task.
// ----------------------------------------------------------------------------
struct AppContext {
  // Networking
  AsyncWebServer *server      = nullptr;   
  AsyncWebSocket *ws          = nullptr;   

  // Scheduler + Event queue
  TaskScheduler              *scheduler       = nullptr;
  EventQueue                 *eventDispatcher = nullptr;

  // Shared state
  IrrigationNodeConfig        config;                   // current plant config
  IrrigationNodeStatus        lastStatus;               // most recent data from Pico
  bool                        configReady     = false;
  bool                        picoConnected   = false;
  uint32_t                    lastStatusMs    = 0;      // millis() of last Pico update
  uint32_t                    staleThresholdMs = 30000; // 30 s -> stale alert

  // AP settings (read-only after init)
  const char *apSSID     = "LeafLink-AP";
  const char *apPassword = "leaflink123";
  const char *apIP       = "192.168.4.1";
};