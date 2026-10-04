# Optional USB keyboard + joystick (RP2040)

An RP2040 (Waveshare RP2040-Zero or Raspberry Pi Pico) acts as a USB **host** for a keyboard and a gamepad and
sends their state to the FPGA over **one wire** (UART, Tang Nano 20K pin 75). The FPGA AND-merges it into the MSX
keyboard-matrix read (PPI `0xA9`) and the PSG joystick read (`0xA2`, register 14), so the USB devices work
**alongside** the real keyboard and joysticks. It is meant for MSX machines without a keyboard, or just for comfort.

It is **optional and off by default**: with it disabled the core behaves exactly as before.

## Enabling it in the FPGA

1. In `fpga/top.v`, uncomment `` `define ENABLE_USB_KBD ``.
2. Build as usual (`fpga/Z80_goauld.gprj` already lists `src/kbd_uart_rx.v`; `tang9k.cst` already maps pin 75
   with a pull-up, so the line idles high when no RP2040 is connected).

The FPGA side is `fpga/src/kbd_uart_rx.v` (UART receiver, virtual 11x8 matrix, joystick state, link-loss watchdog)
plus a small block in `fpga/top.v` that watches the PPI and PSG ports and merges the result into `cpu_din`. It only
observes the bus and never drives it. The snoops use a registered copy of the I/O cycle, so the extra logic does not
load the T80 `RD`/`IORQ` nets that feed the half-cycle wait FSM.

## Features

- **USB keyboard**, US / International layout (matches the international MSX2+ BIOS), make/break events plus a
  periodic full-matrix resync, modifiers Shift / Ctrl / Graph (Left Alt) / Code (Right Alt). USB F1..F5 = MSX F1..F5 and
  USB F6..F10 = MSX F6..F10 (Shift + F1..F5). STOP is on Scroll Lock.
- **USB joystick / gamepad** on MSX joystick port 1: D-pad / hat / left stick, button 1 = trigger A, button 2 =
  trigger B, buttons 3 / 4 = autofire (10 Hz) for A / B. Generic **HID** pads and **XInput** (Xbox) pads.
- Keyboard and gamepad at the same time need a **powered USB hub** (the RP2040-Zero has one USB port).
- Wired pads only: 2.4 GHz dongles usually enumerate too late for the RP2040 host; DualShock 4 / PS-mode pads use
  a report format the basic HID parser does not decode (use XInput mode if the pad has it).
- **Status LED**: the RP2040-Zero on-board WS2812 (blue blink at boot, red = nothing connected, green = keyboard,
  yellow = gamepad, white flash = activity). Optional 8-LED WS2812 strip on **GP26**, one signal per LED
  (power, keyboard, gamepad, typing, fire A, fire B, direction, heartbeat) -- see `src/main.c`.

## Wiring

```
RP2040 GP15 (PIO-UART TX) ->  Tang Nano 20K pin 75
RP2040 GND                ->  Tang Nano 20K GND   (common ground, required)
RP2040 VBUS / 5V          ->  MSX +5V
USB keyboard / gamepad    ->  RP2040 USB port (powered hub for both at once)
```

UART **115200 8N1**, data flows RP2040 -> FPGA only, 3.3 V LVCMOS at both ends (no level shifter).
Optional LED strip: GP26 -> strip DIN, GND -> GND, MSX +5V -> strip 5V (through a 1N4007 if the colours glitch).

## Building the firmware

Needs the [pico-sdk](https://github.com/raspberrypi/pico-sdk) (with TinyUSB) and `PICO_SDK_PATH` pointing to it:

```
cmake -B build -G Ninja && cmake --build build      # RP2040-Zero (default)
cmake -B build -G Ninja -DRP2040_ZERO=0 && cmake --build build      # Raspberry Pi Pico
```

Flash: hold BOOTSEL, plug the RP2040 into a PC and copy the `.uf2` to the `RPI-RP2` drive.

## UART protocol (RP2040 -> FPGA)

| Bytes | Meaning |
|---|---|
| `0x90 <cell>` / `0xA0 <cell>` | key make / break, `cell = 0x80 \| bit << 4 \| row` (MSX matrix row 0..10, bit 0..7) |
| `0xFE m0..m10 0xFF` | full-matrix resync (11 active-low rows), sent every 250 ms |
| `0xB0 <port> <state>` | joystick state, active-high (bit0 R, 1 L, 2 D, 3 U, 4 A, 5 B) |
| `0xC0 <version>` | firmware version announce (informational) |
| `0x01`..`0x04` | hot-key commands (scanlines, reset, OSD, F11 = turbo); decoded but left unconnected in `top.v` |

If no byte arrives for about 1 s the FPGA releases every key and the joystick.

## Credits and licences

- Firmware based on [Chandler-Kluser / msx-goauld-ga](https://github.com/Chandler-Kluser/msx-goauld-ga)
  (Guardian Angel), GPLv3 -- see `LICENSE`.
- HID report parser by No0ne / pdaxrom, MIT (notice in `src/usbin.c`).
- XInput host driver: [Ryzee119 / tusb_xinput](https://github.com/Ryzee119/tusb_xinput), MIT (notice in
  `src/xinput_host.c` / `inc/xinput_host.h`).
- Keyboard/joystick integration for the Goa'uld: Papipapito.
