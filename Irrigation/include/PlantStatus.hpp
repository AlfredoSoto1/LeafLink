#pragma once

#include <cstddef>
#include <cstdint>

enum class PlantStatusKind : uint8_t {
  Status = 0,
  Message = 1,
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

    float power_voltage;
    float power_percent;
  };

  struct MessageData {
    bool truncated;
    uint8_t length;
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
  void write_message(const char *message);
  void write_message(const char *message, size_t length);

private:
  static void fill_message(MessageData &record, const char *message,
                           size_t length);

private:
  Payload m_payload;
  PlantStatusKind m_kind;
  bool m_dirty;
};