# Oneiroi / TechnoMachine: Loader & Troubleshooting Guide

This guide covers how to load your custom `TechnoMachine` patch onto the Befaco Oneiroi module and how to recover if things go wrong.

## ⚠️ Important Safety Note: "Bricking"

**You cannot permanently brick your Oneiroi by uploading bad code.**

The module has a separate "Bootloader" that lives in a protected part of memory. Even if your code crashes the module immediately upon startup (making it unresponsive), you can always enter Bootloader Mode to wipe it and upload the factory firmware.

## 1. How to Load Your Patch

### Option A: The "Plain Human" Way (Web Browser)
*Best for getting started and simple testing.*

1.  **Locate the File**: After compiling, your file is located at `../OwlProgram/Build/patch.syx`.
2.  **Connect**: Plug your Oneiroi into your computer via USB.
3.  **Open Browser**: Use Google Chrome, Edge, or Opera (Firefox/Safari do not support WebMIDI).
4.  **Go to**: [OpenWare Laboratory Web Control](https://pingdynasty.github.io/OwlWebControl/extended.html).
5.  **Connect**: The page should automatically detect "Oneiroi" or "OWL MIDI Device". If not, check your USB cable.
6.  **Upload**:
    *   Click **"Load File"** (or "Choose File").
    *   Select your `patch.syx`.
    *   Click **"Run"** to test it in RAM (temporary).
    *   Click **"Store"** (Slot 1) to save it permanently.

### Option B: The "Nerdy" Way (Command Line)
*Best for rapid development iterations.*

1.  **Connect** via USB.
2.  **Run Command**:
    *   **Test (RAM):** `make load`
    *   **Save (Flash):** `make store`
3.  **Monitor**: You will see a progress bar in your terminal as the SysEx data is sent.

## 2. Virtual Testing (No Hardware)

You can compile and run the code on your computer to verify it doesn't crash.

1.  **Compile & Run Native**:
    ```bash
    make run
    ```
    This builds an executable version of your patch and runs it once. If you see "Register patch TechnoMachine" and no errors, your code is valid C++.

2.  **Audio Processing Test**:
    You can process a WAV file through your patch logic:
    ```bash
    ../OwlProgram/Build/Test/patch input.wav output.wav
    ```
    *(Note: You need to provide your own 48kHz stereo `input.wav`)*

---

## 3. Troubleshooting & Recovery

### "The module is frozen / I uploaded code that crashes"
If you upload code that enters an infinite loop or crashes immediately, the module might stop responding to USB.

**Recovery Steps:**
1.  **Power Cycle**: Turn the Eurorack case off and on.
2.  **Bootloader Mode**: 
    *   Disconnect USB.
    *   Turn power **OFF**.
    *   Hold down the **Button** (Check Befaco manual for the specific DFU/Boot button combo, typically it is the main encoder button or a specific function button).
    *   Turn power **ON** while holding the button.
    *   The module should start in a "safe" mode where it waits for firmware.
3.  **Reload Factory Firmware**:
    *   Go to the [Befaco Oneiroi Releases Page](https://github.com/Befaco/Oneiroi/tags).
    *   Download the official `patch.syx`.
    *   Use the Web Loader to flash this file.

### "Device not found in Browser"
1.  **Browser**: Ensure you are using **Chrome** or **Edge**.
2.  **Cable**: 90% of issues are bad USB cables. Ensure it is a **Data** cable, not just a charging cable.
3.  **Permissions**: The browser will ask for permission to access MIDI devices. You must click "Allow".
4.  **Close Other Apps**: Ensure no other DAW (Ableton, Logic) or MIDI utility is "hogging" the USB connection.

### "I hear no audio"
1.  **Gain Staging**: The input/output gains might be set to zero. Check the `patchCtrls.inputVol` in your code.
2.  **Bypass Mode**: You might be in hardware bypass.
3.  **Calibration**: If pitch tracking is weird, run the Calibration Procedure detailed in the main `README.md`.

### "My changes aren't doing anything"
1.  **Did you compile?** Run `make` again and check the timestamp of `patch.syx`.
2.  **Did you store?** If you only clicked "Run" or `make load`, the changes disappear when you power cycle. You must use "Store" or `make store` to make them permanent.
3.  **Slot**: Ensure you are storing to **Slot 1**. The Oneiroi typically runs the patch in Slot 1 by default.

---

## 4. Useful Development Tips

*   **LED Debugging**: Since you don't have a screen, use the LEDs!
    ```cpp
    // Example: Turn on Red LED if variable is high
    if (myVariable > 0.5f) {
        ui_->SetLed(LED_RED, 1.0f); 
    }
    ```
*   **Audio Debugging**: Route control signals to the audio output to "hear" what they are doing.
    ```cpp
    // Debug: output the LFO to the left audio channel
    buffer.getSamples(LEFT_CHANNEL)[i] = myLFOValue; 
    ```

