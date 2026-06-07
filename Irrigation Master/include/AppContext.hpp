#pragma once

#include "EventQueue.hpp"
#include "IrrigationNode.hpp"

#include <ESPAsyncWebServer.h>
#include <WiFi.h>

struct AppContext {
  static constexpr uint16_t PICO_TCP_PORT = 5000;
  static constexpr uint8_t MAX_PICO_NETWORKS = 8;

  struct PicoNetwork {
    char ssid[33]{};
    int32_t rssi = 0;
    bool secure = false;
  };

  AppContext()
    : server(80),
      ws("/ws"),
      picoServer(PICO_TCP_PORT) {}

  AsyncWebServer server;
  AsyncWebSocket ws;
  WiFiServer picoServer;
  EventQueue events;
  IrrigationNode node;

  bool picoConnected = false;
  bool picoServerStarted = false;
  uint32_t lastStateMs = 0;
  uint32_t staleThresholdMs = 30000;

  PicoNetwork picoNetworks[MAX_PICO_NETWORKS];
  uint8_t picoNetworkCount = 0;
  bool picoScanInProgress = false;
  uint32_t picoScanStartedMs = 0;
  uint32_t picoScanCompletedMs = 0;

  bool picoStaWanted = false;
  bool picoStaConnecting = false;
  bool picoStaConnected = false;
  uint32_t picoStaConnectStartedMs = 0;
  uint32_t picoStaLastAttemptMs = 0;
  uint32_t picoServiceLastAttemptMs = 0;
  bool picoNeedsConfig = false;
  bool picoConfigSendInProgress = false;
  char picoStaStatus[96] = "No Pico selected";

  const char *apSSID = "LeafLink-AP";
  const char *apPassword = "leaflink123";
  const char *apIP = "192.168.4.1";

  char picoPairSSID[32] = "PICO_PAIR_DEVICE";
  char picoPairPassword[64] = "12345678";
  const char *picoPairHost = "192.168.5.1";
};
