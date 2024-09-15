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
#ifndef _TRIGGER_H
#define _TRIGGER_H

#include "swx.h"
#include "parameter.h"
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   bool enabled; // False to disable trigger slot.

   uint8_t input_mask;        // Bitmask for specific trigger channels (LSB=trigger A1). Set zero to disable trigger.
   uint8_t input_invert_mask; // Bitmask inversion for specific trigger channels (LSB=trigger A1).

   analog_channel_t input_audio; // Audio source to compare volume against set threshold. Set ANALOG_CHANNEL_NONE to disable threshold detection.
   float threshold;
   bool threshold_invert; // True to invert threshold result - ie. true when below threshold.
   bool require_both;     // True, trigger will activate only when input operation and threshold are both true, else activate when either are true.

   uint8_t op;         // The conditional operation to perform on the masked input bits. Set TRIGGER_OP_DDD to disable trigger. See trigger_op_t.
   bool output_invert; // True to invert operation result.

   bool repeating;         // True to run without waiting for output state change (e.g. execute action repeatedly when trigger is held).
   uint32_t min_period_us; // The minimum delay between executing triggered actions. Useful when in repeating mode.

   // The action range to run if triggered, set start and end index equal to disable trigger.
   uint8_t action_start_index;
   uint8_t action_end_index;
} trigger_t;

extern trigger_t triggers[MAX_TRIGGERS];

extern uint8_t trig_input_states;

void trigger_init();

void trigger_process();

#ifdef __cplusplus
}
#endif

#endif // _TRIGGER_H