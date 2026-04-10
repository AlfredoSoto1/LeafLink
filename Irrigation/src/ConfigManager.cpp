#include "SystemConfig.hpp"

#include <cstring>
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/platform.h"

uint32_t ConfigManager::compute_checksum(const SystemConfig &cfg) const {
  const uint8_t *p   = reinterpret_cast<const uint8_t *>(&cfg);
  const uint8_t *end = p + sizeof(SystemConfig);
  uint32_t csum = 0;
  while (p < end) {
    csum ^= static_cast<uint32_t>(*p++);
  }
  return csum;
}

bool ConfigManager::load() {
  const FlashRecord *rec =
      reinterpret_cast<const FlashRecord *>(XIP_BASE + FLASH_OFFSET);

  if (rec->magic != MAGIC) {
    m_valid = false;
    return false;
  }

  if (rec->checksum != compute_checksum(rec->config)) {
    m_valid = false;
    return false;
  }

  m_config = rec->config;
  m_valid  = true;
  return true;
}

bool ConfigManager::save(const SystemConfig &cfg) {
  static_assert(sizeof(FlashRecord) <= FLASH_SECTOR_SIZE,
                "FlashRecord exceeds flash sector size");
  FlashRecord rec;
  rec.magic    = MAGIC;
  rec.config   = cfg;
  rec.checksum = compute_checksum(cfg);

  static uint8_t sector_buf[FLASH_SECTOR_SIZE];
  memset(sector_buf, 0xFF, sizeof(sector_buf));
  memcpy(sector_buf, &rec, sizeof(FlashRecord));

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(FLASH_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(FLASH_OFFSET, sector_buf, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);

  // Verify write
  const FlashRecord *verify =
      reinterpret_cast<const FlashRecord *>(XIP_BASE + FLASH_OFFSET);
  if (verify->magic != MAGIC || verify->checksum != compute_checksum(verify->config)) {
    return false;
  }

  m_config = cfg;
  m_valid  = true;
  return true;
}
