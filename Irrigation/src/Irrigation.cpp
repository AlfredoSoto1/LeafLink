// =============================================================================
// Irrigation.cpp — LeafLink Irrigation Node
// Raspberry Pi Pico (RP2040)
// =============================================================================
#include "Irrigation.hpp"
#include <stdlib.h>
#include "hardware/watchdog.h"

// ---------------------------------------------------------------------------
// Global flags used by ISRs — must live in RAM (.data / .bss)
// ---------------------------------------------------------------------------
static volatile bool g_wake_by_pin = false;   // set by WAKE pin IRQ
static volatile bool g_wake_by_rtc = false;   // set by RTC alarm callback

static void _rtc_alarm_cb(void) {
    g_wake_by_rtc = true;
}

static void _wake_pin_irq(uint gpio, uint32_t events) {
    (void)gpio; (void)events;
    g_wake_by_pin = true;
}

// =============================================================================
// ConfigManager
// =============================================================================
ConfigManager::ConfigManager() {
    memset(&_cfg, 0, sizeof(_cfg));
}

// Compute a simple XOR-based checksum over every byte except the checksum field
uint32_t ConfigManager::compute_checksum(const Config &c) {
    const uint8_t *p   = reinterpret_cast<const uint8_t *>(&c);
    const uint8_t *end = p + offsetof(Config, checksum);
    uint32_t csum = 0;
    while (p < end) {
        csum ^= (uint32_t)(*p++);
    }
    return csum;
}

bool ConfigManager::load() {
    // Flash is memory-mapped at XIP_BASE; config lives at the last sector
    const Config *flash_ptr =
        reinterpret_cast<const Config *>(XIP_BASE + FLASH_CONFIG_OFFSET);

    memcpy(&_cfg, flash_ptr, sizeof(Config));

    if (_cfg.magic != CONFIG_MAGIC) {
        _valid = false;
        return false;
    }
    if (_cfg.checksum != compute_checksum(_cfg)) {
        _valid = false;
        return false;
    }

    _valid = true;
    return true;
}

bool ConfigManager::save(const Config &cfg) {
    // Work on a local copy so we can compute the checksum
    Config c = cfg;
    c.magic    = CONFIG_MAGIC;
    c.checksum = compute_checksum(c);

    // Flash writes must be page-aligned (256 B) and erased in sector blocks (4 KB)
    static uint8_t sector_buf[FLASH_SECTOR_SIZE];
    memset(sector_buf, 0xFF, sizeof(sector_buf));
    memcpy(sector_buf, &c, sizeof(Config));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CONFIG_OFFSET, sector_buf, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);

    // Verify the write
    const Config *verify =
        reinterpret_cast<const Config *>(XIP_BASE + FLASH_CONFIG_OFFSET);
    if (verify->magic != CONFIG_MAGIC ||
        verify->checksum != compute_checksum(*verify)) {
        return false;
    }

    _cfg   = c;
    _valid = true;
    return true;
}

void ConfigManager::reset() {
    static const uint8_t erase_buf[FLASH_SECTOR_SIZE] = {};
    // Write 0x00 over the sector so the magic check fails on next boot
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_CONFIG_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_CONFIG_OFFSET, erase_buf, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
    _valid = false;
}

// =============================================================================
// TaskScheduler  (circular FIFO)
// =============================================================================
TaskScheduler::TaskScheduler() {
    clear();
}

void TaskScheduler::clear() {
    _head  = 0;
    _tail  = 0;
    _count = 0;
    memset(_queue, 0, sizeof(_queue));
}

bool TaskScheduler::push(Task t) {
    if (_count >= TASK_QUEUE_SIZE) return false;
    t.valid = true;
    _queue[_tail] = t;
    _tail  = (_tail + 1) % TASK_QUEUE_SIZE;
    _count++;
    return true;
}

bool TaskScheduler::pop(Task &out) {
    if (_count == 0) return false;
    out   = _queue[_head];
    _head = (_head + 1) % TASK_QUEUE_SIZE;
    _count--;
    return true;
}

// =============================================================================
// WaterPump
// =============================================================================
WaterPump::WaterPump(uint8_t pin, bool active_high, bool use_pwm)
    : _pin(pin), _active_high(active_high), _use_pwm(use_pwm),
      _pwm_slice(0), _pwm_channel(0), _duty_pct(100),
      _running(false), _total_run_ms(0)
{
    if (_use_pwm) {
        gpio_set_function(_pin, GPIO_FUNC_PWM);
        _pwm_slice   = pwm_gpio_to_slice_num(_pin);
        _pwm_channel = pwm_gpio_to_channel(_pin);
        pwm_set_wrap(_pwm_slice, 999);          // 1000 steps → 0–100 %
        pwm_set_clkdiv(_pwm_slice, 125.0f);     // ~1 kHz at 125 MHz sys clk
        pwm_set_chan_level(_pwm_slice, _pwm_channel, 0);
        pwm_set_enabled(_pwm_slice, true);
    } else {
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_OUT);
        gpio_put(_pin, _active_high ? 0 : 1);  // ensure off
    }
}

void WaterPump::_apply() {
    if (_use_pwm) {
        uint16_t level = _running ? (uint16_t)(_duty_pct * 10u) : 0u;
        pwm_set_chan_level(_pwm_slice, _pwm_channel, level);
    } else {
        bool drive = _running ? _active_high : !_active_high;
        gpio_put(_pin, drive ? 1 : 0);
    }
}

void WaterPump::on() {
    _running = true;
    _apply();
}

void WaterPump::off() {
    _running = false;
    _apply();
}

void WaterPump::toggle() {
    _running ? off() : on();
}

void WaterPump::run_for(uint32_t duration_ms) {
    on();
    sleep_ms(duration_ms);
    off();
    _total_run_ms += duration_ms;
}

void WaterPump::set_duty(uint8_t percent) {
    if (percent > 100) percent = 100;
    _duty_pct = percent;
    if (_running) _apply();
}

// =============================================================================
// MoistureSensor
// =============================================================================
MoistureSensor::MoistureSensor(uint8_t adc_pin,
                                uint16_t dry_val, uint16_t wet_val,
                                float low_limit_pct)
    : _adc_pin(adc_pin), _adc_ch(adc_pin - 26u),
      _dry(dry_val), _wet(wet_val), _low_limit(low_limit_pct),
      _raw(0), _moisture_pct(0.0f), _needs_water(false)
{
    adc_init();
    adc_gpio_init(_adc_pin);
}

void MoistureSensor::calibrate(uint16_t dry_val, uint16_t wet_val) {
    _dry = dry_val;
    _wet = wet_val;
}

void MoistureSensor::read() {
    adc_select_input(_adc_ch);
    _raw = adc_read();

    // Higher ADC value = drier soil (capacitive sensor inverts signal)
    if (_dry <= _wet) {
        // Safety: if calibration is inverted just return 50 %
        _moisture_pct = 50.0f;
    } else {
        float pct = (float)(_dry - _raw) / (float)(_dry - _wet) * 100.0f;
        if (pct < 0.0f)   pct = 0.0f;
        if (pct > 100.0f) pct = 100.0f;
        _moisture_pct = pct;
    }

    _needs_water = (_moisture_pct < _low_limit);
}

// =============================================================================
// UvSensor
// =============================================================================
UvSensor::UvSensor(uint8_t adc_pin, float alert_threshold)
    : _mode(ANALOG), _adc_pin(adc_pin), _adc_ch(adc_pin - 26u),
      _i2c(nullptr), _i2c_addr(0),
      _uv_index(0.0f), _uva(0.0f), _uvb(0.0f),
      _alert(false), _threshold(alert_threshold)
{
    adc_init();
    adc_gpio_init(_adc_pin);
}

UvSensor::UvSensor(i2c_inst_t *i2c, uint8_t i2c_addr, float alert_threshold)
    : _mode(I2C_MODE), _adc_pin(0), _adc_ch(0),
      _i2c(i2c), _i2c_addr(i2c_addr),
      _uv_index(0.0f), _uva(0.0f), _uvb(0.0f),
      _alert(false), _threshold(alert_threshold)
{
}

void UvSensor::read() {
    if (_mode == ANALOG) _read_analog(); else _read_i2c();
    _alert = (_uv_index >= _threshold);
}

void UvSensor::_read_analog() {
    // ML8511: Vout ≈ 1.0 V at 0 UVI, ≈ 2.8 V at 15 UVI (3.3 V supply)
    adc_select_input(_adc_ch);
    uint16_t raw     = adc_read();
    float    voltage = raw * (3.3f / 4095.0f);
    float    idx     = (voltage - 1.0f) / 1.8f * 15.0f;
    if (idx < 0.0f) idx = 0.0f;
    _uv_index = idx;
    _uva      = idx;   // ML8511 gives a combined UV reading
    _uvb      = 0.0f;
}

void UvSensor::_read_i2c() {
    // VEML6075 — registers: 0x07 UVA, 0x09 UVB, 0x0A UVCOMP1, 0x0B UVCOMP2
    auto read_reg = [&](uint8_t reg) -> uint16_t {
        uint8_t buf[2] = {0, 0};
        i2c_write_blocking(_i2c, _i2c_addr, &reg, 1, true);
        i2c_read_blocking(_i2c, _i2c_addr, buf, 2, false);
        return (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    };

    // Power on: write 0x00 to config register 0x00
    uint8_t cfg_on[2] = {0x00, 0x00};
    i2c_write_blocking(_i2c, _i2c_addr, cfg_on, 2, false);
    sleep_ms(100); // integration time

    uint16_t uva_raw   = read_reg(0x07);
    uint16_t uvb_raw   = read_reg(0x09);
    uint16_t comp1_raw = read_reg(0x0A);
    uint16_t comp2_raw = read_reg(0x0B);

    // VEML6075 application note coefficients
    const float UVA_A = 2.22f, UVA_B = 1.33f;
    const float UVB_C = 2.95f, UVB_D = 1.75f;
    const float UVA_RESP = 0.001461f, UVB_RESP = 0.002591f;

    float uva_comp = (float)uva_raw - UVA_A * comp1_raw - UVA_B * comp2_raw;
    float uvb_comp = (float)uvb_raw - UVB_C * comp1_raw - UVB_D * comp2_raw;
    if (uva_comp < 0.0f) uva_comp = 0.0f;
    if (uvb_comp < 0.0f) uvb_comp = 0.0f;

    _uva      = uva_comp;
    _uvb      = uvb_comp;
    _uv_index = ((uva_comp * UVA_RESP) + (uvb_comp * UVB_RESP)) / 2.0f;
}

// =============================================================================
// WaterLevelSensor
// =============================================================================
WaterLevelSensor::WaterLevelSensor(uint8_t pin, bool active_high)
    : _type(DIGITAL), _data_pin(pin), _adc_ch(0),
      _trig_pin(0), _echo_pin(0), _active_high(active_high),
      _tank_height_cm(0.0f), _raw_adc(0), _distance_cm(0.0f),
      _level_pct(0.0f), _empty(false), _full(false)
{
    gpio_init(_data_pin);
    gpio_set_dir(_data_pin, GPIO_IN);
    gpio_pull_down(_data_pin);
}

WaterLevelSensor::WaterLevelSensor(uint8_t adc_pin, float tank_height_cm)
    : _type(ANALOG), _data_pin(adc_pin), _adc_ch(adc_pin - 26u),
      _trig_pin(0), _echo_pin(0), _active_high(false),
      _tank_height_cm(tank_height_cm), _raw_adc(0), _distance_cm(0.0f),
      _level_pct(0.0f), _empty(false), _full(false)
{
    adc_init();
    adc_gpio_init(adc_pin);
}

WaterLevelSensor::WaterLevelSensor(uint8_t trig_pin, uint8_t echo_pin,
                                    float tank_height_cm)
    : _type(ULTRASONIC), _data_pin(0), _adc_ch(0),
      _trig_pin(trig_pin), _echo_pin(echo_pin), _active_high(false),
      _tank_height_cm(tank_height_cm), _raw_adc(0), _distance_cm(0.0f),
      _level_pct(0.0f), _empty(false), _full(false)
{
    gpio_init(_trig_pin);
    gpio_set_dir(_trig_pin, GPIO_OUT);
    gpio_put(_trig_pin, 0);

    gpio_init(_echo_pin);
    gpio_set_dir(_echo_pin, GPIO_IN);
}

void WaterLevelSensor::set_thresholds(float empty_pct, float full_pct) {
    _empty_thresh = empty_pct;
    _full_thresh  = full_pct;
}

void WaterLevelSensor::read() {
    switch (_type) {
        case DIGITAL:    _read_digital();    break;
        case ANALOG:     _read_analog();     break;
        case ULTRASONIC: _read_ultrasonic(); break;
    }
    _update_flags();
}

void WaterLevelSensor::_read_digital() {
    bool pin_state = gpio_get(_data_pin);
    _level_pct = (_active_high ? pin_state : !pin_state) ? 100.0f : 0.0f;
}

void WaterLevelSensor::_read_analog() {
    adc_select_input(_adc_ch);
    _raw_adc   = adc_read();
    _level_pct = (float)_raw_adc / 4095.0f * 100.0f;
}

void WaterLevelSensor::_read_ultrasonic() {
    _distance_cm = _measure_ultrasonic_cm();
    if (_distance_cm < 0.0f || _tank_height_cm <= 0.0f) {
        _level_pct = 0.0f;
        return;
    }
    // Sensor points down: smaller distance = more water
    float pct = (1.0f - _distance_cm / _tank_height_cm) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    _level_pct = pct;
}

float WaterLevelSensor::_measure_ultrasonic_cm() {
    // Send 10 µs trigger pulse
    gpio_put(_trig_pin, 1);
    sleep_us(10);
    gpio_put(_trig_pin, 0);

    // Wait for echo to go HIGH (timeout 30 ms)
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (!gpio_get(_echo_pin)) {
        if (to_ms_since_boot(get_absolute_time()) - t0 > 30) return -1.0f;
    }

    // Measure echo pulse width
    uint64_t start = time_us_64();
    while (gpio_get(_echo_pin)) {
        if (time_us_64() - start > 30000ULL) break;
    }
    uint64_t duration_us = time_us_64() - start;

    // distance = duration / 58  (cm, round-trip at ~340 m/s)
    return (float)duration_us / 58.0f;
}

void WaterLevelSensor::_update_flags() {
    _empty = (_level_pct <= _empty_thresh);
    _full  = (_level_pct >= _full_thresh);
}

// =============================================================================
// SleepManager
// =============================================================================
SleepManager::SleepManager(uint8_t wake_pin, uint8_t sleep_signal_pin)
    : _wake_pin(wake_pin), _sleep_signal_pin(sleep_signal_pin)
{
    // SLEEP_SIGNAL_PIN: output, default LOW (awake)
    gpio_init(_sleep_signal_pin);
    gpio_set_dir(_sleep_signal_pin, GPIO_OUT);
    gpio_put(_sleep_signal_pin, 0);

    // WAKE_PIN: input with pull-down, triggered by ESP8266
    gpio_init(_wake_pin);
    gpio_set_dir(_wake_pin, GPIO_IN);
    gpio_pull_down(_wake_pin);
}

void SleepManager::_add_seconds(datetime_t &dt, uint32_t secs) {
    uint32_t t;
    t      = (uint32_t)dt.sec + secs;
    dt.sec = (int8_t)(t % 60);
    t      = (uint32_t)dt.min + t / 60;
    dt.min = (int8_t)(t % 60);
    t       = (uint32_t)dt.hour + t / 60;
    dt.hour = (int8_t)(t % 24);
    uint32_t extra_days = t / 24;
    dt.day  = (int8_t)(dt.day + extra_days);
    dt.dotw = (int8_t)((dt.dotw + extra_days) % 7);
    // Simple day overflow — clamp to 28 (sufficient for hourly sleep)
    if (dt.day > 28) dt.day = 1;
}

void SleepManager::enter_sleep(uint32_t duration_s) {
    _woken_by_pin = false;
    g_wake_by_pin = false;
    g_wake_by_rtc = false;

    // ----- 1. Prepare all GPIO for sleep (safe low-power state) -----
    // Pump output off
    gpio_init(PIN_PUMP_OUT);
    gpio_set_dir(PIN_PUMP_OUT, GPIO_IN);

    // ADC pins — just leave as inputs (ADC peripheral will be off)
    gpio_init(PIN_MOISTURE_ADC);
    gpio_set_dir(PIN_MOISTURE_ADC, GPIO_IN);
    gpio_disable_pulls(PIN_MOISTURE_ADC);

    gpio_init(PIN_UV_ADC);
    gpio_set_dir(PIN_UV_ADC, GPIO_IN);
    gpio_disable_pulls(PIN_UV_ADC);

    // Ultrasonic trigger off
    gpio_init(PIN_LEVEL_TRIG);
    gpio_set_dir(PIN_LEVEL_TRIG, GPIO_IN);
    gpio_disable_pulls(PIN_LEVEL_TRIG);

    gpio_init(PIN_LEVEL_ECHO);
    gpio_set_dir(PIN_LEVEL_ECHO, GPIO_IN);
    gpio_disable_pulls(PIN_LEVEL_ECHO);

    // ----- 2. Tell ESP8266 we are sleeping -----
    gpio_put(_sleep_signal_pin, 1);
    sleep_ms(10); // give ESP8266 time to notice

    // ----- 3. Configure WAKE_PIN interrupt -----
    gpio_set_irq_enabled_with_callback(
        _wake_pin, GPIO_IRQ_EDGE_RISE, true, _wake_pin_irq);

    // ----- 4. Set RTC alarm -----
    datetime_t now;
    rtc_get_datetime(&now);
    datetime_t alarm = now;
    _add_seconds(alarm, duration_s);
    rtc_set_alarm(&alarm, _rtc_alarm_cb);

    // ----- 5. WFI loop — CPU sleeps between interrupts (low-power) -----
    while (!g_wake_by_pin && !g_wake_by_rtc) {
        __wfi();
    }

    _woken_by_pin = g_wake_by_pin;

    // ----- 6. Disable wake sources -----
    gpio_set_irq_enabled(_wake_pin, GPIO_IRQ_EDGE_RISE, false);
    rtc_disable_alarm();

    // ----- 7. De-assert sleep signal -----
    gpio_put(_sleep_signal_pin, 0);
}

// =============================================================================
// IrrigationSystem
// =============================================================================
IrrigationSystem::IrrigationSystem() {
    // SleepManager can be constructed without config
    _sleep_mgr = new SleepManager(PIN_WAKE, PIN_SLEEP_SIGNAL);
}

IrrigationSystem::~IrrigationSystem() {
    delete _sleep_mgr;
    delete _wifi;
    delete _pump;
    delete _moisture;
    delete _uv;
    delete _level;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
void IrrigationSystem::run() {
    _boot_sequence();

    while (true) {
        // Let the WiFi module drain any queued commands from master
        _check_master_commands();

        // Fill the task queue with the default sensor/report cycle
        _queue_default_tasks();

        // Execute all queued tasks (tasks may push more tasks)
        _run_scheduler_loop();

        // Go to sleep until the next cycle
        _enter_sleep();
    }
}

// ---------------------------------------------------------------------------
// Boot: config check → pair if needed → init hardware → notify master
// ---------------------------------------------------------------------------
void IrrigationSystem::_boot_sequence() {
    stdio_init_all();

    // Initialize RTC with a base epoch so alarms work
    rtc_init();
    datetime_t base = {
        .year  = 2020, .month = 1,  .day  = 1,
        .dotw  = 3,    .hour  = 0,  .min  = 0, .sec = 0
    };
    rtc_set_datetime(&base);
    sleep_us(64); // settling time

    // Bring up the WiFi UART before anything else
    _wifi = new WifiModule(uart0, PIN_WIFI_TX, PIN_WIFI_RX, 115200);
    _wifi->init();

    if (!_config_mgr.load()) {
        // No valid config — enter pairing mode
        _pair_with_master();
    }

    _init_hardware();

    _send_notification(NOTIF_BOOT_OK);
    _send_notification(NOTIF_HW_READY);
    _send_notification(NOTIF_CFG_LOADED);
}

// ---------------------------------------------------------------------------
// Pairing: ESP8266 AP + TCP server, wait for Config packet from master
// ---------------------------------------------------------------------------
void IrrigationSystem::_pair_with_master() {
    // Build a unique-ish AP name from the last byte of the flash chip ID
    char ap_name[24];
    snprintf(ap_name, sizeof(ap_name), "LeafLink_%04X",
             (unsigned)(XIP_BASE & 0xFFFFu));

    _wifi->start_ap_mode(ap_name);

    Config new_cfg;
    memset(&new_cfg, 0, sizeof(new_cfg));

    // Block until master sends a valid config (no timeout — stays in pairing)
    while (!_wifi->wait_for_config(new_cfg, 0xFFFFFFFFu)) {
        sleep_ms(500);
    }

    _config_mgr.save(new_cfg);
}

// ---------------------------------------------------------------------------
// Hardware initialisation using the (now valid) config
// ---------------------------------------------------------------------------
void IrrigationSystem::_init_hardware() {
    const Config &cfg = _config_mgr.get();

    // Pump — GPIO, active HIGH, no PWM
    _pump = new WaterPump(PIN_PUMP_OUT, true, false);

    // Moisture sensor
    _moisture = new MoistureSensor(
        PIN_MOISTURE_ADC,
        cfg.moisture_dry_val  ? cfg.moisture_dry_val  : 2900u,
        cfg.moisture_wet_val  ? cfg.moisture_wet_val  : 1200u,
        cfg.moisture_threshold_pct > 0.0f ? cfg.moisture_threshold_pct : 30.0f
    );

    // UV sensor — analog ML8511
    _uv = new UvSensor(
        PIN_UV_ADC,
        cfg.uv_alert_threshold > 0.0f ? cfg.uv_alert_threshold : 6.0f
    );

    // Water level — HC-SR04 ultrasonic
    _level = new WaterLevelSensor(
        PIN_LEVEL_TRIG, PIN_LEVEL_ECHO,
        cfg.tank_height_cm > 0.0f ? cfg.tank_height_cm : 30.0f
    );

    // Connect WiFi to the configured network
    _wifi->set_server(cfg.server_url, cfg.server_port);
    _wifi->connect(cfg.ssid, cfg.password);

    _hw_initialized = true;
}

// ---------------------------------------------------------------------------
// Send a notification code to master
// ---------------------------------------------------------------------------
void IrrigationSystem::_send_notification(uint32_t code) {
    if (!_wifi || !_wifi->is_connected()) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "NOTIF:code=%lu\n", (unsigned long)code);
    _wifi->send(buf);
}

// ---------------------------------------------------------------------------
// Scheduler loop — drains the queue, tasks may push new tasks
// ---------------------------------------------------------------------------
void IrrigationSystem::_run_scheduler_loop() {
    Task t;
    // Guard against runaway task chains
    uint32_t iterations = 0;
    while (!_scheduler.empty() && iterations < 64u) {
        if (_scheduler.pop(t)) {
            _execute_task(t);
        }
        iterations++;
    }
}

void IrrigationSystem::_execute_task(const Task &t) {
    switch (t.type) {
        case TaskType::READ_MOISTURE:     _task_read_moisture();               break;
        case TaskType::READ_UV:           _task_read_uv();                     break;
        case TaskType::READ_WATER_LEVEL:  _task_read_water_level();            break;
        case TaskType::PUMP_ON:           _task_pump_on(t.param);              break;
        case TaskType::PUMP_OFF:          _task_pump_off();                    break;
        case TaskType::SEND_SENSOR_DATA:  _task_send_sensor_data();            break;
        case TaskType::SEND_NOTIFICATION: _send_notification(t.param);         break;
        case TaskType::PAIR_WITH_MASTER:  _pair_with_master();                 break;
        case TaskType::SYSTEM_SLEEP:      /* handled by _enter_sleep() */      break;
        default:                                                                break;
    }
}

// ---------------------------------------------------------------------------
// Default task set loaded at the start of each wake cycle
// ---------------------------------------------------------------------------
void IrrigationSystem::_queue_default_tasks() {
    _scheduler.push({ TaskType::READ_MOISTURE,    0, true });
    _scheduler.push({ TaskType::READ_UV,          0, true });
    _scheduler.push({ TaskType::READ_WATER_LEVEL, 0, true });
    _scheduler.push({ TaskType::SEND_SENSOR_DATA, 0, true });
}

// ---------------------------------------------------------------------------
// Poll WiFi for incoming commands from master
// ---------------------------------------------------------------------------
void IrrigationSystem::_check_master_commands() {
    if (!_wifi) return;

    char cmd[256];
    int  n = _wifi->read_command(cmd, sizeof(cmd), 200);
    if (n <= 0) return;

    // Simple command dispatch
    if (strncmp(cmd, "PUMP_ON:", 8) == 0) {
        uint32_t ms = (uint32_t)atoi(cmd + 8);
        if (ms == 0) ms = _config_mgr.get().pump_duration_ms;
        _scheduler.push({ TaskType::PUMP_ON, ms, true });
    } else if (strncmp(cmd, "PUMP_OFF", 8) == 0) {
        _scheduler.push({ TaskType::PUMP_OFF, 0, true });
    } else if (strncmp(cmd, "PAIR", 4) == 0) {
        _scheduler.push({ TaskType::PAIR_WITH_MASTER, 0, true });
    } else if (strncmp(cmd, "RESET_CFG", 9) == 0) {
        _config_mgr.reset();
        // Reboot
        watchdog_reboot(0, SRAM_END, 10);
    } else if (strncmp(cmd, "READ_NOW", 8) == 0) {
        _queue_default_tasks();
    }
}

// ---------------------------------------------------------------------------
// Enter sleep for the configured interval
// ---------------------------------------------------------------------------
void IrrigationSystem::_enter_sleep() {
    uint32_t interval = _config_mgr.is_valid()
                      ? _config_mgr.get().sleep_interval_s
                      : 3600u;
    if (interval < 10u) interval = 10u; // safety floor

    _sleep_mgr->enter_sleep(interval);

    // After wake: re-initialise hardware peripherals if woken by WiFi
    // (If woken by RTC the normal cycle continues; if by WiFi we also
    //  want to re-init so everything is in a known state.)
    if (_hw_initialized) {
        _init_hardware();
    }
}

// ---------------------------------------------------------------------------
// Task: read moisture, queue pump if dry
// ---------------------------------------------------------------------------
void IrrigationSystem::_task_read_moisture() {
    if (!_moisture) return;
    _moisture->read();

    if (_moisture->needs_water()) {
        uint32_t dur = _config_mgr.is_valid()
                     ? _config_mgr.get().pump_duration_ms
                     : 5000u;
        _scheduler.push({ TaskType::SEND_NOTIFICATION, NOTIF_DRY_SOIL, true });
        _scheduler.push({ TaskType::PUMP_ON, dur, true });
    }
}

// ---------------------------------------------------------------------------
// Task: read UV, queue alert if too high
// ---------------------------------------------------------------------------
void IrrigationSystem::_task_read_uv() {
    if (!_uv) return;
    _uv->read();

    if (_uv->high_alert()) {
        _scheduler.push({ TaskType::SEND_NOTIFICATION, NOTIF_HIGH_UV, true });
    }
}

// ---------------------------------------------------------------------------
// Task: read water level, queue alert if empty
// ---------------------------------------------------------------------------
void IrrigationSystem::_task_read_water_level() {
    if (!_level) return;
    _level->read();

    if (_level->is_empty()) {
        _scheduler.push({ TaskType::SEND_NOTIFICATION, NOTIF_LOW_WATER, true });
        // Make sure we don't run the pump on an empty tank
        // Remove any pending PUMP_ON from the queue by clearing and re-loading
        // only the tasks that are safe to run.
        _scheduler.clear();
        _scheduler.push({ TaskType::SEND_SENSOR_DATA, 0, true });
    }
}

// ---------------------------------------------------------------------------
// Task: turn pump on for duration_ms, then queue pump off + notification
// ---------------------------------------------------------------------------
void IrrigationSystem::_task_pump_on(uint32_t duration_ms) {
    if (!_pump) return;
    if (duration_ms == 0) duration_ms = 5000u;
    _send_notification(NOTIF_PUMP_ON);
    _pump->run_for(duration_ms); // blocking — pump stays on for duration
    _scheduler.push({ TaskType::SEND_NOTIFICATION, NOTIF_PUMP_OFF, true });
}

// ---------------------------------------------------------------------------
// Task: force pump off immediately
// ---------------------------------------------------------------------------
void IrrigationSystem::_task_pump_off() {
    if (!_pump) return;
    _pump->off();
    _send_notification(NOTIF_PUMP_OFF);
}

// ---------------------------------------------------------------------------
// Task: pack sensor readings and send to master
// ---------------------------------------------------------------------------
void IrrigationSystem::_task_send_sensor_data() {
    if (!_wifi || !_wifi->is_connected()) return;

    float moisture = _moisture ? _moisture->get_percent() : -1.0f;
    float uv       = _uv      ? _uv->get_index()         : -1.0f;
    float level    = _level   ? _level->get_percent()     : -1.0f;
    int   pump_on  = (_pump && _pump->is_running()) ? 1 : 0;

    char buf[192];
    snprintf(buf, sizeof(buf),
             "DATA:moisture=%.1f,uv=%.2f,level=%.1f,pump=%d\n",
             moisture, uv, level, pump_on);

    _wifi->send(buf);
}

// =============================================================================
// main
// =============================================================================
int main() {
    IrrigationSystem system;
    system.run();
    return 0; // never reached
}
