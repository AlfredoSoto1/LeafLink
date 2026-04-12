#pragma once

#include <cstddef>
#include <cstdint>
#include "SystemConfig.hpp"

enum class EventType : uint8_t {
  None = 0,               // default value for zero-init

  // Pico -> ESP32
  PicoConnected,          // a Pico client just joined the AP
  PicoDisconnected,       // a Pico client left
  PlantStatusReceived,    // fresh PlantStatus arrived over HTTP/WS

  // ESP32 internal events
  ConfigReady,            // SystemConfig built and ready to send
  ConfigSentToPico,       // HTTP response with config was dispatched
  SensorDataStale,        // no status update for > threshold
  PumpAlert,              // pump ran on Pico side
  BatteryLow,             // battery below configured threshold

  // Frontend / WebSocket events
  WsClientConnected,      // browser WebSocket client connected
  WsClientDisconnected,   // browser WebSocket client disconnected
  BroadcastStatus,        // trigger a WS push to all browser clients
  BroadcastAlert,         // push an alert banner to browsers
};

// ----------------------------------------------------------------------------
// Event
// Tagged union – keep it plain-old-data so it can live in a static array.
// ----------------------------------------------------------------------------
struct Event {
  EventType type = EventType::None;

  union Payload {
    IrrigationNodeStatus  status;     // PlantStatusReceived
    IrrigationNodeConfig  config;     // ConfigReady / ConfigSentToPico
    struct {
      uint32_t client_id;
      char     ip[16];
    } client;               // PicoConnected / WsClientConnected …
    char         alert[64]; // BroadcastAlert
    float        value;     // BatteryLow, SensorDataStale
    uint8_t      raw[sizeof(IrrigationNodeStatus)]; // zero-init helper

    Payload() { }           // leave uninitialized; EventQueue zeroes on push
  } payload;

  uint32_t timestamp_ms = 0;  // millis() when the event was enqueued

  Event() = default;

  // Convenience constructors
  static Event make(EventType t, uint32_t ts = 0) {
    Event e;
    e.type         = t;
    e.timestamp_ms = ts;
    return e;
  }

  static Event fromStatus(const IrrigationNodeStatus &s, uint32_t ts = 0) {
    Event e;
    e.type             = EventType::PlantStatusReceived;
    e.payload.status   = s;
    e.timestamp_ms     = ts;
    return e;
  }

  static Event fromConfig(const IrrigationNodeConfig &c, uint32_t ts = 0) {
    Event e;
    e.type            = EventType::ConfigReady;
    e.payload.config  = c;
    e.timestamp_ms    = ts;
    return e;
  }

  static Event fromAlert(const char *msg, uint32_t ts = 0) {
    Event e;
    e.type         = EventType::BroadcastAlert;
    e.timestamp_ms = ts;
    strncpy(e.payload.alert, msg, sizeof(e.payload.alert) - 1);
    e.payload.alert[sizeof(e.payload.alert) - 1] = '\0';
    return e;
  }
};

// -----------------------------------------------------------------------------
// EventQueue
// Simple ring buffer queue for Events. Not thread-safe, but that's fine since
// we'll only access it from the main loop task.
// -----------------------------------------------------------------------------

#define EVENT_QUEUE_CAPACITY 10

class EventQueue {
public:
  EventQueue()  = default;
  ~EventQueue() = default;

  bool push(const Event &evt);
  bool pop(Event &out);
  bool peek(Event &out) const;

  bool   empty() const;
  bool   full()  const;
  size_t size()  const;
  void   clear();       

private:
  Event  _buf[EVENT_QUEUE_CAPACITY] = {};
  size_t _head  = 0;
  size_t _tail  = 0;
  size_t _count = 0;
};
