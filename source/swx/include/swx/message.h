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

// Each message is formatted the following way:
//
// [MSG_FRAME_START:8] [data:n]
//
// The message is encoded using COBS before being sent. The receiver will buffer data until it receives a 0x00 byte before decoding it as a frame.
// Debugging information (LOG_ macros) and COBS encoded messages share the same communication channel. This is achieved by having 0x00 byte appended
// to any debugging messages, and assuming these messages will not contain the non-printable ASCII control character (STX 0x02 - MSG_FRAME_START).

#define MSG_SIZE (255)
#define MSG_FRAME_SIZE (MSG_SIZE + 1 + ((MSG_SIZE + (254 - 1)) / 254))
#define MSG_FRAME_START (2) // STX

#define MSG_FETCH (0)
#define MSG_UPDATE (1)

// Get fixed firmware information (e.g. version, channel count)
// MSG_FETCH: <none>
// RESPONSE: [version_pcb_rev:8 version_major:8 version_minor:8] [channel_count:8] [channel_power_max_hi:8 channel_power_max_lo:8]
#define MSG_CMD_INFO (0 << 1)

// Set or get the maximum power level for one or more output channels. Value represents a percentage, out of CHANNEL_POWER_MAX.
// MSG_FETCH:  [ch_mask:8]
// MSG_UPDATE: [ch_mask:8] [value_hi:8 value_lo:8]
//
// Note: MSG_FETCH will reply with MSG_UPDATE, ch_mask is replaced with ch_index.
#define MSG_CMD_MAX_POWER (1 << 1)

#endif // _MESSAGE_H