#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <stdio.h>

enum class PlantStatusKind : uint8_t {
  Status = 0,
  Message = 1,
};

enum class ErrorType : uint8_t {
  None = 0,
  SensorReadFailed = 1,
  PumpControlFailed = 2,
  WifiError = 3,
  ConfigError = 4,
};

// ---------------------------------------------------------------------------
// PlantStatus — holds the latest status data and messages for the plant
// ---------------------------------------------------------------------------
struct PlantStatus {
  static constexpr size_t MAX_MESSAGE_LENGTH = 96;

  struct StatusData {
    uint32_t sampled_at_ms;

    bool pump_active;
    bool moisture_needs_water;
    float moisture_percent;

    bool uv_alert;
    float uv_index;

    float water_percent;
    float water_ounces_remaining;

    float temperature_celsius;

    float power_voltage;
    float power_percent;
  };

  struct MessageData {
    bool truncated;
    uint8_t length;
    ErrorType error;
    char text[MAX_MESSAGE_LENGTH];
  };

  union Payload {
    StatusData status;
    MessageData message;
  };

  PlantStatus();

  void clear();
  bool is_dirty() const;

  const Payload &payload() const;
  const StatusData &status() const;
  const MessageData &message() const;
  
  const PlantStatusKind kind() const;

  StatusData& write_status();
  void write_message(ErrorType error, const char *message);
  void write_messagef(ErrorType error, const char *format, ...) {
    char buffer[MAX_MESSAGE_LENGTH] = {};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    write_message(error, buffer);
  }

private:
  static void fill_message(MessageData &record, const char *message,
                           size_t length);

private:
  Payload m_payload;
  PlantStatusKind m_kind;
  bool m_dirty;
};