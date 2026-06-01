#pragma once

#include "AppContext.hpp"

// ---------------------------------------------------------------------------
// Tasks — high-level operations that can be scheduled in the main loop
// ---------------------------------------------------------------------------
namespace Tasks {

  // Boot & configuration
  void boot_os(AppContext &ctx);
  void request_config_from_master(AppContext &ctx);
  void finish(AppContext &ctx);

  // Per-cycle entry point
  void wakeup_os(AppContext &ctx);

  // Sensor reads (chained in order)
  void read_power(AppContext &ctx);
  void read_moisture(AppContext &ctx);
  void read_uv(AppContext &ctx);
  void read_water_level(AppContext &ctx);

  // Plant logic & actuation
  void check_plant_conditions(AppContext &ctx);
  void control_pump(AppContext &ctx);

  // Persistence & reporting
  void save_states(AppContext &ctx);
  void transmit_report(AppContext &ctx);
}
