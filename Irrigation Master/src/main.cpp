/*
 * LeafLink ESP32-S3 Master
 *
 * Hosts the dashboard/WebSocket API, sends raw configuration bytes to a Pico
 * during pairing, and receives raw Pico state reports over TCP.
 */

#include <Arduino.h>

#include "AppContext.hpp"
#include "Tasks.hpp"

AppContext gCtx;

namespace {
uint32_t lastStaleCheckMs = 0;
constexpr uint32_t STALE_CHECK_INTERVAL_MS = 5000;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== LeafLink ESP32-S3 Master ===");

  Tasks::init(gCtx);
}

void loop() {
  Tasks::maintain_pico_connection(gCtx);
  Tasks::handle_pico_report(gCtx);
  Tasks::process_events(gCtx);

  uint32_t now = millis();
  if (now - lastStaleCheckMs >= STALE_CHECK_INTERVAL_MS) {
    lastStaleCheckMs = now;
    Tasks::check_stale_data(gCtx);
  }

  gCtx.ws.cleanupClients();
  delay(10);
}
