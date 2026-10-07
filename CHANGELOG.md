# pico-mooncresta

An emulator of the arcade game Moon Cresta (Nichibutsu, 1980) for RP2350 boards with DVI/HDMI output.

# Changelog

## v0.1

Initial release.

- Moon Cresta with sound, on every RP2350 board of this family. PSRAM is not needed.
- The game ROMs are not included. Copy MAME's `mooncrst.zip` to `/roms/arcade/MOONCRESTA` on the SD card, as it is or unzipped. Missing files are named on screen.
- **Tate mode** setting: the picture is turned upright for a normal screen, or shown unrotated for a monitor turned on its side, either way round.
- A and B both fire, each with its own rapid fire setting.
- Can be started from pico-bootLoader, where it appears in the Arcade category.
