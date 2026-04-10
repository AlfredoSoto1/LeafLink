#pragma once

#include "AppContext.hpp"

// ---------------------------------------------------------------------------
// Tasks — high-level operations that can be scheduled in the main loop
// ---------------------------------------------------------------------------
class Tasks {
public:
  static void load_config_from_flash(AppContext &ctx);
  static void request_config_from_master(AppContext &ctx);

  static void apply_config_to_sensors(AppContext &ctx);

  static void read_sensors(AppContext &ctx);
  static void control_pump(AppContext &ctx);
  static void send_plant_status(AppContext &ctx);
};
