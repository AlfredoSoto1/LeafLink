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
  void start(AppContext &ctx);
  void init_access_point(AppContext &ctx);
  void init_web_server(AppContext &ctx);
  void build_default_config(AppContext &ctx); 

  // Pico pairing & config delivery
  void await_pico_connection(AppContext &ctx);
  void send_config_to_pico(AppContext &ctx);  

  // Event processing
  void process_events(AppContext &ctx);

  // Frontend / WebSocket
  void broadcast_status(AppContext &ctx);
  void broadcast_alert(AppContext &ctx);

  // Watchdogs
  void check_stale_data(AppContext &ctx);
}