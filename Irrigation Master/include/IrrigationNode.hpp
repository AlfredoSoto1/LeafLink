#pragma once

#include <cstdint>
#include <cstddef>

enum class WifiState : int32_t {
  DISCONNECTED = 0,
  CONNECTING,
  CONNECTED,
  ERROR,
};

struct WifiConfig {
  char ap_ssid[32]{"LeafLink-AP"};
  char ap_password[64]{"leaflink123"};
  char master_host[64]{"192.168.4.1"};
  uint16_t tcp_port = 5000;
};

struct PumpConfig {
  float target_oz_per_day = 1.0f;
  float flow_rate_oz_per_sec = 0.0f;
};

struct UVConfig {
  float alert_threshold = 6.0f;
  float min_voltage = 1.0f;
  float max_voltage = 2.8f;
  float max_uv_index = 15.0f;
};

struct PowerConfig {
  uint32_t warmup_ms = 100;
  float v_max = 4.2f;
  float v_min = 3.0f;
  float divider_ratio = 0.5f;
};

struct WaterLevelConfig {
  uint32_t warmup_ms = 100;
  float tank_capacity_oz = 128.0f;
  float tank_min_threshold_percent = 10.0f;
};

struct SoilMoistureConfig {
  uint32_t warmup_ms = 500;
  float threshold_percent = 30.0f;
  uint16_t dry_cal = 1023;
  uint16_t wet_cal = 0;
};

// Wire placeholder for Pico SDK repeating_timer_t. The Pico ignores this when
// active is false, but its bytes must be present to match SystemConfig size.
struct alignas(8) PicoRepeatingTimerWire {
  int64_t delay_us = 0;
  uint32_t pool = 0;
  int32_t alarm_id = 0;
  uint32_t callback = 0;
  uint32_t user_data = 0;
};

struct SleepTimeConfig {
  PicoRepeatingTimerWire timer;
  bool active = false;
  uint8_t padding[3]{0, 0, 0};
  uint32_t sleep_interval_ms = 10000;
};

struct IrrigationNodeConfig {
  WifiConfig wifi;
  PumpConfig pump;
  UVConfig uv;
  PowerConfig power;
  WaterLevelConfig water;
  SoilMoistureConfig soilMoisture;
  SleepTimeConfig sleep;
};

struct PumpState {
  bool running = false;
  uint32_t duration_ms = 0;
  uint32_t started_at_ms = 0;
  float total_oz_dispensed = 0.0f;
};

struct UVState {
  float uv_index = 0.0f;
  bool is_alert = false;
  bool error = false;
};

struct PowerState {
  float voltage = 0.0f;
  float percentage = 0.0f;
  bool warning = false;
  bool error = false;
};

struct WaterLevelState {
  float ounces_remaining = 0.0f;
  bool error = false;
};

struct SoilMoistureState {
  float moisture_percent = 0.0f;
  bool is_dry = false;
  bool error = false;
};

struct IrrigationNodeState {
  WifiState wifi = WifiState::DISCONNECTED;
  PumpState pump;
  UVState uv;
  PowerState power;
  WaterLevelState water;
  SoilMoistureState soilMoisture;
};

struct IrrigationNode {
  IrrigationNodeConfig config;
  IrrigationNodeState state;
};

static_assert(sizeof(WifiConfig) == 162, "WifiConfig must match Pico wire layout");
static_assert(sizeof(PicoRepeatingTimerWire) == 24, "Timer placeholder must match Pico repeating_timer_t");
static_assert(sizeof(SleepTimeConfig) == 32, "SleepTimeConfig must match Pico wire layout");
static_assert(sizeof(IrrigationNodeConfig) == 264, "IrrigationNodeConfig must match Pico SystemConfig");
static_assert(sizeof(IrrigationNodeState) == 56, "IrrigationNodeState must match Pico SystemStates");
