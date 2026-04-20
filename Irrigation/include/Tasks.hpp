#pragma once

#include "AppContext.hpp"

// ---------------------------------------------------------------------------
// Tasks — high-level operations that can be scheduled in the main loop
// ---------------------------------------------------------------------------
namespace Tasks {
  void load_config_from_flash(AppContext &ctx);
  void request_config_from_master(AppContext &ctx);
  void apply_config_to_sensors(AppContext &ctx);

  void read_power(AppContext &ctx);
  void read_sensors(AppContext &ctx);
  void check_plant_conditions(AppContext &ctx);
  void control_pump(AppContext &ctx);
  
  void notify_error(AppContext &ctx);
  void notify_status(AppContext &ctx);
}
