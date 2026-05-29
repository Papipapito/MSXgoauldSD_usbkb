#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "bsp/board_api.h"
#include "tusb_config.h"
#include "class/hid/hid_host.h"
#include "usbin.h"

// ============== Defines ==============

#define KEYBOARD_REPORT_LEN 6

// Status LED pin. Default target is the Waveshare RP2040-Zero (WS2812 on
// GPIO16). Build with -DRP2040_ZERO=0 to target a Raspberry Pi Pico (LED on
// GPIO25). The define is forwarded from CMake; default here keeps the
// historical RP2040-Zero behaviour if it is ever compiled standalone.
#ifndef RP2040_ZERO
#define RP2040_ZERO 1
#endif
#if RP2040_ZERO
#define LED_PIN 16   // Waveshare RP2040-Zero on-board WS2812 data pin
#else
#define LED_PIN 25   // Raspberry Pi Pico on-board LED
#endif

#define BUF_COUNT   4

// Full-matrix resync cadence (microseconds). Self-heals any dropped event.
#define RESYNC_INTERVAL_US 250000ull

// ================= headers ===================

void led_blinking_task(void);
void print_device_descriptor(tuh_xfer_t* xfer);

// ============== Global Variables ==============

tusb_desc_device_t desc_device;
uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 };
uint8_t isMounted = 0;
uint8_t kb_leds = 0;
uint8_t kb_modifiers = 0;
// NKRO scratch sizing only (the event model uses its own internal state in
// usbin.c). Kept so the NKRO expansion in tuh_hid_report_received_cb is
// unchanged; kb_report_receive() caps the effective key count safely.
uint8_t kb_keys[120] = {0};

// ============== Function to run on Core 1 ==============

void core1_entry() {
    while (1) {
        // gpio_put(LED_PIN, 1);  // Turn LED on
        sleep_ms(200);         // Wait 500 ms
        // gpio_put(LED_PIN, 0);  // Turn LED off
        // sleep_ms(800);         // Wait 500 ms
    }
}

// ============== Timer Callback ==============

bool toggle_led_callback(repeating_timer_t *timer) {
    gpio_put(LED_PIN, isMounted);
    return true;
}

// ============== Main ==============

int main() {
    // stdio is routed to USB-CDC only (PICO_STDIO_UART disabled in CMakeLists),
    // so this never touches uart0. Any DEBUG printf goes over USB-CDC.
    stdio_init_all();

    // Dedicated keyboard link on uart0 (GPIO0 = TX -> FPGA pin 75). Must run
    // AFTER stdio_init_all() so our explicit pin mux owns GPIO0/uart0.
    kb_uart_init();

    tusb_init();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Launch Core 1
    multicore_launch_core1(core1_entry);

    repeating_timer_t timer;
    add_repeating_timer_ms(5, toggle_led_callback, NULL, &timer);

#ifdef DEBUG
    printf("Firmware Boot Done\n");
#endif

    // Firmware boot LED output
    gpio_put(LED_PIN, 1);
    sleep_ms(200);
    gpio_put(LED_PIN, 0);

    tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);

    uint64_t next_resync = time_us_64() + RESYNC_INTERVAL_US;

    while (1) {
        tuh_task();      // service USB host (fills the TX ring via callbacks)
        kb_tx_pump();    // drain TX ring into uart0 FIFO -- never blocks

        // Periodic full-matrix resync (also self-heals any dropped byte).
        uint64_t now = time_us_64();
        if ((int64_t)(now - next_resync) >= 0) {
            kb_send_resync();
            kb_tx_pump();
            next_resync = now + RESYNC_INTERVAL_US;
        }
    }

    return 0;
}
