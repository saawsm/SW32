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
#ifndef _PULSE_GEN_H
#define _PULSE_GEN_H

#include "swx.h"
#include "parameter.h"
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   bool enabled; // True if action is enabled (type must not be ACTION_NONE).

   action_type_t type; // The operation to perform when this action runs. Set ACTION_NONE to disable.
   uint8_t ch_mask;    // Channel mask indicating channels this action will affect (LSB=channel 1).

   param_t param;   // A parameter used as an argument by some operations.
   target_t target; // A target used as an argument by some operations.

   uint16_t value; // A main value used by operations (e.g. increment/decrement amount).
} action_t;

typedef struct {
   // A bitflag mask indicating what pulse generator channel is enabled (LSB=channel 1).
   // This mask is also updated by actions (e.g. ACTION_ENABLE/DISABLE) to control generation.
   uint8_t en_mask;

   struct {
      uint32_t period_us;           // The duration between sequence mask changes. Set zero to disable sequencer.
      uint8_t index;                // The current mask index.
      uint8_t count;                // The number of sequence items in the masks array before wrapping. Set zero to disable sequencer.
      uint8_t masks[MAX_SEQUENCES]; // LSB=channel 1
   } sequencer;

   struct {
      // The channel analog source, when set generate output pulses based on the analog input, instead of using the function gen. See analog_channel_t.
      // MSBs contains flags indicating how the source should be processed. See AUDIO_MODE_FLAG*.
      uint8_t audio;

      uint16_t parameters[TOTAL_PARAMS][TOTAL_TARGETS];
   } channels[CHANNEL_COUNT];

   // A list of actions, a range of actions can be run by using the execute_action_list() function.
   action_t actions[MAX_ACTIONS];
} pulse_gen_t;

extern pulse_gen_t pulse_gen;

void pulse_gen_init();

// Update pulse generator by updating sequencer, parameters, power level transitions, and generating the actual
// pulses manually or via audio processing depending on configured source.
void pulse_gen_process();

// Updates the parameter step period and step size based on the current target mode, minmum, maximum, and rate.
// Should be called whenever TARGET_MODE, TARGET_MIN, TARGET_MAX, or TARGET_RATE is changed, and the parameter is
// sweeping the value.
void parameter_update(uint8_t ch_index, param_t param);

// Execute each action between indices al_start and al_end
void execute_action_list(uint8_t al_start, uint8_t al_end);

static inline void parameter_set(uint8_t ch_index, param_t param, target_t target, uint16_t value) {
   pulse_gen.channels[ch_index].parameters[param][target] = value;
}

static inline uint16_t parameter_get(uint8_t ch_index, param_t param, target_t target) {
   return pulse_gen.channels[ch_index].parameters[param][target];
}

#ifdef __cplusplus
}
#endif

#endif // _PULSE_GEN_H