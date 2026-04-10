#include "Tasks.hpp"
#include <stdio.h>
#include "pico/stdlib.h"

void Tasks::load_config_from_flash(AppContext &ctx) {
  // Implementation for loading config from flash
  printf("Loading config...\n");
}

void Tasks::request_config_from_master(AppContext &ctx) {
  // Implementation for requesting config from master
  printf("Requesting config from master...\n");
}

void Tasks::apply_config_to_sensors(AppContext &ctx) {
  // Implementation for applying config to sensors
  printf("Applying config to sensors...\n");
}

void Tasks::read_sensors(AppContext &ctx) {
  // Implementation for reading sensors
  printf("Reading sensors...\n");

  printf("Reading sensors...\n");

  auto moistureReading = ctx.moisture.read();
  auto uvReading = ctx.uv.read();

  printf("Moisture: raw=%u percent=%.2f needs_water=%s\n",
         moistureReading.raw,
         moistureReading.percent,
         moistureReading.needs_water ? "yes" : "no");

  printf("UV: raw=%u uv_index=%.2f alert=%s\n",
         uvReading.raw,
         uvReading.uv_index,
         uvReading.is_alert ? "yes" : "no");

  // if (moistureReading.needs_water) {
  //   ctx.scheduler->schedule(Tasks::control_pump);
  // }
  // ctx.scheduler->schedule(Tasks::send_plant_status);
}

void Tasks::control_pump(AppContext &ctx) {
  // Implementation for controlling the pump
  printf("Controlling pump...\n");
}

void Tasks::send_plant_status(AppContext &ctx) {
  // Implementation for WiFi communication
  printf("Sending plant status...\n");
}
