# Firmware

The firmware for the SW32 is open-source and mostly written in C. It makes use of both [pico-sdk](https://github.com/raspberrypi/pico-sdk){:target="_blank"} and [esp-idf](https://github.com/espressif/esp-idf){:target="_blank"}
frameworks.

It can be acquired by downloading precompiled binaries or built directly from source. Many settings can be changed even after the firmware has been built, so unless you make significant hardware changes, you can use precompiled firmware most of the time.

## Precompiled Firmware

Available in `/firmware/<board_name>`, compiled using default configuration options. If your SW32 build differs from stock, you might need to [build from source](#building-from-source).

## Flashing

The following sections detail how to flash the boards with firmware.

### Main Board {: #flashing-main-board }

Since the main board uses an RP2040, the process of flashing the main board is very similar to flashing a Raspberry Pi Pico.

1. Ensure main board is unpowered by checking that the PWR LED is off (D7).
2. Bridge the `BOOT` header (JP3) using a jumper shunt or paper clip.
3. Connect to computer via USB cable.
4. Remove the jumper shunt or paper clip from `BOOT` header (JP3) (step #2)
5. The main board should appear as a USB mass storage device. Copy the `swx.uf2` file found in `/firmware` (`/source/swx/build` if building from source) into this drive.
6. The USB mass storage device should disappear. You can confirm flashing was successful using a Serial Monitor or wait until after the front control board is loaded with firmware.

### Front Control Board {: #flashing-front-control-board }

Flashing the front control board is a little more involved compared to the main board. Ideally, you will need an [ESP-PROG](https://docs.espressif.com/projects/espressif-esp-dev-kits/en/latest/other/esp-prog/user_guide.html){:target="_blank"} to make flashing easier.

#### ESP-PROG Method (Preferred)

The ESP-PROG is the easiest way to connect to the front control board for flashing.

1. Ensure the front control board is disconnected from the main board.
2. Configure the ESP-PROG jumpers:

   - `PROG PWR SEL` = 3.3V (**Warning**: Ensure this is set to **3.3V**, if set to 5V the board will be damaged)
   - `IO0 On/Off` = On (jumper shunt installed)
	 
3. Connect the ESP-PROG 2x3 0.1" `PROG` connector to the front control board `PROG` connector.
4. Flash using [ESPWEBTOOL](#flash-using-espwebtool-preferred) if using precompiled firmware or by [direct flashing](#direct-flashing) when building from source.

#### Main Board USB Bridge Method (Not recommended)

If you don't have an ESP-PROG or don't want to get one. This method uses the main board as a USB to Serial converter so you can flash the front control board through the main board.

1. Follow the section on [flashing the main board](#flashing-main-board) but instead of using the `swx.uf2` file, use the `swx-bridge.uf2` file.
2. Ensure the main board is unpowered by checking that the PWR LED is off (D7).
3. Ensure the front control board is connected to the main board via the 2x4 connector.
4. Jump the `IO0` pin of the front control panel `PROG` header to ground.
5. Connect to computer via USB cable.
6. Remove jumper from the `IO0` pin (step #4).
7. Flash using [ESPWEBTOOL](#flash-using-espwebtool-preferred) if using precompiled firmware or by [direct flashing](#direct-flashing) when building from source.
8. Revert step one by following the section on [flashing the main board](#flashing-main-board) (using the `swx.uf2` this time).

#### Flash using ESPWEBTOOL (Preferred)

This is the easiest method for flashing the front control board when using precompiled firmware.

1. Goto the [ESPWEBTOOL](https://esp.huhn.me/){:target="_blank"} site.
2. Click `Connect` & select the correct COM port.
3. Enter the following partitions:

      Offset   | File Name
      -------- | --------
      0x1000   | bootloader.bin
      0x8000   | partition-table.bin
      0x10000  | swef.bin
			0x210000 | www.bin

4. Click `PROGRAM`
5. Upon completion, the display should startup.

## Building from Source

### Main Board

1. Install [pico-sdk](https://github.com/raspberrypi/pico-sdk){:target="_blank"} (Windows users look [here](https://github.com/raspberrypi/pico-setup-windows)).

```bash
git submodule update --init --recursive

cd source/swx
mkdir build

cmake ..
make -j4
```

Compiled firmware is at `/build/swx.uf2`.
See the [flashing section](#flashing-front-control-board) for how to flash the board with the firmware.

### Front Control Board

1. Install [esp-idf](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/get-started/index.html#installation){:target="_blank"}.

```bash
git submodule update --init --recursive

cd source/swef
mkdir build

idf.py build

```

Compiled firmware is at:

- `/build/swef.bin`
- `/build/www.bin`
- `/build/partition-table/partition-table.bin`
- `/build/bootloader/bootloader.bin`

See the [flashing section](#flashing-front-control-board) for how to flash the board with the firmware.

#### Direct Flashing

1. Ensure you have [esp-idf](https://docs.espressif.com/projects/esp-idf/en/v5.2.2/esp32/get-started/index.html#installation){:target="_blank"} installed.
2. Build [front control board](#front-control-board) firmware at least once.

```bash
cd source/swef
idf.py flash -p <port>
```
