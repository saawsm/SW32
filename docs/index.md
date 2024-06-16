# About

The SW32 is a 4-channel E-Stim box based around the MK312-BT. Focusing on a design that adds modern conveniences like USB Type-C PD and Li-Ion battery support.

Controlling and interacting with the box can be achieved in numerous ways. This includes the front panel inferface, Bluetooth, or via Wi-Fi using the internally hosted web app.

An RP2040 microcontroller was picked since it has dual cores and the [PIO](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip27a2111bf36b4a04eb7b) hardware peripheral. An active community and well documented SDK is also a bonus.

*[Bluetooth]: A2DP - Stream audio to the box like a Bluetooth speaker.

## Features

* 1.8" 160x128 Color Display
* 4x Isolated Output Channels
  * Triphase Support
  * Tri-polar (+, GND, -) Support
* 4x Trigger Channels
* Stereo Line-In and Mono Microphone In (with programmable gain)
* Internal Microphone
* USB Type-C PD Charging
* Li-Ion / SLA Battery Pack Support

*[PD]: Power Delivery
*[SLA]: Sealed Lead Acid

## Overview

The SW32 consists of following PCBs:

PCB                 | Description
------------------- | -------------------
Main Board          | Provides the core functionality of the SW32.
Output Board        | Generates isolated E-Stim output for each channel. Driven by the main board.
Front Control Board | The PCB with all the front panel controls (e.g. pots, buttons, LEDs). Provides wireless features.
Front Panel         | A cosmetic silkscreen PCB for the front control board.
Display Module      | 1.8" 160x128 TFT SPI color display.

*[Display Module]: Third-party display module (ADA358)

The reason for having a separate output board is primarily for flexibility, space savings (expand vertically instead of horizontally), and long-term cost savings. Most of the cost is in the main board. So being able to reuse the main board when changing the output design will help keep costs down.
