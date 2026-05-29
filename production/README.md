# Production binaries — MSXgoauldSD_usbkb

Prebuilt binaries for the USB keyboard + joystick fork. See the **[main README](../README.md)** for full documentation.

## Files
| File | What it is |
|---|---|
| `MSXgoauldSD_usbkb.fs` | FPGA bitstream (Tang Nano 20K, GW2AR-18): MSX2+ core + USB keyboard/joystick merge. |
| `rp2040_keyboard.uf2` | RP2040 firmware (RP2040-Zero, WS2812 LED on GPIO16): USB keyboard (US/International) + USB joystick (HID + XInput). |

> The `.uf2` targets the **RP2040-Zero**. For a plain Raspberry Pi Pico, rebuild with `-DRP2040_ZERO=0` (mono LED on GPIO25).

## Status LED
| Colour | Meaning |
|---|---|
| 🔵 Blue blink (3×) at power-up | Firmware booted OK |
| 🔴 Solid red | Alive — nothing connected |
| 🟢 Solid green | USB **keyboard** connected |
| 🟡 Solid yellow | USB **gamepad** connected |
| ⚪ White flash | Key press / joystick activity |

## Flashing
1. **FPGA:** Gowin Programmer → load `MSXgoauldSD_usbkb.fs` into the Tang Nano 20K (SRAM for a quick test, or the external config flash to persist). **Your BIOS pack at `0x200000` is untouched.** From a scratch install, also flash the BIOS pack to the external SPI flash at `0x200000` (*exFlash C Bin Erase, Program thru GAO-Bridge*).
2. **RP2040:** hold **BOOTSEL**, plug USB-C → the `RPI-RP2` drive appears → copy `rp2040_keyboard.uf2` onto it. The blue boot blink = it's running.

## Wiring (one data wire + ground + 5 V)
```
RP2040 GPIO0 (UART0 TX)  ->  Tang Nano 20K pin 75
RP2040 GND               ->  Tang Nano 20K GND   (common ground, required)
RP2040 VBUS / 5V         ->  MSX +5V
USB keyboard / gamepad   ->  RP2040 USB host port (use a hub for both at once)
```
UART **115200 8N1**, data flows RP2040 → FPGA only; 3.3 V LVCMOS both ends (no level shifter).

## USB keyboard
US / International layout (matches the international MSX2+ BIOS), working **alongside** the MSX's physical keyboard.

## USB joystick / gamepad (HID + XInput)
Wired pads → **MSX joystick port 1**, merged with the real joysticks: directions + **A / button 1 = fire 1**, **B / button 2 = fire 2**.
- **XInput** — Xbox 360 / One / Series and XInput-mode pads. ✅ tested on hardware.
- **Generic HID** — pads with standard X/Y axes, a hat and buttons.
- **Limitations:** wireless 2.4 GHz dongles usually won't enumerate on the RP2040's lightweight USB host → use **wired**; some DualShock 4 / PS-mode HID pads aren't decoded yet → use the pad in **XInput mode** if it has one.

## WiFi (ESP-01S, optional)
ESP-01S **TX → pin 77**, **RX → pin 79**, **VCC + CH_PD → 3.3 V**, GND → GND. Firmware: the **OCM** build of **[ducasp / MSX-Development ESPFW1.4](https://github.com/ducasp/MSX-Development/releases/tag/ESPFW1.4)**; baud **859372**, I/O ports **0x06 / 0x07** (UNAPI TCP/IP). Coexists with the RP2040 (pin 75).

> The Tang Nano's on-board BL616 is **not** used in this design. The MSX **BIOS pack** and any third-party module firmware are **not** included here (copyright).
