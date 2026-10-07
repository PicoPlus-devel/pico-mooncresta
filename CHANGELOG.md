# pico-mooncresta

An emulator of the arcade game Moon Cresta (Nichibutsu, 1980) for RP2350 boards with DVI/HDMI output.

# General Info

[Binaries for each configuration and PCB design are at the end of this page](#downloads___).

[See the readme for the supported boards, the game data and the controls](https://github.com/PicoPlus-devel/pico-mooncresta#readme)

> [!IMPORTANT]
> An **RP2350** board is required. The original RP2040 (Pico 1) is not supported.

# Changelog

## v0.1

Initial release.

- Moon Cresta with sound, on every RP2350 board of this family. PSRAM is not needed.
- The game ROMs are not included. Copy MAME's `mooncrst.zip` to `/roms/arcade/MOONCRESTA` on the SD card, as it is or unzipped. Missing files are named on screen.
- **Tate mode** setting: the picture is turned upright for a normal screen, or shown unrotated for a monitor turned on its side, either way round.
- A and B both fire, each with its own rapid fire setting.
- Can be started from pico-bootLoader, where it appears in the Arcade category.

<a name="downloads___"></a>
## Downloads by configuration

Binaries for each configuration are listed below. Only RP2350-based boards are supported; no RP2040 / Pico 1 binaries are provided.

Wiring and assembly are the same as for the NES emulator; see the [pico-infonesPlus documentation](https://github.com/PicoPlus-devel/pico-infonesPlus#setup) for the RP2350 boards listed below.

### Standalone boards

| Board | Binary |
|:--|:--|
| Adafruit Metro RP2350 | [picoMoonCresta_AdafruitMetroRP2350_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitMetroRP2350_arm.uf2) |
| Adafruit Fruit Jam | [picoMoonCresta_AdafruitFruitJam_arm_piousb.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitFruitJam_arm_piousb.uf2) |
| Adafruit Feather RP2350 with TLV320DAC3100 | [picoMoonCresta_AdafruitFeatherRP2350_TLV320DAC3100_arm_piousb.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitFeatherRP2350_TLV320DAC3100_arm_piousb.uf2) |
| Waveshare RP2350-PiZero | [picoMoonCresta_WaveShareRP2350PiZero_arm_piousb.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_WaveShareRP2350PiZero_arm_piousb.uf2) |

### Breadboard

| Board | Binary |
|:--|:--|
| Pico 2 | [picoMoonCresta_AdafruitDVISD_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitDVISD_pico2_arm.uf2) |
| Pimoroni Pico Plus 2 | [picoMoonCresta_AdafruitDVISD_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitDVISD_pico2_arm.uf2) |

### PicoNES PCB (PCB required)

| Board | Binary |
|:--|:--|
| Pico 2 / Pico 2 W | [picoMoonCresta_AdafruitDVISD_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitDVISD_pico2_arm.uf2) |
| Pimoroni Pico Plus 2 | [picoMoonCresta_AdafruitDVISD_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_AdafruitDVISD_pico2_arm.uf2) |

PCB: [pico_nesPCB_v2.6.zip](https://github.com/PicoPlus-devel/pico-infonesPlus/releases/latest/download/pico_nesPCB_v2.6.zip). [Readme](https://github.com/PicoPlus-devel/pico-infonesPlus#pcb-with-raspberry-pi-pico-or-pico-2-and-pimoroni-pico-plus-2)

3D-printed case: [thingiverse.com/thing:6689537](https://www.thingiverse.com/thing:6689537). When the board is fitted on male headers, use the latest top cover; the older covers assume a Pico soldered flat and leave no room for the USB cable.

### PicoNES Mini PCB, Waveshare RP2350-Zero (PCB required)

| Board | Binary |
|:--|:--|
| Waveshare RP2350-Zero | [picoMoonCresta_WaveShareRP2350ZeroWithPCB_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_WaveShareRP2350ZeroWithPCB_arm.uf2) |

PCB: [Gerber_PicoNES_Mini_PCB_v2.0.zip](https://github.com/PicoPlus-devel/pico-infonesPlus/releases/latest/download/Gerber_PicoNES_Mini_PCB_v2.0.zip)

3D-printed case: [thingiverse.com/thing:7041536](https://www.thingiverse.com/thing:7041536)

### PicoNES Micro PCB, Waveshare RP2350-USBA (PCB required)

[Binary](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_WaveShare2350USBA_arm_piousb.uf2)

PCB: [Gerber_PicoNES_Micro_v1.2.zip](https://github.com/PicoPlus-devel/pico-infonesPlus/releases/latest/download/Gerber_PicoNES_Micro_v1.2.zip)

[Build guide](https://www.instructables.com/PicoNES-RaspberryPi-Pico-Based-NES-Emulator/)

### Pimoroni Pico DV

| Board | Binary |
|:--|:--|
| Pico 2 / Pico 2 W | [picoMoonCresta_PimoroniDVI_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_PimoroniDVI_pico2_arm.uf2) |
| Pimoroni Pico Plus 2 | [picoMoonCresta_PimoroniDVI_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_PimoroniDVI_pico2_arm.uf2) |

### SpotPear HDMI

For more info about the SpotPear HDMI see https://spotpear.com/index/product/detail/id/1207.html and https://spotpear.com/index/study/detail/id/971.html.

| Board | Binary |
|:--|:--|
| Pico 2 / Pico 2 W | [picoMoonCresta_SpotpearHDMI_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_SpotpearHDMI_pico2_arm.uf2) |

### Murmulator M1

For more info about the Murmulator see https://murmulator.ru/.

| Board | Binary |
|:--|:--|
| Pico 2 / Pico 2 W | [picoMoonCresta_MurmulatorM1_pico2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_MurmulatorM1_pico2_arm.uf2) |

### Murmulator M2

For more info about the Murmulator see https://murmulator.ru/.

| Board | Binary |
|:--|:--|
| Murmulator M2 | [picoMoonCresta_MurmulatorM2_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_MurmulatorM2_arm.uf2) |

### Olimex RP2040-PICO-PC

| Board | Binary |
|:--|:--|
| Olimex RP2040-PICO-PC with a Pico 2 | [picoMoonCresta_OlimexPicoPC_arm.uf2](https://github.com/PicoPlus-devel/pico-mooncresta/releases/latest/download/picoMoonCresta_OlimexPicoPC_arm.uf2) |
