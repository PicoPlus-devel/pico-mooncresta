# pico-mooncresta

**pico-mooncresta** is an emulator of the arcade game **Moon Cresta** (Nichibutsu, 1980) for RP2350-based microcontroller boards, with video and audio over DVI/HDMI. It emulates the original board, which is built on Galaxian hardware: the Zilog Z80 CPU, the scrolling tile layer, the sprites, shells and missiles, the starfield, the colour PROM and the discrete sound circuits. The emulation is a port of the Galaxian driver and the Z80 core of [MAME](https://www.mamedev.org/).

It uses the same menu, display, audio and controller framework as this family of emulators:

- NES: [pico-infonesPlus](https://github.com/PicoPlus-devel/pico-infonesPlus)
- Super Nintendo: [pico-snesPlus](https://github.com/PicoPlus-devel/pico-snesPlus)
- Sega Master System / Game Gear: [pico-smsplus](https://github.com/PicoPlus-devel/pico-smsplus)
- Game Boy / Game Boy Color: [pico-peanutGB](https://github.com/PicoPlus-devel/pico-peanutGB)
- Sega Mega Drive / Genesis: [pico-genesisPlus](https://github.com/PicoPlus-devel/pico-genesisPlus)
- OutRun: [pico-outrun](https://github.com/PicoPlus-devel/pico-outrun)
- Phoenix: [pico-phoenix](https://github.com/PicoPlus-devel/pico-phoenix)

pico-mooncresta runs standalone, or as an application of [pico-bootLoader](https://github.com/PicoPlus-devel/pico-bootLoader), where it appears in the **Arcade** category.

**The Moon Cresta ROM set is not included and must be supplied by the user.** It is copyright Nichibutsu and is not distributed with this project. See [Game data](#game-data).

***

## Screenshots

The game as the emulator draws it, at twice the original size, with *Tate mode* off. For the two tate orientations, see [Display and tate mode](#display-and-tate-mode).

<table>
  <tr>
    <td><img width="320" alt="Title screen: Trip to the space war, Moon Cresta, and the copyright notice" src="docs/screenshots/title.png" /></td>
    <td><img width="320" alt="Four eye-shaped aliens above the player's rocket" src="docs/screenshots/eyes.png" /></td>
  </tr>
  <tr>
    <td align="center">Title screen</td>
    <td align="center">Waves 1 and 2: the eyes, which split in two when hit</td>
  </tr>
  <tr>
    <td><img width="320" alt="Docking: the upper stage descends towards the lower stage, with the docking time counting down" src="docs/screenshots/docking.png" /></td>
    <td><img width="320" alt="Insect-like aliens attacking in a V formation" src="docs/screenshots/flies.png" /></td>
  </tr>
  <tr>
    <td align="center">Docking with the next stage of the rocket</td>
    <td align="center">The flies attack in formation</td>
  </tr>
</table>

***

## Game data

Required is the MAME **`mooncrst`** set: Moon Cresta (Nichibutsu). It consists of 13 files, about 24 KB in total. The program ROMs of this set are encrypted; they are decrypted when loaded.

Copy it to the folder **`/roms/arcade/MOONCRESTA`** on the SD card, in either form:

- **`mooncrst.zip` as it is.** Merged, split and non-merged sets all work: in a merged set the clone sets in the subfolders of the archive are ignored.
- **The files, unzipped.** A subfolder `/roms/arcade/MOONCRESTA/mooncrst`, which unzipping commonly produces, is searched as well.

Files are recognised by their size and CRC32, so their names do not matter, and a corrupt or different file is never loaded. The board creates the folder `/roms/arcade/MOONCRESTA` on first start.

When the set is missing or incomplete, the board shows a screen that names the missing files. **SELECT + START** opens the settings menu from that screen; its *USB drive mode* shows the SD card on a computer, so the ROM set can be copied to it without removing the card. After leaving the menu, the board restarts and looks for the set again.

The ROM sets of the clones and bootlegs (Gremlin, the USA and UK versions, Super Moon Cresta, Eagle, Fantazia and others) are not supported.

***

## Supported hardware

pico-mooncresta runs on every RP2350 configuration of this family. It requires neither PSRAM nor HSTX: the emulated board is small enough for the internal SRAM, and both video back-ends are supported. RP2040 boards are not supported.

| HW_CONFIG | Hardware | Video | Binary |
| --- | --- | --- | --- |
| 1 | Pimoroni Pico DV Demo Base with a Raspberry Pi Pico 2 | PicoDVI | `picoMoonCresta_PimoroniDVI_pico2_arm.uf2` |
| 2 | Adafruit DVI Breakout and microSD breakout with a Raspberry Pi Pico 2, or the PicoNES PCB | HSTX | `picoMoonCresta_AdafruitDVISD_pico2_arm.uf2` |
| 5 | Adafruit Metro RP2350 | HSTX | `picoMoonCresta_AdafruitMetroRP2350_arm.uf2` |
| 6 | Waveshare RP2350-Zero with custom PCB | PicoDVI | `picoMoonCresta_WaveShareRP2350ZeroWithPCB_arm.uf2` |
| 7 | Waveshare RP2350-PiZero | PicoDVI | `picoMoonCresta_WaveShareRP2350PiZero_arm_piousb.uf2` |
| 8 | Adafruit Fruit Jam | HSTX | `picoMoonCresta_AdafruitFruitJam_arm_piousb.uf2` |
| 9 | Waveshare RP2350-USB-A | PicoDVI | `picoMoonCresta_WaveShare2350USBA_arm_piousb.uf2` |
| 10 | Spotpear HDMI with a Raspberry Pi Pico 2 | PicoDVI | `picoMoonCresta_SpotpearHDMI_pico2_arm.uf2` |
| 12 | Murmulator M1 with a Raspberry Pi Pico 2 | PicoDVI | `picoMoonCresta_MurmulatorM1_pico2_arm.uf2` |
| 13 | Murmulator M2 | HSTX | `picoMoonCresta_MurmulatorM2_arm.uf2` |
| 14 | Adafruit Feather RP2350 with HSTX Port and TLV320DAC3100 I2S DAC | HSTX | `picoMoonCresta_AdafruitFeatherRP2350_TLV320DAC3100_arm_piousb.uf2` |
| 15 | Olimex RP2040-PICO-PC with a Raspberry Pi Pico 2 | HSTX | `picoMoonCresta_OlimexPicoPC_arm.uf2` |

For wiring and assembly instructions, see the setup sections of the [pico-infonesPlus README](https://github.com/PicoPlus-devel/pico-infonesPlus#setup). To flash a board, hold BOOTSEL while connecting it over USB, then copy the `.uf2` file onto the USB drive that appears.

Audio is sent over HDMI. Boards with an I2S DAC (configurations 8, 13 and 14) can use it instead through the *External Audio* setting; on the Fruit Jam, plugging in headphones selects it automatically. Configuration 15 also plays the sound on its PWM audio jack.

***

## Controls

Moon Cresta has a two-way joystick and one fire button. Two players take turns on the same controls.

| Controller | Moon Cresta |
| --- | --- |
| D-pad left / right | Move the ship |
| A (or X) | Fire |
| B (or Y) | Fire |
| SELECT | Insert a coin (on release) |
| START | 1 player start |
| D-pad up | 2 player start |
| START on a second USB controller | 2 player start |
| START + A | Frame rate display on/off |
| SELECT + START | Settings menu |

USB game controllers, NES and SNES controllers on the GPIO ports and the Wii Classic controller are supported, as in the other emulators of this family. A and B both press the fire button. Each has its own rapid fire setting, so one of them can fire single shots and the other fire automatically. The coin is inserted when SELECT is released, and only when no other button was pressed while it was held, so opening the settings menu does not insert a coin. While START is held, A switches the frame rate display (top left, in the border) on or off instead of firing; the same setting is also in the settings menu.

The game is set to its factory defaults: an upright cabinet, a bonus ship at 30,000 points, English text, and 1 coin for 1 credit.

***

## Display and tate mode

Moon Cresta was made for a monitor mounted on its side: the original picture is 224 pixels wide and 256 pixels tall. The *Tate mode* setting chooses how it is shown:

| Tate mode | Picture | For |
| --- | --- | --- |
| Off (default) | Turned upright and shown in the centre of the screen. The 256 lines are fitted into the 240 of the screen by leaving out every sixteenth line. | A normal monitor or TV |
| Bottom left | The original 256 x 224 picture, unrotated and pixel exact. | A monitor turned 90 degrees clockwise, so that its bottom edge is on the left |
| Bottom right | The same picture, rotated by 180 degrees. | A monitor turned 90 degrees counter-clockwise, so that its bottom edge is on the right |

The setting takes effect immediately and is saved with the other settings. The settings menu itself is not rotated.

<table>
  <tr>
    <td><img width="320" alt="Tate mode Bottom left: the eyes wave shown unrotated, the score along the left edge" src="docs/screenshots/tate-bottom-left.png" /></td>
    <td><img width="320" alt="Tate mode Bottom right: the same picture rotated by 180 degrees, the score along the right edge" src="docs/screenshots/tate-bottom-right.png" /></td>
  </tr>
  <tr>
    <td align="center">Bottom left</td>
    <td align="center">Bottom right</td>
  </tr>
</table>

Both pictures appear upright once the monitor is turned as described in the table above.

***

## Settings menu

**SELECT + START** opens the settings menu during the game. Besides *Tate mode*, it offers the screen mode with or without scanlines, the FPS overlay, audio on/off, rapid fire on A and on B, the external audio output, the Fruit Jam volume and VU meter, *Reset Game*, the controller test, BOOTSEL mode and USB drive mode. When started from pico-bootLoader, it also offers *Return to emulator selection menu*.

The settings are stored on the SD card in `/settings_ARC.dat`, a file shared by all arcade games of this family. The game has no save states, and high scores are not kept after a reset or power cycle.

***

## Building from source

Requirements: the Raspberry Pi Pico SDK (`PICO_SDK_PATH`), Pico-PIO-USB (`PICO_PIO_USB_PATH`) for the configurations that use it, the `arm-none-eabi` toolchain and `picotool`.

```sh
git clone --recursive https://github.com/PicoPlus-devel/pico-mooncresta.git
cd pico-mooncresta
./bld.sh -c 8 -2        # one configuration: HW_CONFIG 8 (Fruit Jam)
./bld.sh -b -c 8 -2     # the same, built for pico-bootLoader (releases_bl/)
./buildAll.sh           # every supported configuration (releases/)
./bld.sh -h             # all options
```

Every configuration must be built with `-2` (RP2350).

The instruction loop of the Z80 core, `mooncresta/z80_ops.h`, is generated from MAME's opcode list `mooncresta/z80.lst` and is checked in, so the build does not need Python. After a change to the list, regenerate it with `tools/z80gen.py mooncresta/z80.lst mooncresta/z80_ops.h`. The built-in screensaver image (`DefaultSS444.c`, `DefaultSS555.c`) and the pico-bootLoader menu artwork are built by `tools/mkartwork.py`. The pictures of the game in this artwork are captured from your own ROM set by the host harness, and then laid out in the style of the bootloader's two themes.

```sh
hosttest/build.sh
tools/capture_footage.sh ~/roms/arcade/MOONCRESTA /tmp/mcr_footage
tools/mkartwork.py --footage /tmp/mcr_footage --bootloader ../pico-bootLoader/emu/assets
```

`capture_footage.sh` keeps one frame per second of a scripted game in `/tmp/mcr_footage/frames`, and copies five of them to the names `mkartwork.py` reads (`badge`, `left`, `middle`, `right` and `screensaver.ppm`). Copy another frame over one of these names to change what that part of the artwork shows. Afterwards, run the bootloader's `tools/png2raw.py` on the two tiles to rebuild their `.444`/`.555` caches.

### Host test harness

The emulated board in `mooncresta/` is plain C without Pico dependencies, and also builds for Linux:

```sh
hosttest/build.sh
./hosttest/mcr_host ~/roms/arcade/MOONCRESTA 3600 300 hosttest/out --coin 1800 --start 1860 --play 1920 --wav hosttest/out/game.wav
python3 hosttest/ppm2png.py hosttest/out/frame_02400.ppm
./hosttest/zex_host zexdoc.com zexall.com
```

`mcr_host` loads the ROM set through the same loader as the firmware, runs the given number of frames with scripted input, writes every Nth frame as a 320 x 240 PPM in the chosen orientation (`--tate 0|1|2`) and the sound as a WAV file. `zex_host` runs the ZEXDOC and ZEXALL instruction exercisers on the Z80 core; the test programs are not part of this repository.

***

## Credits and licence

- Galaxian and Moon Cresta driver and video emulation: the MAME project, by Aaron Giles, Couriersud, Stephane Humbert and Robbbert. Sound: Couriersud, with the discrete sound modules of K. Wilkins and Derrick Renaud. Z80 CPU core: Juergen Buchmueller and Andrei I. Holub. These parts are under the BSD-3-Clause licence; see [mooncresta/LICENSE](mooncresta/LICENSE).
- miniz (inflate and CRC32): Rich Geldreich and others; see the licence text in [third_party/miniz/miniz.c](third_party/miniz/miniz.c).
- Framework, menu, display, audio and controller support: [pico_shared](https://github.com/PicoPlus-devel/pico_shared) and the sister projects listed above.

pico-mooncresta is distributed under the GNU General Public License v3.0; see [LICENSE](LICENSE).

Moon Cresta is copyright 1980 Nichibutsu (Nihon Bussan). This project is not affiliated with Nichibutsu or with the MAME project.
