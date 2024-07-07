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
#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <stdint.h>

#define MSG_HEADER_SIZE (3)
#define MSG_PAYLOAD_SIZE (255)
#define MSG_CRC_SIZE (2)
#define MSG_FRAME_SIZE (MSG_HEADER_SIZE + MSG_PAYLOAD_SIZE + MSG_CRC_SIZE + 1) // [id:8][seq:8][len:8][payload:<len>][crc:16] 0x00

#endif // _MESSAGE_H