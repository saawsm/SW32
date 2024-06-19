# Build Guide

This build guide assumes the reader has some experience with building open-source electronics kits.

The PCBs have been designed with partial assembly from [JLCPCB](https://jlcpcb.com/quote){:target="_blank"} in mind. Hand soldering through-hole components will be required once ordered PCBs arrive.

It's recommended to have access to a 3D printer. Some of the parts are designed to be 3D printed, such as the case and button caps. Optionally a 3D printing service such as [JLC3DP](https://jlc3dp.com/3d-printing-quote){:target="_blank"} could be used.

## Part Ordering

All parts required to populate the PCBs can be purchased from LCSC/JLCPCB with the following exceptions:

- Transformers - Usually in stock at Mouser.
- Potentiometers - Available at Mouser. Available as a preorder item at LCSC at the time of writing.
- Rotary Encoders - Available at Mouser. Out of stock at LCSC at the time of writing.

BOM lists are available with associated zipped gerber files located in the `/pcb` directory. Each list is split in two, one for the surface mount components intended for the JLCPCB assembly service (`*.bom.csv`), and another for through-hole components (`*.bom-hand_assembly.csv`).

This allows for easy ordering of through-hole components at LCSC via their [BOM Tool](https://www.lcsc.com/bom){:target="_blank"}

*[BOM]: Bill of Materials

## Misc Parts

Parts that don't fit the assembly BOM files are placed in the BOM file located in the `/docs/assets` directory.

{{ read_csv('assets/bom.misc.csv') }}

## PCB Ordering

The following provides recommended settings for ordering PCBs and getting them assembled. If an option isn't specified it's assumed to be set to the default setting provided by JLCPCB.

Component rotations in CPL files should be correct, but I recommend to double check them anyway during checkout.

??? info "About order quantity"
    Please note that the minimum order quantity for PCB Assembly at JLCPCB is two, while the minimum for PCB fabrication is five. Additionally certain components have a minimum assembly quantity of five (and attrition quantity but we will ignore that for now).

    If you order two assembled boards each using one component, and this component has a minimum assembly quantity of five, it will still use five components. You then will receive two assembled and three blank PCBs.

    Additionally initial setup fees don't scale with order quantity, so ordering five is more efficient than ordering two assembled boards.

### Main Board

Asset       | File
----------- | -----------
Gerbers     | `main_board.zip`
BOM         | `main_board.bom.csv`
CPL         | `main_board.top-cpl.csv`

Option                 | Value
---------------------- | ----------------------
Base Material          | FR-4
Layers                 | 4
PCB Thickness          | 1.6mm
Outer Copper Weight    | 1oz
Inner Copper Weight    | 0.5oz
Specify Layer Sequence | F_Cu, In1_Cu, In2_Cu, B_Cu
Impedance Control?     | Yes
Layer Stackup          | JLC04161H-7628
Via Covering           | Plugged (free upgrade from tented)
Remove Order Number    | Specify a location

PCB Assembly?          | Yes
---------------------- | ----------------------
PCBA Type              | Economic
Assembly Side          | Top Side

### Output Board

Asset       | File
----------- | -----------
Gerbers     | `output_board.zip`
BOM         | `output_board.bom.csv`
CPL         | `output_board.top-cpl.csv`

PCB Option             | Value
---------------------- | ----------------------
Base Material          | FR-4
Layers                 | 2
PCB Thickness          | 1.6mm
Outer Copper Weight    | 1oz
Via Covering           | Tented
Remove Order Number    | Specify a location

PCB Assembly?          | Yes
---------------------- | ----------------------
PCBA Type              | Economic
Assembly Side          | Top Side

### Front Control Board

Asset       | File
----------- | -----------
Gerbers     | `front_control_board.zip`
BOM         | `front_control_board.bom.csv`
CPL         | `front_control_board.top-cpl.csv`

Option                 | Value
---------------------- | ----------------------
Base Material          | FR-4
Layers                 | 2
PCB Thickness          | 1.6mm
Outer Copper Weight    | 1oz
Via Covering           | Tented
Remove Order Number    | Specify a location

PCB Assembly?          | Yes
---------------------- | ----------------------
PCBA Type              | Standard (required for LEDs and ESP32 module)
Assembly Side          | Top Side
Bake Components?       | Yes (recommended for moisture sensitive LEDs. LEDs have a high chance of being DOA without this)

*[DOA]: Dead on Arrival

### Front Panel

Asset       | File
----------- | -----------
Gerbers     | `front_panel.zip`
BOM         | N/A
CPL         | N/A

Option                 | Value
---------------------- | ----------------------
Base Material          | FR-4
Layers                 | 2
PCB Thickness          | 1.6mm
Outer Copper Weight    | 1oz
Via Covering           | Tented
PCB Color              | Black (suggested for high contrast readable text, feel free to pick any color)
Silkscreen             | White
Remove Order Number    | Specify a location

PCB Assembly?          | No
---------------------- | ----------------------
Front panel has no components {: class="parent-display-none" } | .

*[CPL]: Component Pick List (also known as a POS file)
