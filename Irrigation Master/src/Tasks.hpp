#pragma once

#include "AppContext.hpp"

// ----------------------------------------------------------------------------
// Tasks
// Each function matches the TaskScheduler::TaskFunc signature:
//     void (*)(AppContext &)
//
// Naming mirrors the Pico side intentionally so the two codebases read in
// parallel and it's obvious which ESP32 task corresponds to which Pico task.
// -----------------------------------------------------------------------------
namespace Tasks {
    // Boot sequence
    void init_ap(AppContext &ctx);           // Start WiFi soft-AP
    void init_web_server(AppContext &ctx);   // Register HTTP routes + WS handler
    void build_default_config(AppContext &ctx); // Populate ctx.config with defaults

    // Pico pairing & config delivery
    // (Pico GETs /config → ESP32 serves it; these tasks prep and log that flow)
    void await_pico_connection(AppContext &ctx);
    void send_config_to_pico(AppContext &ctx);   // enqueues ConfigSentToPico event

    // Event processing
    void process_events(AppContext &ctx);    // drain EventQueue each loop tick

    // Frontend / WebSocket
    void broadcast_status(AppContext &ctx);  // push PlantStatus JSON over WS
    void broadcast_alert(AppContext &ctx);   // push top alert from queue

    // Watchdogs
    void check_stale_data(AppContext &ctx);  // enqueue SensorDataStale if needed
}
