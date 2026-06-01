#include "StorageController.hpp"

#include <cstring>

static_assert(
  sizeof(StorageController::FlashRecord) <= FLASH_SECTOR_SIZE,
  "FlashRecord must fit inside one flash sector"
);

void StorageController::init() {
  // Try loading from flash to validate if data is present. This will set the internal state
  // accordingly, so we can check it later when applying config to sensors.
  load();
}

void StorageController::load() {
  const auto* flash_ptr =
    reinterpret_cast<const FlashRecord*>(XIP_BASE + FLASH_OFFSET);

  std::memcpy(&flash, flash_ptr, sizeof(FlashRecord));

  // Validate the magic number to check if the data is valid. If not, zero out
  // the record and set state to NO_DATA.
  if (flash.magic != MAGIC) {
    std::memset(&flash, 0, sizeof(FlashRecord));
    state = State::NO_DATA;
  } else {
    state = State::OK;
  }
}

void StorageController::save() {
  uint8_t sector_buffer[FLASH_SECTOR_SIZE];
  std::memset(sector_buffer, 0xFF, sizeof(sector_buffer));
  std::memcpy(sector_buffer, &flash, sizeof(FlashRecord));

  uint32_t interrupts = save_and_disable_interrupts();

  flash_range_erase(FLASH_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(FLASH_OFFSET, sector_buffer, FLASH_SECTOR_SIZE);

  restore_interrupts(interrupts);
}
