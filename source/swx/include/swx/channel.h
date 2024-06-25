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
#ifndef _CHANNEL_H
#define _CHANNEL_H

#include <stdint.h>

#ifndef CHANNEL_COUNT
#define CHANNEL_COUNT (4)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   CHANNEL_INVALID = 0,
   CHANNEL_FAULT,
   CHANNEL_READY,
} channel_status_t;

typedef enum {
   ANALOG_CHANNEL_NONE = 0,
   ANALOG_SENSE_CHANNEL,
   ANALOG_AUDIO_CHANNEL_MIC,
   ANALOG_AUDIO_CHANNEL_LEFT,
   ANALOG_AUDIO_CHANNEL_RIGHT,
   TOTAL_ANALOG_CHANNELS,
} analog_channel_t;

typedef enum {
   TRIGGER_CHANNEL_NONE = 0,
   TRIGGER_CHANNEL_A1,
   TRIGGER_CHANNEL_A2,
   TRIGGER_CHANNEL_B1,
   TRIGGER_CHANNEL_B2,
   TOTAL_TRIGGERS
} trigger_channel_t;

#ifdef __cplusplus
}
#endif

#endif // _CHANNEL_H