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
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include "swx.h"

#define U16_U8(value) ((value) >> 8), ((value) & 0xff)
#define U8_U16(arr, i) ((arr[(i)] << 8) | arr[((i) + 1)])

#define PROTO_REPLY(ch, id, ...)                                                                                                                                         \
   do {                                                                                                                                                                  \
      uint8_t msg[] = {MSG_FRAME_START, id, __VA_ARGS__};                                                                                                                \
      protocol_write_frame((ch), msg, sizeof(msg));                                                                                                                      \
   } while (0)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   COMM_UART,
   COMM_STDIO,
} comm_channel_t;

void protocol_init();

void protocol_process();

void protocol_write_frame(comm_channel_t ch, uint8_t* src, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _PROTOCOL_H