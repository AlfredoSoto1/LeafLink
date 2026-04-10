#pragma once

class Tasks {
public:
  static void load_config_from_flash();
  static void request_config_from_master();

  static void apply_config_to_sensors();

  static void read_sensors();
  static void control_pump();
  static void wifi_communication();
};
