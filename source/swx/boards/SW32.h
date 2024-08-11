/*
 * swx
 * Copyright (C) 2024 saawsm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BOARDS_SW32_H
#define _BOARDS_SW32_H

#define PIN_PWR_CTRL (19)
#define PIN_DRV_EN (10)
#define PIN_PIP_EN (11) // active low
#define PIN_DISCH_EN (24)

#define PIN_INT_PD (25) // active low

// -------- Triggers --------

#define TRIGGER_COUNT (4)

#define PIN_TRIG_A1 (14) // active low
#define PIN_TRIG_A2 (15) // active low
#define PIN_TRIG_B1 (12) // active low
#define PIN_TRIG_B2 (13) // active low

// -------- I2C --------
#define PIN_I2C_SCL (23)
#define PIN_I2C_SDA (22)
#define I2C_PORT (i2c1)
#define I2C_FREQ (400000) // Hz

#define PIN_I2C_SCL_DAC (9)
#define PIN_I2C_SDA_DAC (8)
#define I2C_PORT_DAC (i2c0)
#define I2C_FREQ_DAC (400000) // Hz

// -------- UART --------
#define PIN_TXD1 (20)
#define PIN_RXD1 (21)

#define UART_PORT (uart1)
#define UART_BAUD (115200)

// -------- I2S Audio --------
#define PIN_I2S_DIN (16)
#define PIN_I2S_WS (17)
#define PIN_I2S_BCLK (18)

// -------- ADC --------
#define PIN_ADC_SENSE (26)
#define PIN_ADC_AUDIO_LEFT (29)
#define PIN_ADC_AUDIO_RIGHT (28)
#define PIN_ADC_AUDIO_MIC (27)

// -------- Channels --------

#define CHANNEL_COUNT (4)
#define CH_DAC_ADDRESS (0x60)

// -------- Channel Defaults --------

#define CH_CAL_THRESHOLD_OK (0.015f)
#define CH_CAL_THRESHOLD_OVER (0.018f)
#define CH_CAL_OFFSET (400)

// -------- Channel 1 --------
#define PIN_CH1_GA (4)
#define PIN_CH1_GB (5)
#define CH1_DAC_CH (0)

// -------- Channel 2 --------
#define PIN_CH2_GA (0)
#define PIN_CH2_GB (1)
#define CH2_DAC_CH (1)

// -------- Channel 3 --------
#define PIN_CH3_GA (2)
#define PIN_CH3_GB (3)
#define CH3_DAC_CH (2)

// -------- Channel 4 --------
#define PIN_CH4_GA (6)
#define PIN_CH4_GB (7)
#define CH4_DAC_CH (3)

// ---------------------------

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

// 16M flash
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

// Board has B1 RP2040
#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

#endif // _SW32_H