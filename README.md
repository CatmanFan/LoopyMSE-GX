## LoopyMSE-GX
An unfinished work-in-progress/POC fork of [LoopyMSE](https://github.com/LoopyMSE/LoopyMSE) (Casio Loopy emulator) for the Wii/GameCube.

The original emulator uses SDL/SDL2 so it might be easier to port although currently it does not output any video/sound and several lines of code which caused freezing/exceptions have been commented out. Any contributions/assistance with the project to get it to working status are welcome.

Original README follows below.

# LoopyMSE
A Casio Loopy emulator. WIP, plays commercial games with sound.

Features:
- Runs all commercial games including Magical Shop
- High-level printer emulation (prints saved as BMP images)
- Sound emulation (unfinished, see below)
- High-level PCM expansion audio emulation
- Keyboard and controller input support
- Screenshots (saved as BMP images)
- Available on Mac, Win, Linux

Features still TODO:
- GUI menu for in-app configuration
- Mouse emulation
- Internal "demo" music used by some games
- Improved / low-level printer emulation

## Builds

Rolling builds are available for every commit on [Github Actions](../../actions/).

## Usage
See the [Readme](assets/README.md) (included in builds).

## Credits and Special Thanks
* PSI - original (upstream) [LoopyMSE](https://github.com/PSI-Rockin/LoopyMSE) project
* kasami - reverse engineering, accuracy improvements, bug fixes, sound & printer implementation, BIOS dumping
* partlyhuman - continued development, testing, automated builds, visual and quality-of-life improvements
* UBCH discord server - documentation and archival, translations, moral support
