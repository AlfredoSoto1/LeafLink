#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/rtc.h"
#include "hardware/clocks.h"
#include "pico/util/datetime.h"

// ---------------------------------------------------------------------------
// Hardware Pin Assignments  (edit to match your board wiring)
// ---------------------------------------------------------------------------
#define PIN_PUMP_OUT        15   // Pump driver GPIO (active HIGH)
#define PIN_MOISTURE_ADC    26   // ADC0 — capacitive moisture sensor
#define PIN_UV_ADC          27   // ADC1 — ML8511 UV sensor (analog mode)
#define PIN_LEVEL_TRIG       6   // HC-SR04 trigger
#define PIN_LEVEL_ECHO       7   // HC-SR04 echo
#define PIN_WAKE            22   // ESP8266 pulls HIGH to wake the Pico
#define PIN_SLEEP_SIGNAL    21   // Pico pulls HIGH to tell ESP8266 "I am sleeping"
#define PIN_WIFI_TX          0   // UART0 TX → ESP8266 RX
#define PIN_WIFI_RX          1   // UART0 RX ← ESP8266 TX

// ---------------------------------------------------------------------------
// Flash – reserve the last 4 KB sector for persistent configuration
// ---------------------------------------------------------------------------
#define FLASH_CONFIG_OFFSET  (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define CONFIG_MAGIC         0xA5B6C7D8u

// ---------------------------------------------------------------------------
// Task queue capacity
// ---------------------------------------------------------------------------
#define TASK_QUEUE_SIZE      16

// ---------------------------------------------------------------------------
// Task types
// ---------------------------------------------------------------------------
enum class TaskType : uint8_t {
    NONE             = 0,
    READ_MOISTURE,
    READ_UV,
    READ_WATER_LEVEL,
    PUMP_ON,
    PUMP_OFF,
    SEND_SENSOR_DATA,
    SEND_NOTIFICATION,
    PAIR_WITH_MASTER,
    SYSTEM_SLEEP,
};

struct Task {
    TaskType type  = TaskType::NONE;
    uint32_t param = 0;    // PUMP_ON → duration_ms; SEND_NOTIFICATION → code
    bool     valid = false;
};

// ---------------------------------------------------------------------------
// Notification codes sent to master
// ---------------------------------------------------------------------------
enum NotificationCode : uint32_t {
    NOTIF_BOOT_OK      = 1,
    NOTIF_HW_READY     = 2,
    NOTIF_CFG_LOADED   = 3,
    NOTIF_LOW_WATER    = 4,
    NOTIF_HIGH_UV      = 5,
    NOTIF_PUMP_ON      = 6,
    NOTIF_PUMP_OFF     = 7,
    NOTIF_DRY_SOIL     = 8,
};

// ---------------------------------------------------------------------------
// Persistent configuration (stored in flash)
// ---------------------------------------------------------------------------
struct Config {
    uint32_t magic;                  // Must equal CONFIG_MAGIC to be valid
    char     ssid[33];
    char     password[65];
    char     server_url[129];
    uint16_t server_port;
    uint32_t sleep_interval_s;       // Wakeup period in seconds (default 3600)
    float    moisture_threshold_pct; // Trigger irrigation below this %
    float    uv_alert_threshold;     // Alert master above this UV index
    uint32_t pump_duration_ms;       // How long to run pump each cycle
    uint16_t moisture_dry_val;       // ADC reading at 0 % moisture (air)
    uint16_t moisture_wet_val;       // ADC reading at 100 % moisture (water)
    float    tank_height_cm;         // Used by ultrasonic level sensor
    uint32_t checksum;               // XOR of all preceding bytes
};

// ---------------------------------------------------------------------------
// ConfigManager — flash-backed config load / save
// ---------------------------------------------------------------------------
class ConfigManager {
public:
    ConfigManager();

    bool          load();
    bool          save(const Config &cfg);
    bool          is_valid()  const { return _valid; }
    const Config& get()       const { return _cfg; }
    Config&       get()             { return _cfg; }
    void          reset();

    static uint32_t compute_checksum(const Config &c);

private:
    Config _cfg;
    bool   _valid = false;
};

// ---------------------------------------------------------------------------
// TaskScheduler — FIFO circular buffer of Tasks
// ---------------------------------------------------------------------------
class TaskScheduler {
public:
    TaskScheduler();

    bool    push(Task t);
    bool    pop(Task &out);
    bool    empty() const { return _count == 0; }
    uint8_t size()  const { return _count; }
    void    clear();

private:
    Task    _queue[TASK_QUEUE_SIZE];
    uint8_t _head  = 0;
    uint8_t _tail  = 0;
    uint8_t _count = 0;
};

// ---------------------------------------------------------------------------
// WaterPump
// ---------------------------------------------------------------------------
class WaterPump {
public:
    WaterPump(uint8_t pin, bool active_high, bool use_pwm = false);
    ~WaterPump() = default;

    void on();
    void off();
    void toggle();
    void run_for(uint32_t duration_ms);
    void set_duty(uint8_t percent);

    bool     is_running()       const { return _running; }
    uint8_t  get_duty()         const { return _duty_pct; }
    uint32_t get_total_run_ms() const { return _total_run_ms; }

private:
    uint8_t  _pin;
    bool     _active_high;
    bool     _use_pwm;
    uint8_t  _pwm_slice;
    uint8_t  _pwm_channel;
    uint8_t  _duty_pct;
    bool     _running;
    uint32_t _total_run_ms;

    void _apply();
};

// ---------------------------------------------------------------------------
// MoistureSensor — capacitive / resistive probe on ADC
// ---------------------------------------------------------------------------
class MoistureSensor {
public:
    // dry_val = ADC reading in open air; wet_val = fully submerged in water
    MoistureSensor(uint8_t adc_pin,
                   uint16_t dry_val, uint16_t wet_val,
                   float low_limit_pct = 30.0f);
    ~MoistureSensor() = default;

    void     read();
    void     calibrate(uint16_t dry_val, uint16_t wet_val);

    float    get_percent() const { return _moisture_pct; }
    uint16_t get_raw()     const { return _raw; }
    bool     needs_water() const { return _needs_water; }

private:
    uint8_t  _adc_pin;
    uint8_t  _adc_ch;
    uint16_t _dry;
    uint16_t _wet;
    float    _low_limit;
    uint16_t _raw;
    float    _moisture_pct;
    bool     _needs_water;
};

// ---------------------------------------------------------------------------
// UvSensor — ML8511 (analog) or VEML6075 (I2C)
// ---------------------------------------------------------------------------
class UvSensor {
public:
    enum Mode { ANALOG = 0, I2C_MODE };

    explicit UvSensor(uint8_t adc_pin, float alert_threshold = 6.0f);
    UvSensor(i2c_inst_t *i2c, uint8_t i2c_addr, float alert_threshold = 6.0f);

    void  read();

    float get_index()  const { return _uv_index; }
    float get_uva()    const { return _uva; }
    float get_uvb()    const { return _uvb; }
    bool  high_alert() const { return _alert; }

private:
    Mode        _mode;
    uint8_t     _adc_pin;
    uint8_t     _adc_ch;
    i2c_inst_t *_i2c;
    uint8_t     _i2c_addr;
    float       _uv_index;
    float       _uva;
    float       _uvb;
    bool        _alert;
    float       _threshold;

    void _read_analog();
    void _read_i2c();
};

// ---------------------------------------------------------------------------
// WaterLevelSensor — digital float switch, analog probe, or HC-SR04
// ---------------------------------------------------------------------------
class WaterLevelSensor {
public:
    enum Type { DIGITAL = 0, ANALOG, ULTRASONIC };

    // Digital float switch
    WaterLevelSensor(uint8_t pin, bool active_high);
    // Analog resistive / capacitive probe
    WaterLevelSensor(uint8_t adc_pin, float tank_height_cm);
    // HC-SR04 ultrasonic
    WaterLevelSensor(uint8_t trig_pin, uint8_t echo_pin, float tank_height_cm);

    void  read();
    void  set_thresholds(float empty_pct, float full_pct);

    float    get_percent()  const { return _level_pct; }
    float    get_distance() const { return _distance_cm; }
    uint16_t get_raw_adc()  const { return _raw_adc; }
    bool     is_empty()     const { return _empty; }
    bool     is_full()      const { return _full; }

private:
    Type     _type;
    uint8_t  _data_pin;
    uint8_t  _adc_ch;
    uint8_t  _trig_pin;
    uint8_t  _echo_pin;
    bool     _active_high;
    float    _tank_height_cm;
    uint16_t _raw_adc;
    float    _distance_cm;
    float    _level_pct;
    bool     _empty;
    bool     _full;
    float    _empty_thresh = 10.0f;
    float    _full_thresh  = 90.0f;

    void  _read_digital();
    void  _read_analog();
    void  _read_ultrasonic();
    float _measure_ultrasonic_cm();
    void  _update_flags();
};

// ---------------------------------------------------------------------------
// WifiModule — ESP8266 / ESP32 over UART using Hayes AT commands
// ---------------------------------------------------------------------------
class WifiModule {
public:
    enum State { DISCONNECTED = 0, CONNECTING, CONNECTED, ERROR_STATE };

    WifiModule(uart_inst_t *uart,
               uint8_t tx_pin, uint8_t rx_pin,
               uint32_t baud = 115200);

    bool init();
    bool connect(const char *ssid, const char *password);
    void disconnect();
    void reset();

    // Send a null-terminated string payload over TCP to the configured server
    bool send(const char *payload);
    void set_server(const char *url, uint16_t port);

    // Pairing: put ESP8266 in AP + TCP server mode and wait for a config packet
    bool start_ap_mode(const char *ap_name);
    bool wait_for_config(Config &out_cfg, uint32_t timeout_ms);

    // Poll for an incoming command from master; returns bytes read (0 = none)
    int  read_command(char *buf, size_t max_len, uint32_t timeout_ms);

    State    get_state()       const { return _state; }
    bool     is_connected()    const { return _state == CONNECTED; }
    uint32_t last_tx_ms()      const { return _last_tx_ms; }
    bool     data_sent_ok()    const { return _sent_ok; }
    void     clear_sent_flag()       { _sent_ok = false; }

private:
    uart_inst_t *_uart;
    uint8_t      _tx_pin;
    uint8_t      _rx_pin;
    uint32_t     _baud;
    State        _state;
    char         _ssid[33];
    char         _password[65];
    char         _server_url[129];
    uint16_t     _server_port;
    uint32_t     _last_tx_ms;
    bool         _sent_ok;

    // Send cmd\r\n, scan response for `expected` substring within timeout_ms
    bool _at(const char *cmd, const char *expected, uint32_t timeout_ms);
    // Read one NL-terminated line; returns char count
    int  _read_line(char *buf, size_t max, uint32_t timeout_ms);
    void _flush_rx();
    bool _parse_config_line(const char *line, Config &out);
};

// ---------------------------------------------------------------------------
// SleepManager — low-power sleep with dual wakeup: RTC alarm or WAKE pin
// ---------------------------------------------------------------------------
class SleepManager {
public:
    SleepManager(uint8_t wake_pin, uint8_t sleep_signal_pin);

    // Switch to XOSC, assert SLEEP_SIGNAL, wait until RTC alarm fires or
    // wake_pin rises, then restore clocks.
    void enter_sleep(uint32_t duration_s);

    // True when the last wakeup was triggered by the wake pin (WiFi event)
    bool woken_by_wifi() const { return _woken_by_pin; }

private:
    uint8_t _wake_pin;
    uint8_t _sleep_signal_pin;
    bool    _woken_by_pin = false;

    static void _add_seconds(datetime_t &dt, uint32_t secs);
};

// ---------------------------------------------------------------------------
// IrrigationSystem — top-level coordinator
// ---------------------------------------------------------------------------
class IrrigationSystem {
public:
    IrrigationSystem();
    ~IrrigationSystem();

    // Blocking main loop — call once from main()
    void run();

private:
    ConfigManager    _config_mgr;
    TaskScheduler    _scheduler;
    SleepManager    *_sleep_mgr   = nullptr;
    WifiModule      *_wifi        = nullptr;
    WaterPump       *_pump        = nullptr;
    MoistureSensor  *_moisture    = nullptr;
    UvSensor        *_uv          = nullptr;
    WaterLevelSensor *_level      = nullptr;

    bool _hw_initialized = false;

    // Boot stages
    void _boot_sequence();
    void _pair_with_master();
    void _init_hardware();
    void _send_notification(uint32_t code);

    // Scheduler
    void _run_scheduler_loop();
    void _execute_task(const Task &t);
    void _queue_default_tasks();
    void _check_master_commands();
    void _enter_sleep();

    // Individual task handlers
    void _task_read_moisture();
    void _task_read_uv();
    void _task_read_water_level();
    void _task_pump_on(uint32_t duration_ms);
    void _task_pump_off();
    void _task_send_sensor_data();
};