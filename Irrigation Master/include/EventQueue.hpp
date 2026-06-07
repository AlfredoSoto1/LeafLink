#pragma once

#include "IrrigationNode.hpp"

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

enum class EventType : uint8_t {
  None = 0,
  SendConfigToPico,
  ConfigSentToPico,
  ConfigSendFailed,
  PicoReportReceived,
  WsClientConnected,
  WsClientDisconnected,
  BroadcastStatus,
  BroadcastAlert,
};

struct Event {
  EventType type = EventType::None;
  IrrigationNodeState state{};
  uint32_t client_id = 0;
  uint32_t timestamp_ms = 0;
  bool is_error = false;
  char message[96]{};

  static Event make(EventType type, uint32_t timestamp_ms = 0) {
    Event event;
    event.type = type;
    event.timestamp_ms = timestamp_ms;
    return event;
  }

  static Event fromState(const IrrigationNodeState &state,
                         const char *message = nullptr,
                         bool is_error = false,
                         uint32_t timestamp_ms = 0) {
    Event event = make(EventType::PicoReportReceived, timestamp_ms);
    event.state = state;
    event.is_error = is_error;
    event.setMessage(message);
    return event;
  }

  static Event fromAlert(const char *message,
                         bool is_error = false,
                         uint32_t timestamp_ms = 0) {
    Event event = make(EventType::BroadcastAlert, timestamp_ms);
    event.is_error = is_error;
    event.setMessage(message);
    return event;
  }

  void setMessage(const char *text) {
    if (text == nullptr) {
      message[0] = '\0';
      return;
    }
    strncpy(message, text, sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
  }
};

static constexpr size_t EVENT_QUEUE_CAPACITY = 16;

class EventQueue {
public:
  EventQueue() = default;
  ~EventQueue() = default;

  bool push(const Event &event);
  bool pop(Event &out);
  bool peek(Event &out) const;

  bool empty() const;
  bool full() const;
  size_t size() const;
  void clear();

private:
  Event _buf[EVENT_QUEUE_CAPACITY]{};
  size_t _head = 0;
  size_t _tail = 0;
  size_t _count = 0;
  mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
