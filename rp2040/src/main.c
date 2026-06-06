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

// ===== Status LED =====
// RP2040-Zero: on-board WS2812 (NeoPixel) on GPIO16, driven via PIO -- a plain
// gpio_put() cannot speak the 800kHz NeoPixel protocol. Raspberry Pi Pico: a
// plain mono LED on GPIO25. States (see the main loop):
//   boot                 -> blue blink (you can see it powered up)
//   idle (no USB kbd)     -> solid red
//   USB keyboard mounted  -> solid green
//   key pressed           -> brief white flash
#ifndef RP2040_ZERO
#define RP2040_ZERO 1
#endif

// WS2812 PIO is needed both for the on-board NeoPixel (RP2040-Zero) and for the
// external status strip on the case -- include it unconditionally.
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

// ---- External WS2812 status STRIP on the case (visible status bar) ----------
// Daisy-chained WS2812 on STRIP_PIN, driven by its own PIO state machine; all
// LEDs mirror the on-board status LED's colour (a bright bar). Wiring:
//   STRIP_PIN (GPIO14) -> strip DIN  (3.3V data, direct from the RP2040)
//   strip GND         -> common GND
//   strip +V          -> ~4.3V: one silicon diode (1N4007) from the MSX +5V.
//     The 4-7V strip's VIH (~0.7*Vcc) then sits below 3.3V, so it accepts the
//     RP2040's 3.3V data with NO level shifter. (5V direct also works on many
//     4-7V strips; add the diode only if colours glitch.)
#define STRIP_PIN   14    // GP14: bottom-edge pad on the RP2040-Zero, easy to solder
#define STRIP_LEDS  8
static PIO  s_strip_pio  = pio0;
static uint s_strip_sm   = 1;     // on-board NeoPixel uses SM0; the strip uses SM1
static int  s_ws2812_off = -1;    // shared ws2812 program offset on pio0 (loaded once)

#if RP2040_ZERO
#define WS2812_PIN 16
static PIO  s_led_pio = pio0;
static uint s_led_sm  = 0;
static void status_led_init(void) {
    s_ws2812_off = pio_add_program(s_led_pio, &ws2812_program);
    ws2812_program_init(s_led_pio, s_led_sm, (uint)s_ws2812_off, WS2812_PIN, 800000.f, false);
}
// r,g,b in 0..255 -> WS2812 wants GRB, MSB-first, left-justified in 32 bits.
static inline void status_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    pio_sm_put_blocking(s_led_pio, s_led_sm, grb << 8u);
}
#else
#define PICO_LED_PIN 25
static void status_led_init(void) {
    gpio_init(PICO_LED_PIN);
    gpio_set_dir(PICO_LED_PIN, GPIO_OUT);
}
// Pico has a single mono LED: lit for any non-black colour.
static inline void status_led_rgb(uint8_t r, uint8_t g, uint8_t b) {
    gpio_put(PICO_LED_PIN, (r | g | b) ? 1 : 0);
}
#endif

// Set up the external strip's PIO state machine. Reuses the ws2812 program that
// status_led_init() already loaded on RP2040-Zero; loads it itself on a plain
// Pico. Must be called AFTER status_led_init().
static void strip_init(void) {
    if (s_ws2812_off < 0)
        s_ws2812_off = pio_add_program(s_strip_pio, &ws2812_program);
    ws2812_program_init(s_strip_pio, s_strip_sm, (uint)s_ws2812_off, STRIP_PIN, 800000.f, false);
}
// Fill all STRIP_LEDS with one colour (the status bar).
static inline void strip_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    for (int i = 0; i < STRIP_LEDS; i++)
        pio_sm_put_blocking(s_strip_pio, s_strip_sm, grb << 8u);
}

#define BUF_COUNT   4

// Full-matrix resync cadence (microseconds). Self-heals any dropped event.
#define RESYNC_INTERVAL_US 250000ull

// ============== Global Variables ==============

tusb_desc_device_t desc_device;
uint8_t buf_pool[BUF_COUNT][64];
uint8_t buf_owner[BUF_COUNT] = { 0 };
uint8_t isMounted = 0;       // set by the HID mount/unmount callbacks (usbin.c)
uint8_t kb_leds = 0;
uint8_t kb_modifiers = 0;
// NKRO scratch sizing only (the event model uses its own internal state in
// usbin.c). Kept so the NKRO expansion in tuh_hid_report_received_cb is
// unchanged; kb_report_receive() caps the effective key count safely.
uint8_t kb_keys[120] = {0};

// ============== Core 1 ==============

void core1_entry() {
    while (1) {
        sleep_ms(200);
    }
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

    status_led_init();
    strip_init();          // external 8-LED status strip on the case (GPIO14)

    // Launch Core 1
    multicore_launch_core1(core1_entry);

#ifdef DEBUG
    printf("Firmware Boot Done\n");
#endif

    // Boot indicator: blink blue a few times so it is visibly alive.
    for (int i = 0; i < 3; i++) {
        status_led_rgb(0, 0, 40); strip_rgb(0, 0, 40); sleep_ms(120);
        status_led_rgb(0, 0, 0);  strip_rgb(0, 0, 0);  sleep_ms(120);
    }

    tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);

    uint64_t next_resync = time_us_64() + RESYNC_INTERVAL_US;
    uint32_t led_prev = 0xFFFFFFFFu;

    while (1) {
        tuh_task();      // service USB host (fills the TX ring via callbacks)
        kb_tx_pump();    // drain TX ring into uart0 FIFO -- never blocks

        uint64_t now = time_us_64();

        joy_autofire_tick(now);   // advance autofire square wave + emit composed joy byte

        // Periodic full-matrix resync (also self-heals any dropped byte).
        if ((int64_t)(now - next_resync) >= 0) {
            kb_send_resync();
            joy_send_resync();   // re-emit both MSX joystick ports (0xB0 frames)
            kb_tx_pump();
            next_resync = now + RESYNC_INTERVAL_US;
        }

        // ---- Status LED: white (key/joystick) > yellow (gamepad) > green (keyboard) > red (idle) ----
        uint32_t led;
        if      ((uint64_t)(now - g_last_key_us) < 70000ull) led = 0x202020u;  // white flash on keypress / joystick activity
        else if (g_joy_mounted)                              led = 0x141400u;  // yellow: USB gamepad connected
        else if (isMounted)                                  led = 0x002000u;  // green: USB keyboard connected
        else                                                 led = 0x1A0000u;  // red: nothing connected
        if (led != led_prev) {
            status_led_rgb((uint8_t)(led >> 16), (uint8_t)(led >> 8), (uint8_t)led);
            strip_rgb     ((uint8_t)(led >> 16), (uint8_t)(led >> 8), (uint8_t)led);  // case strip mirrors status
            led_prev = led;
        }
    }

    return 0;
}
