// #include "Tasks.hpp"
// #include "TaskScheduler.hpp"
// #include "AppContext.hpp"
// #include <cstdio>

// // ---------------------------------------------------------------------------
// // Timer callback — fires every 60 seconds on the timer IRQ
// // Sets a flag so the main loop knows to wake and drain the queue
// // ---------------------------------------------------------------------------
// static volatile bool g_timer_fired = false;

// static bool timer_callback(repeating_timer_t *rt) {
//   g_timer_fired = true;
//   return true;
// }

// static void toggle_inboard_led() {
//   gpio_put(PICO_DEFAULT_LED_PIN, 1);
//   sleep_ms(500);
//   gpio_put(PICO_DEFAULT_LED_PIN, 0);
// }

// int main() {
//   stdio_init_all();
//   sleep_ms(2000);

//   // -------------------------------------------------------------------------
//   // 1 — Construct the application context with all components
//   // -------------------------------------------------------------------------
//   TaskScheduler scheduler;

//   AppContext context = {
//     .wifi      = WifiController(),
//     .pump      = PumpController(),

//     .sensor    = SensorController(),
//     .storage   = StorageController(),
//     .scheduler = &scheduler
//   };

//   // -------------------------------------------------------------------------
//   // 2 — Schedule the initial boot task. This will load config, then schedule
//   // sensor reads and other tasks as needed.
//   // -------------------------------------------------------------------------
//   context.scheduler->schedule(Tasks::boot_os);

//   // -------------------------------------------------------------------------
//   // 3 — Set up LED and repeating 60-second timer
//   // -------------------------------------------------------------------------
//   gpio_init(PICO_DEFAULT_LED_PIN);
//   gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

//   repeating_timer_t timer;
//   // add_repeating_timer_ms(-60000, timer_callback, nullptr, &timer);
//   add_repeating_timer_ms(-10000, timer_callback, nullptr, &timer);

//   // -------------------------------------------------------------------------
//   // Main loop — sleep until the timer fires, then drain the task queue
//   // -------------------------------------------------------------------------
//   while (true) {
//     // Deep sleep: CPU halts, only wakes on interrupt (timer IRQ, etc.)
//     // __wfi();

//     // // go back to sleep if the timer wasn't the reason we woke up (spurious wake, or other IRQ)
//     // if (!g_timer_fired) {
//     //   continue;
//     // }
//     // g_timer_fired = false;

//     // Process all tasks in the queue. Tasks can schedule more tasks, 
//     // so keep popping until it's empty.
//     while (!context.scheduler->empty()) {
//       auto task = context.scheduler->pop();
//       if (task != nullptr) {
//         task(context);
//         toggle_inboard_led();
//       }
//     }

//     // After processing all tasks, schedule the next sensor read cycle
//     context.scheduler->schedule(Tasks::wakeup_os);
//   }

//   return 0;
// }


#include <stdio.h>
#include <string>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#define WIFI_UART uart0

#define WIFI_TX_PIN 0
#define WIFI_RX_PIN 1
#define WIFI_ENABLE_PIN 3

#define PAIR_BUTTON_PIN 11

#define WIFI_BAUD 115200

const char* AP_SSID = "PICO_PAIR_DEVICE";
const char* AP_PASSWORD = "12345678";
const int TCP_PORT = 5000;

void wifi_enable(bool enabled) {
    gpio_put(WIFI_ENABLE_PIN, enabled ? 1 : 0);
}

void wifi_power_cycle() {
    printf("Power cycling ESP8266...\n");

    wifi_enable(false);
    sleep_ms(1000);

    wifi_enable(true);
    sleep_ms(3000);
}

void uart_send_raw(const std::string& data) {
    uart_write_blocking(WIFI_UART, reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
}

void uart_send_line(const std::string& command) {
    uart_send_raw(command + "\r\n");
}

std::string uart_read_for(uint32_t timeout_ms) {
    std::string response;
    absolute_time_t end = make_timeout_time_ms(timeout_ms);

    while (!time_reached(end)) {
        while (uart_is_readable(WIFI_UART)) {
            char c = uart_getc(WIFI_UART);
            response += c;
        }
        sleep_ms(5);
    }

    return response;
}

std::string send_at(const std::string& command, uint32_t timeout_ms = 1000) {
    printf(">> %s\n", command.c_str());

    uart_send_line(command);

    std::string response = uart_read_for(timeout_ms);

    printf("%s\n", response.c_str());

    return response;
}

bool wait_for_ok(const std::string& command, uint32_t timeout_ms = 1000) {
    std::string response = send_at(command, timeout_ms);
    return response.find("OK") != std::string::npos;
}

void esp8266_pairing_mode() {
    printf("Starting ESP8266 pairing mode...\n");

    wifi_power_cycle();

    send_at("AT", 1000);
    send_at("ATE0", 1000);

    // Reset ESP8266
    send_at("AT+RST", 3000);
    // AT+RST reboots the module, restoring echo to default (on).
    // Re-disable echo before sending further commands.
    send_at("ATE0", 1000);

    // Mode 2 = SoftAP mode
    send_at("AT+CWMODE_DEF=2", 1000);

    // Optional: set AP IP address
    send_at("AT+CIPAP_DEF=\"192.168.4.1\"", 1000);

    // Configure WiFi AP.
    // Format:
    // AT+CWSAP_DEF="ssid","password",channel,encryption
    //
    // encryption:
    // 0 = open
    // 3 = WPA2_PSK
    std::string apCommand =
        "AT+CWSAP_DEF=\"" +
        std::string(AP_SSID) +
        "\",\"" +
        std::string(AP_PASSWORD) +
        "\",5,3";

    send_at(apCommand, 1500);

    // Allow multiple TCP connections
    send_at("AT+CIPMUX=1", 1000);

    // Start TCP server
    send_at("AT+CIPSERVER=1," + std::to_string(TCP_PORT), 1000);

    printf("PAIRING MODE READY\n");
    printf("SSID: %s\n", AP_SSID);
    printf("PASS: %s\n", AP_PASSWORD);
    printf("PORT: %d\n", TCP_PORT);
}

int extract_connection_id(const std::string& data) {
    size_t start = data.find("+IPD,");
    if (start == std::string::npos) {
        return -1;
    }

    start += 5;

    size_t comma = data.find(",", start);
    if (comma == std::string::npos) {
        return -1;
    }

    std::string id = data.substr(start, comma - start);

    for (char c : id) {
        if (c < '0' || c > '9') {
            return -1;
        }
    }
    if (id.empty()) {
        return -1;
    }

    return std::stoi(id);
}

void send_tcp(int connection_id, const std::string& message) {
    std::string command =
        "AT+CIPSEND=" +
        std::to_string(connection_id) +
        "," +
        std::to_string(message.length());

    std::string response = send_at(command, 1000);

    if (response.find(">") != std::string::npos || response.find("OK") != std::string::npos) {
        uart_send_raw(message);
        sleep_ms(300);

        std::string sendResponse = uart_read_for(1000);
        printf("%s\n", sendResponse.c_str());
    } else {
        printf("ESP8266 not ready to send data\n");
    }
}

void handle_wifi_data(const std::string& data) {
    printf("ESP8266 DATA:\n%s\n", data.c_str());

    if (data.find("CONNECT") != std::string::npos) {
        printf("MASTER CONNECTED\n");
    }

    if (data.find("CLOSED") != std::string::npos) {
        printf("MASTER DISCONNECTED\n");
    }

    if (data.find("+IPD,") != std::string::npos) {
        int connection_id = extract_connection_id(data);

        if (connection_id >= 0) {
            printf("Received data from master on connection %d\n", connection_id);

            send_tcp(connection_id, "ACK_FROM_PICO\n");
        }
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);

    uart_init(WIFI_UART, WIFI_BAUD);
    gpio_set_function(WIFI_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(WIFI_RX_PIN, GPIO_FUNC_UART);

    gpio_init(WIFI_ENABLE_PIN);
    gpio_set_dir(WIFI_ENABLE_PIN, GPIO_OUT);
    gpio_put(WIFI_ENABLE_PIN, 0);

    gpio_init(PAIR_BUTTON_PIN);
    gpio_set_dir(PAIR_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(PAIR_BUTTON_PIN);

    esp8266_pairing_mode();

    while (true) {
        // if (gpio_get(PAIR_BUTTON_PIN) == 0) {
        //     printf("Pairing button pressed\n");

        //     esp8266_pairing_mode();

        //     while (gpio_get(PAIR_BUTTON_PIN) == 0) {
        //         sleep_ms(20);
        //     }

        //     sleep_ms(300);
        // }

        if (uart_is_readable(WIFI_UART)) {
            std::string data;

            absolute_time_t end = make_timeout_time_ms(100);

            while (!time_reached(end)) {
                while (uart_is_readable(WIFI_UART)) {
                    char c = uart_getc(WIFI_UART);
                    data += c;
                }
            }

            if (!data.empty()) {
                handle_wifi_data(data);
            }
        }

        sleep_ms(10);
    }
}



// // #include <stdio.h>
// // #include <string>
// // #include "pico/stdlib.h"
// // #include "hardware/uart.h"

// // #define WIFI_UART uart0

// // #define WIFI_TX_PIN 0
// // #define WIFI_RX_PIN 1
// // #define WIFI_EN_PIN 3

// // #define WIFI_BAUD 115200

// // void esp_enable() {
// //     gpio_put(WIFI_EN_PIN, 0);
// //     sleep_ms(1000);

// //     gpio_put(WIFI_EN_PIN, 1);
// //     sleep_ms(3000);
// // }

// // void uart_send_command(const char* command) {
// //     uart_puts(WIFI_UART, command);
// //     uart_puts(WIFI_UART, "\r\n");
// // }

// // std::string uart_read_response(uint32_t timeout_ms) {
// //     std::string response;

// //     absolute_time_t end_time = make_timeout_time_ms(timeout_ms);

// //     while (!time_reached(end_time)) {
// //         while (uart_is_readable(WIFI_UART)) {
// //             char c = uart_getc(WIFI_UART);
// //             response += c;
// //         }

// //         sleep_ms(10);
// //     }

// //     return response;
// // }

// // void send_at_and_print(const char* command) {
// //     printf("\nSending: %s\n", command);

// //     uart_send_command(command);

// //     std::string response = uart_read_response(2000);

// //     printf("Response:\n%s\n", response.c_str());

// //     if (response.find("OK") != std::string::npos) {
// //         printf("RESULT: ESP-01 has AT firmware and is responding.\n");
// //     } else {
// //         printf("RESULT: No OK response.\n");
// //     }
// // }

// // int main() {
// //     stdio_init_all();
// //     sleep_ms(4000);

// //     printf("Starting ESP-01 AT firmware test...\n");

// //     uart_init(WIFI_UART, WIFI_BAUD);
// //     gpio_set_function(WIFI_TX_PIN, GPIO_FUNC_UART);
// //     gpio_set_function(WIFI_RX_PIN, GPIO_FUNC_UART);

// //     gpio_init(WIFI_EN_PIN);
// //     gpio_set_dir(WIFI_EN_PIN, GPIO_OUT);

// //     esp_enable();

// //     send_at_and_print("AT");
// //     send_at_and_print("AT+GMR");

// //     while (true) {
// //         sleep_ms(1000);
// //     }
// // }
