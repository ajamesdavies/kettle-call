# Kettle Call

A DIY push-to-talk intercom for two rooms on the same Wi-Fi network. Build two ESP32-S3 units, give them different names, then hold the button on either unit to talk. The units find each other via mDNS and send audio over UDP port 5005. No cloud service is required.

This repository contains firmware v0.5.2 and the files for the 3D-printed enclosure.

## Parts and wiring

For each unit: an ESP32-S3 DevKitC-1, an INMP441 I²S microphone, a MAX98357A I²S amplifier, a compatible speaker, and a normally open momentary button. The firmware drives a NeoPixel on GPIO 48.

| Part | Signal | ESP32-S3 GPIO |
| --- | --- | --- |
| INMP441 | SCK, WS, SD | 5, 6, 4 |
| MAX98357A | BCLK, LRC, DIN, SD (shutdown) | 17, 18, 7, 11 |
| Push-to-talk button | Signal; other terminal to GND | 10 |
| NeoPixel | Data | 48 |

Use a common ground for all modules. Connect power and the speaker according to the pinout of your breakout boards.

## Build and flash

1. Open this folder in PlatformIO. Copy [`src/Core/LocalConfig.example.h`](src/Core/LocalConfig.example.h) to `src/Core/LocalConfig.h`. Git ignores the local copy.
2. In the local file, set your Wi-Fi network and password and choose a unique device name (for example, `kettle-one`).
3. Connect the first board and run `pio run -t upload`. Change the device name (for example, to `kettle-two`), then connect and flash the second board.
4. Power both units on the same 2.4 GHz Wi-Fi network. Once both LEDs are green, hold the button on either unit to speak and release it to listen. Use `pio device monitor` at 115200 baud to troubleshoot discovery. If the units cannot find each other, check whether the Wi-Fi network isolates clients or blocks multicast.

The firmware is designed for a pair of devices on a trusted local network. It does not encrypt or authenticate audio traffic.

## Enclosure

The [enclosure folder](enclosure/) contains the editable FreeCAD models and separate 3MF exports for the faceplate and rear tub. Open the 3MF files in your slicer and select settings for your printer and material.
