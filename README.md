# Oneiroi / TechnoMachine

Oneiroi is a multi-functional, self-contained experimental digital synth focused on ambient pads and drone-like landscapes.
It features a full stereo signal path, 3 oscillators (2 of them are mutually exclusive), 4 effects and a looper. It also includes self-modulation and a randomizer.

Oneiroi has been developed by Roberto Noris (<https://github.com/hirnlego>) and is based on Rebel Technology’s OWL platform.

Manual: <https://docs.google.com/document/d/1cE7WRd92GfQUE-cKbV-Cr_mxX72hLjZ5igFbS-vwNsM/edit?usp=sharing>

## Development Setup

This project (formerly Oneiroi 1.2.2) has been renamed to **TechnoMachine** for development purposes.

### Prerequisites

1.  **ARM Toolchain**: You need `arm-none-eabi-gcc`.
    *   **macOS**: `brew install --cask gcc-arm-embedded`
    *   **Linux**: Install via your package manager.
2.  **OwlProgram SDK**: This project depends on the [OwlProgram](https://github.com/RebelTechnology/OwlProgram) library. It is expected to be in the parent directory (`../OwlProgram`).

### Compilation Instructions

1.  **Initialize Dependencies**:
    Ensure `OwlProgram` and its submodules (specifically DaisySP) are downloaded.
    ```bash
    cd ../OwlProgram
    git submodule update --init --recursive
    ```

2.  **Compile**:
    Run `make` in the project directory.
    ```bash
    make
    ```
    This generates:
    *   `../OwlProgram/Build/patch.bin` (Binary for flashing)
    *   `../OwlProgram/Build/patch.syx` (Sysex for MIDI upload)

3.  **Load to Device**:
    *   **Load to RAM (Temporary Test):**
        ```bash
        make load
        ```
    *   **Store to Flash (Permanent):**
        ```bash
        make store
        ```

### Changes from Official Release

To compile the original Befaco Oneiroi code with the latest ARM GCC toolchain (v14.3+) and the current OwlProgram SDK, several modifications were necessary.

**1. Project Renaming**
*   The main patch file `Oneiroi_1_2_2Patch.hpp` was renamed to `TechnoMachinePatch.hpp`.
*   The class name was updated to `TechnoMachinePatch`.
*   The `Makefile` now defaults to `PATCHNAME = TechnoMachine`.

**2. OwlProgram SDK Patches**
The `OwlProgram` SDK required minor adjustments. The Oneiroi hardware requires specific button definitions (`BUTTON_9` - `BUTTON_12`) that are not yet present in the standard Rebel Technology OwlProgram SDK. Additionally, modern GCC compilers (v14+) are stricter about standard library includes in bare-metal environments.

*   **Added Button Definitions**: `OpenWareMidiControl.h` was updated to include `BUTTON_9` through `BUTTON_12`.
*   **Compiler Compatibility (GCC 14+)**:
    *   Removed `#include <string>` from `PatchProgram.cpp` to resolve standard library conflicts in a freestanding environment.
    *   Removed `-ffreestanding` flag from `compile.mk` to allow proper linking with the latest toolchain libraries.
    *   Added `#include <stddef.h>` to `heap.h` to define `size_t`.

---

## Updating The patch via browser (Plain human way)

Open a Web-MIDI-enabled browser. We recommend using Chrome or Chromium.
Other Browsers would maybe work but it has not been tested.

Find the latest patch from the release page:

<https://github.com/Befaco/Oneiroi/tags>

Click on "Downloads" and download the file named "patch.syx"

Connect Oneiroi to a standard Eurorack power supply and your computer using any USBs on the bottom.

On your browser, go to the OpenWare Laboratory:

<https://pingdynasty.github.io/OwlWebControl/extended.html>

Check if Oneiroi is connected to the page: The page should say: "Connected to Oneiroi vx.x.x"

Click "Choose file" and upload the patch.syx from your computer

Push "Store". A popup will appear, asking for the slot. Accept to store on slot 1.

Done!



## Building and flashing the patch (Nerdy way)

To build and flash the patch you can either run

`make PLATFORM=OWL3 PATCHNAME=TechnoMachine CONFIG=Release clean load`

to load it in RAM (temporarily), or

`make PLATFORM=OWL3 PATCHNAME=TechnoMachine CONFIG=Release clean store SLOT=1`

to store it in FLASH (permanently), or

`make PLATFORM=OWL3 PATCHNAME=TechnoMachine CONFIG=Release clean sysex`

to generate `./Build/patch.syx` that can be flashed with
the online tool

<https://pingdynasty.github.io/OwlWebControl/extended.html>

## Calibration Procedure for >1.2 Patch/Firmware

It calibrates V/OCT IN, and Pitch/Speed Knobs mid position

Updating the patch do not erase the calibration data, so no need to perform calibration after patch update.

Only needed after module's assembly (usually preformed only at factory or by DIYers).


  1- Ensure that there is 0 volts at the CV IN (either with no cable connected or with a cable that measures 0 volts).
     Then, power on the module while simultaneously holding down the [MOD AMT] and [SHIFT] buttons.
     The [RECORD] button should light up red, indicating that you are in Calibration Mode.
     
  2- Place both the PITCH knob and the VARISPEED knob in the middle, this allows for calibrating the center point
  
  3- Plug 2 Volts into V/OCT IN and push [RECORD] button. Now [SHIFT] button will light up red.

  4- Plug 5 Volts into V/OCT IN and push [SHIFT] button. Now [RANDOM] button will light up red.

  5- Plug 8 Volts into V/OCT IN and push [RANDOM] button. Now [MOD AMT] button will light up green. 

  6- Push [MOD AMT] button to exit calibration mode.

  

  ## Calibration Procedure for 1.1 or older Patch/Firmware versions.
  
It calibrates V/OCT IN, Pitch/Speed Knobs mid position and manual Controls range.

Updating the patch do not erase the calibration data, so no need to perform calibration after patch update.

Only needed after module's assembly (usually preformed only at factory or by DIYers).


  1- Power the module while holding [MOD AMT + SHIFT] buttons at the same time. 
The [RECORD] button should light up red indicating you are in Calibration Mode.


  2- Place both the PITCH knob and the VARISPEED knob in the middle, this allows for calibrating the center point
  
  3- Plug 2 Volts into V/OCT IN and push [RECORD] button. Now [SHIFT] button will light up red.

  4- Plug 5 Volts into V/OCT IN and push [SHIFT] button. Now [RANDOM] button will light up red.

  5- Plug 8 Volts into V/OCT IN and push [RANDOM] button. Now [MOD AMT] button will light up green. 

  6- Move all manual controls to the minimum position, this is all pots counter-clockwise and all faders down.
  
  7- Move all manual controls to the maximun position, this is all pots clockwise and all faders up.

  7- Push [MOD AMT] button to exit calibration mode.
