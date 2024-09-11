# SW32 - 4 Channel E-Stim

*This project is a work in-progress. Changes to the design might occur. Order PCBs at your own risk..*

---

## About

The SW32 is a 4-channel E-Stim box based around the MK312 box. Focusing on a modern and compact redesign. It supports USB Type-C PD charging with a variety of batteries such as Li-Ion, Li-Poly, or Lead Acid.

Controlling and interacting with the box can be achieved in numerous ways. Such as the front panel controls, USB, Bluetooth/A2DP, or via Wi-Fi using the internally hosted web app.

An RP2040 microcontroller was picked since it has dual cores and the [PIO](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip27a2111bf36b4a04eb7b) hardware peripheral. An active community and well documented SDK is also a bonus.

## Features

* 1.8" 160x128 Color Display
* 4x Isolated Output Channels
  * Triphase Support
  * Tri-polar (+, GND, -) Support
* Stereo Line-In and Mono Microphone Input (with programmable gain)
* Internal Microphone
* 4x Trigger Channels
* USB Type-C PD Charging
* Multi-chemistry Battery Pack Support (Li-Ion/Li-Poly/Lead Acid)

## Overview

The SW32 consists of following PCBs:

* Main Board - Provides the core functionality of the SW32.
* Output Board - Generates the isolated E-Stim output for each channel. Driven by the main board.
* Front Control Board - The PCB with all the front panel controls (e.g. pots, buttons, LEDs).
* Front Panel - A cosmetic silkscreen PCB for the front control board.
* Display Module* - 1.8" 160x128 TFT SPI color display.

The reason for having a separate output board is primarily for flexibility, space savings (expand vertically instead of horizontally), and long-term cost savings. Most of the cost is in the main board. So being able to reuse the main board when changing the output design will help keep costs down.

## Fabrication

This build is mostly SMT with THT for connectors and front panel components. PCBs have been designed specifically for JLCPCB fabrication and assembly.

Schematics can be found in [/schematics](/schematics), and PCB fabrication files (gerbers, BoMs, CPL, etc.) can be found in [/pcb](/pcb).

## Disclaimer

The SW32 comes with absolutely no warranty of any kind. The designs are free to use, under the conditions defined in the [LICENSE](LICENSE). Using the designs in original or modified form, implies acceptance of any and all associated risks and liabilities.

* Use it entirely at your own risk.
* Do not use it for anything safety-critical, medical, industrial, military, or illegal.
* Do not use it on anyone without explicit and informed consent.
* Keep any and all electrodes below the chest.
* Do not use Li-Ion battery packs without a BMS.

### Safety

SW32 can generate potentially lethal voltages and currents on its outputs and on internal components, even when powered from a battery. The risks associated with building, testing and using this device can be reduced but not eliminated.
