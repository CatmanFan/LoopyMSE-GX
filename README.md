## LoopyMSE-GX
An experimental fork of [LoopyMSE](https://github.com/LoopyMSE/LoopyMSE) (Casio Loopy emulator) for the Wii/GameCube.

Note that performance is still slow.

### Usage
Place BIOS (bios.bin) and sound BIOS (soundbios.bin) in the same directory as the app's boot.dol. ROMs must be placed in a "roms" subdirectory (e.g. apps/LoopyMSE-GX/roms) or, to autoboot the ROM by default, as rom.bin alongside boot.dol. The emulator will not run if bios.bin or ROM(s) cannot be found!

Latest build is compiled with each commit and can be downloaded from [Github Actions](../../actions/).

### To-Do
- Code translation (dynarec), use [Seta GX](https://github.com/fadedled/seta-gx) as a reference?
- Fix cropping issue when changing between 240/224-line modes?

### Credits
Credits to PSI, kasami and partlyhuman for their work on the original emulator.

### License
The original project and this fork are both licensed under GPL-3.0.