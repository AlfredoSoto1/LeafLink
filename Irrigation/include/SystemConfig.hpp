#pragma once

#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// ---------------------------------------------------------------------------
// Default connection parameters used on first boot (no config in flash yet)
// ---------------------------------------------------------------------------
namespace Defaults {
  static constexpr const char *WIFI_SSID     = "LeafLink_AP";
  static constexpr const char *WIFI_PASSWORD = "leaflink123";
  static constexpr const char *MASTER_HOST   = "192.168.4.1";
  static constexpr uint16_t    MASTER_PORT   = 8080;
}

// ---------------------------------------------------------------------------
// SystemConfig — all tunable parameters persisted in flash
// ---------------------------------------------------------------------------
struct SystemConfig {
  // Soil moisture sensor
  uint16_t moisture_dry_cal       = 3000;
  uint16_t moisture_wet_cal       = 1500;
  float    moisture_threshold_pct = 30.0f;
  uint32_t moisture_sample_count  = 16;
  uint32_t moisture_warmup_ms     = 500;

  // UV sensor
  float    uv_alert_threshold     = 6.0f;
  uint32_t uv_sample_count        = 16;
  uint32_t uv_warmup_ms           = 100;

  // Pump
  uint32_t pump_run_duration_ms   = 5000;

  // Water level sensor
  uint16_t water_dry_cal          = 0;
  uint16_t water_wet_cal          = 3500;
  uint32_t water_sample_count     = 8;
  uint32_t water_warmup_ms        = 100;
  uint32_t water_tank_oz          = 128;  // total tank capacity in fluid ounces

  // Power / battery monitor
  float    power_v_max            = 4.2f;  // voltage at 100 %
  float    power_v_min            = 3.0f;  // voltage at 0 %
  float    power_divider_ratio    = 0.5f;  // R2 / (R1 + R2)
  uint32_t power_sample_count     = 8;
};

// ---------------------------------------------------------------------------
// ConfigManager — load / save SystemConfig to the last flash sector
//
// Wire format used when receiving config from master via Wi-Fi (all values
// are unsigned integers; floats are scaled by 100 to avoid float parsing):
//
//   CFG:<m_dry>,<m_wet>,<m_thresh*100>,<m_samples>,<m_warmup>,
//       <uv_alert*100>,<uv_samples>,<uv_warmup>,<pump_ms>,
//       <w_dry>,<w_wet>,<w_samples>,<w_warmup>,<w_tank_oz>,
//       <pwr_vmax*100>,<pwr_vmin*100>,<pwr_div*1000>,<pwr_samples>\r\n
// ---------------------------------------------------------------------------
class ConfigManager {
public:
  static constexpr uint32_t MAGIC        = 0xA5B6C7D8u;
  static constexpr uint32_t FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

  ConfigManager() = default;

  bool load();
  bool save(const SystemConfig &cfg);

  bool               is_valid() const { return m_valid;  }
  const SystemConfig &get()     const { return m_config; }

private:
  struct FlashRecord {
    uint32_t     magic;
    SystemConfig config;
    uint32_t     checksum;
  };

  uint32_t compute_checksum(const SystemConfig &cfg) const;

  SystemConfig m_config = {};
  bool         m_valid  = false;
};
