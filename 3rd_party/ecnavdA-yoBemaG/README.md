# ecnavdA-yoBemaG
A WIP Game Boy Advance emulator.

## Major missing features
* Proper transparency and alpha blending
* Correct prefetch buffer
* EEPROM saves
* All extra cartridge hardware
* Serial
* [HLE BIOS] Accurate timings
* [HLE BIOS] Correct boot state
* [HLE BIOS] Non-IRQ exception handling
* [HLE BIOS] SWIs 0x10-0x2A

## Build Requirements
* Any modern 64 bit Linux distribution or Windows
* `cmake`, `make`, and SDL2>=20.0.18 development libraries
### Linux
* `gcc` or `clang`
* GTK3 development libraries (for `nativefiledialog-extended`)
### Windows
* Only `clang-cl` will work. No MSVC.

## Usage Instructions
Download the executable from Releases or compile:
* `git clone https://github.com/KellanClark/ecnavdA-yoBemaG.git --recurse-submodules`
* `cd ecnavdA-yoBemaG`
* `mkdir build`
* `cd build`
* `cmake ..`
* `make`

Run the executable named `ecnavda-yobemag`. If the first argument is not valid, it is treated as the ROM path.

Files can also be selected from the "File" menu in the GUI.

Arguments:
* `--rom <file>`
* `--bios <file>` Give path to the BIOS. If invalid or not specified, the emulator will default to an HLE implementation.
* `--record <file.wav>` Record all played audio samples to a WAV file.
* `--uncap-fps` Tries to run the emulator at the maximum possible speed.
