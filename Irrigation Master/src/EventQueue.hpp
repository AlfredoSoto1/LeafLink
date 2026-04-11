#pragma once

#include <cstddef>
#include <cstdint>
#include "SystemConfig.hpp"

// ----------------------------------------------------------------------------
// EventType
// Every event that can travel through the system gets a tag here.
// ----------------------------------------------------------------------------
enum class EventType : uint8_t {
    None = 0,

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
        PlantStatus  status;     // PlantStatusReceived
        SystemConfig config;     // ConfigReady / ConfigSentToPico
        struct {
            uint32_t client_id;
            char     ip[16];
        } client;               // PicoConnected / WsClientConnected …
        char         alert[64]; // BroadcastAlert
        float        value;     // BatteryLow, SensorDataStale
        uint8_t      raw[sizeof(PlantStatus)]; // zero-init helper

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

    static Event fromStatus(const PlantStatus &s, uint32_t ts = 0) {
        Event e;
        e.type             = EventType::PlantStatusReceived;
        e.payload.status   = s;
        e.timestamp_ms     = ts;
        return e;
    }

    static Event fromConfig(const SystemConfig &c, uint32_t ts = 0) {
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
// EventQueue  –  lock-free single-producer / single-consumer ring buffer
//
// On the ESP32-S3 we run everything on the Arduino loop() thread, so we don't
// need a mutex here. If you later add an RTOS task that pushes from ISR context
// wrap push() / pop() with portENTER_CRITICAL / portEXIT_CRITICAL.
// -----------------------------------------------------------------------------
template<size_t Capacity = 16>
class EventQueue {
public:
    static_assert(Capacity >= 2, "EventQueue needs at least 2 slots");

    EventQueue()  = default;
    ~EventQueue() = default;

    // Returns false if the queue is full (event is dropped).
    bool push(const Event &evt) {
        if (full()) return false;
        _buf[_tail] = evt;
        _tail = (_tail + 1) % Capacity;
        ++_count;
        return true;
    }

    // Pops into `out`. Returns false if empty.
    bool pop(Event &out) {
        if (empty()) return false;
        out   = _buf[_head];
        _head = (_head + 1) % Capacity;
        --_count;
        return true;
    }

    // Peek without consuming.
    bool peek(Event &out) const {
        if (empty()) return false;
        out = _buf[_head];
        return true;
    }

    bool   empty() const { return _count == 0; }
    bool   full()  const { return _count == Capacity; }
    size_t size()  const { return _count; }
    void   clear()       { _head = _tail = _count = 0; }

private:
    Event  _buf[Capacity] = {};
    size_t _head  = 0;
    size_t _tail  = 0;
    size_t _count = 0;
};
