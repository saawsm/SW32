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
#ifndef _MCP443X_H
#define _MCP443X_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   MCP443X_CHANNEL_1 = 0,
   MCP443X_CHANNEL_2,
   MCP443X_CHANNEL_3,
   MCP443X_CHANNEL_4,
   MCP443X_MAX_CHANNELS,
} mcp443x_channel_t;

static inline size_t mcp443x_build_write_cmd(uint8_t* buffer, size_t len, mcp443x_channel_t channel, uint8_t value) {
   static const size_t BUF_SIZE = 2;

   if (len < BUF_SIZE)
      return 0;

   static const uint8_t addresses[] = {
       [MCP443X_CHANNEL_1] = 0b0000,
       [MCP443X_CHANNEL_2] = 0b0001,
       [MCP443X_CHANNEL_3] = 0b0110,
       [MCP443X_CHANNEL_4] = 0b0111,
   };

   buffer[0] = addresses[channel]; // Note: Value is 9 bit (max 256), ignored for simplicity.
   buffer[1] = value;

   return BUF_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif // _MCP443X_H