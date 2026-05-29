# MSXgoauldSD_usbkb — MSX Goa'uld con teclado USB

> Fork de **[jabadiagm/MSXgoauldSD_tn20k](https://github.com/jabadiagm/MSXgoauldSD_tn20k)** que añade **teclado USB** (funciona en paralelo al teclado físico del MSX) mediante un **Raspberry Pi Pico / RP2040** conectado por UART.
>
> **Trabajo derivado del gran trabajo de:**
> - **jabadiagm** — MSX Goa'uld: motor MSX2+ en FPGA, placa y core. <https://github.com/jabadiagm/MSXgoauldSD_tn20k>
> - **Chandler-Klüser** — MSX Goa'uld *Guardian Angel*: firmware RP2040 host USB. <https://github.com/Chandler-Kluser/msx-goauld-ga> (a su vez basado en el parser HID de No0ne / pdaxrom).
>
> **Añade:** matriz de teclado USB virtual fusionada en el FPGA (sin tocar BIOS, slots, joysticks ni cartuchos), firmware RP2040 reescrito (make/break no bloqueante, modificadores, LED de estado WS2812) y layout **US⇄Español conmutable en caliente**. Código en [`rp2040/`](rp2040/) y [`fpga/`](fpga/); binarios en [Releases](../../releases). Licencia **GPLv3** (heredada).

---

# MSXgoauldSD_tn20k
MSX Goa'uld board with Tang Nano 20k and SD support

![V1.5_SMD](/pics/V1_5_smd.jpg)

MSX2+ engine in Z80 socket. It turns one MSX into an MSX2+ by replacing Z80 processor. FPGA in board contains: 
* Z80
* V9958 with hdmi output
* MSX2+ BIOS
* SD Card support + Nextor 2.1
* 4MB mapper
* 2MB megaram SCC
* RTC
* PSG
* OPLL
* Kanji Level 1 & 2
* Wifi support using ESP (experimental)


## Boards

Old boards V1.4 and V4.1 are compatible with Goa'uld SD. V3 is not supported, owners of V3 should use Goa'uld standard firmware (0.80).

New pcb V1.5 is same as 1.4, optimized for online PCB assembly. See [Assembly guide](/pcba/pcba.md)


## Slot map

![Slot map](/pics/mapa_slots5.png)

Mapper and megaram can be relocated to slots 1 or 2 using config menu.

## Megaram + Sofarun
Megaram is detected automatically by sofarun using default settings. When using other software you may need to indicate location, Slot 3-3 by default.


## Configuration
Config menu is showed pressing g during MSX logo. New improved menu is created by [nataliapc](https://github.com/nataliapc/msx_goauld_settings_menu)

![Config](/pics/config5.png)

* Enable Mapper: On by default. Disable when having compatibility issues or to use a different mapper
* Enable Megaram: On by default. Disable when having compatibility issues or to use a different megaram
* Enable SD: On by default. Disable when using an external SD mapper
* Mapper Slot: 3 by default. Change to 1 or 2 to get mapper in a not expanded slot. Physical slot will be disabled
* Megaram Slot: 3 by default. Change to 1 or 2 to get megaram in a not expanded slot. Physical slot will be disabled
* Ghost SCC: Off by default. Enable to get sound from an SCC cartridge located in physical slot 1 or 2
* Enable Scanlines: On by default. Disable to get a clean hdmi picture
* Compatible Mode: On by default. Enable for use with external cartridges, disable for internal SD use
* Save & Exit: store new config and continue, changes in mapper settings will be effective after pressing reset
* Save & Reset: store new config and make software reset, changes will be immediate

## Known issues
* Reset from config menu is not compatible with some hardware. Use physical reset button when possible
* Tape games fail: use poke -1,0
* Carnivore C2+ and Megaflashrom SCC+ SD are not compatible with Compatible mode (yes, I know)
* Wifi unreliable: place ESP at appropiate distance from Tang


## Flashing
Programming is done in two steps:
* Flash firmware Z80_goauld.fs  

![Flash1a](/pics/flashing1a.png)
![Flash1b](/pics/flashing1b.png)
* Flash bios pack. Set Operation = "exFlash C Bin Erase, Program thru GAO-Bridge" and Start Address = 0x200000  

![Flash2a](/pics/flashing2a.png)
![Flash2b](/pics/flashing2b.png)

> [!WARNING]
> Not yet fully working on all MSX!
>