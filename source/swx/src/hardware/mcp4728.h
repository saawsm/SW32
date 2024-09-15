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
#ifndef _MCP4728_H
#define _MCP4728_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MCP4728_MAX_VALUE ((1 << 12) - 1)

#define MCP4728_CMD_WRITE_MULTI_IR (0x40)        // Sequential multi-write for DAC Input Registers
#define MCP4728_CMD_WRITE_MULTI_IR_EEPROM (0x50) // Sequential write for DAC Input Registers and EEPROM

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   MCP4728_CHANNEL_1 = 0,
   MCP4728_CHANNEL_2,
   MCP4728_CHANNEL_3,
   MCP4728_CHANNEL_4,
   MCP4728_MAX_CHANNELS,
} mcp4728_channel_t;

typedef enum {
   MCP4728_GAIN_ONE = 0,
   MCP4728_GAIN_TWO,
} mcp4728_gain_t;

typedef enum {
   MCP4728_VREF_VDD = 0,
   MCP4728_VREF_INTERNAL,
} mcp4728_vref_t;

typedef enum {
   MCP4728_PD_NORMAL = 0,
   MCP4728_PD_GND_1K,
   MCP4728_PD_GND_100K,
   MCP4728_PD_GND_500K,
} mcp4728_pd_mode_t;

typedef enum {
   MCP4728_UDAC_FALSE = 0,
   MCP4728_UDAC_TRUE,
} mcp4728_udac_t;

static inline size_t mcp4728_build_write_cmd(uint8_t* buffer, size_t len, mcp4728_channel_t channel, uint16_t value, mcp4728_vref_t vref, mcp4728_gain_t gain,
                                             mcp4728_pd_mode_t mode, mcp4728_udac_t udac) {
   static const size_t BUF_SIZE = 3;

   if (len < BUF_SIZE)
      return 0;

   // ------------------------------------------------------------------------------------------------
   // |             0                 |               1                 |             2              |
   // ------------------------------------------------------------------------------------------------
   // C2 C1 C0 W1 W2 DAC1 DAC0 ~UDAC [A] VREF PD1 PD0 Gx D11 D10 D9 D8 [A] D7 D6 D5 D4 D3 D2 D1 D0 [A]

   buffer[0] = MCP4728_CMD_WRITE_MULTI_IR | ((channel & 0b11) << 1) | udac;

   value = (value & MCP4728_MAX_VALUE) | (vref << 15) | (mode << 13) | (gain << 12);

   buffer[1] = (value >> 8) & 0xFF;
   buffer[2] = value & 0xFF;

   return BUF_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif // _MCP4728_H