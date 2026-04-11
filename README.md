## LoopyMSE-GX (LoopyMSE for Wii)
An experimental port of [LoopyMSE](https://github.com/LoopyMSE/LoopyMSE) (Casio Loopy emulator) to the Wii/GameCube. Note that performance is still slow, and the majority of the code remains untested, although it can run both commercial and homebrew games.

### Usage
Place BIOS (bios.bin) and sound BIOS (soundbios.bin) in the same directory as the app's boot.dol. ROMs must be placed in a "roms" subdirectory (e.g. apps/LoopyMSE-GX/roms) or, to autoboot the ROM by default, as rom.bin alongside boot.dol. The emulator will not run if bios.bin or ROM(s) cannot be found!

Supports Wii Remote, Classic Controller, Wii U GamePad and GameCube controller. To exit during emulation, press HOME (Wiimote, Classic Controller or GamePad) or Start+Z (GameCube controller).

Latest (debug) build is compiled with each commit and can be downloaded from [Github Actions](../../actions/). The latest stable build can be downloaded from [Releases](../../releases/).

### To-Do
- Code translation (dynarec), use [Seta GX](https://github.com/fadedled/seta-gx) as a reference?
- Fix cropping issue when changing between 240/224-line modes?

Collaboration on this port would be welcome.

### Credits
Credits to **PSI, kasami and partlyhuman** for their work on the original emulator.

### License
The original project and this fork are both licensed under GPL-3.0.