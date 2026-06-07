#pragma once

#include "AppContext.hpp"

namespace Tasks {
  void init(AppContext &ctx);
  void handle_pico_report(AppContext &ctx);
  void process_events(AppContext &ctx);
  void check_stale_data(AppContext &ctx);
  void maintain_pico_connection(AppContext &ctx);

  bool send_config_to_pico(AppContext &ctx);
  void broadcast_status(AppContext &ctx);
  void broadcast_alert(AppContext &ctx, const char *message, bool is_error = false);
}
