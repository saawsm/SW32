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
#include "pulse_gen.h"

#include "output.h"

#define STATE_COUNT (4)
#define MAX_FREQUENCY_HZ (500) // pulse generation frequency limit

// Pulse generation power fade-in/fade-out transition sequence
static const param_t STATE_SEQUENCE[STATE_COUNT] = {
    PARAM_ON_RAMP_TIME,
    PARAM_ON_TIME,
    PARAM_OFF_RAMP_TIME,
    PARAM_OFF_TIME,
};

typedef struct {
   int8_t step; // Number of steps to increment/decrement per parameter update.

   uint32_t last_update_time_us; // The absolute timestamp since the last parameter step update.
   uint32_t update_period_us;    // Parameter step update period.
} parameter_t;

typedef struct {
   uint8_t state_index; // The current "waveform" state (e.g. off, on_ramp, on).

   uint32_t last_power_time_us; // The absolute timestamp since the last power update occurred.
   uint32_t last_pulse_time_us; // The absolute timestamp since the last pulse occurred.

   uint32_t last_state_time_us; // The absolute timestamp since the last "waveform" state change (e.g. off -> on_ramp -> on).

   parameter_t parameters[TOTAL_PARAMS];
} generator_t;

static inline void parameter_step(uint8_t ch_index, param_t param);
static inline void execute_action(uint8_t a_index, uint8_t nest_count);

static generator_t generators[CHANNEL_COUNT] = {0};

static uint32_t last_sequence_time_us = 0;

pulse_gen_t pulse_gen = {0};

void pulse_gen_init() {
   LOG_DEBUG("Init pulse generator...\n");

   // Set default parameter values
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      parameter_set(ch_index, PARAM_POWER, TARGET_MAX, UINT16_MAX);   // max. 100% (auto cycle limit)
      parameter_set(ch_index, PARAM_POWER, TARGET_VALUE, UINT16_MAX); // 100%

      parameter_set(ch_index, PARAM_FREQUENCY, TARGET_MAX, 5000);   // max. 500 Hz (auto cycle limit)
      parameter_set(ch_index, PARAM_FREQUENCY, TARGET_VALUE, 1800); // 180 Hz

      parameter_set(ch_index, PARAM_PULSE_WIDTH, TARGET_MAX, 500);   // max. 500 us (auto cycle limit)
      parameter_set(ch_index, PARAM_PULSE_WIDTH, TARGET_VALUE, 150); // 150 us

      parameter_set(ch_index, PARAM_ON_TIME, TARGET_MAX, 10000);      // max. 10 seconds (auto cycle limit)
      parameter_set(ch_index, PARAM_ON_RAMP_TIME, TARGET_MAX, 5000);  // max. 5 seconds (auto cycle limit)
      parameter_set(ch_index, PARAM_OFF_TIME, TARGET_MAX, 10000);     // max. 10 seconds (auto cycle limit)
      parameter_set(ch_index, PARAM_OFF_RAMP_TIME, TARGET_MAX, 5000); // max. 5 seconds (auto cycle limit)
   }
}

static inline uint32_t state_time_us(uint8_t ch_index) {
   return parameter_get(ch_index, STATE_SEQUENCE[generators[ch_index].state_index], TARGET_VALUE) * 1000u;
}

void pulse_gen_process() {
   uint8_t sequencer_mask;
   if (pulse_gen.sequencer.period_us == 0 || pulse_gen.sequencer.count == 0) {
      sequencer_mask = 0xff; // If sequencer is disabled, mask all enabled
   } else {

      if ((time_us_32() - last_sequence_time_us) > pulse_gen.sequencer.period_us) {
         last_sequence_time_us = time_us_32();

         if (++pulse_gen.sequencer.index >= pulse_gen.sequencer.count)
            pulse_gen.sequencer.index = 0; // Increment or reset after count
      }

      if (pulse_gen.sequencer.index >= MAX_SEQUENCES)
         pulse_gen.sequencer.index = MAX_SEQUENCES - 1;

      sequencer_mask = pulse_gen.sequencer.masks[pulse_gen.sequencer.index];
   }

   // Currently enabled channel mask
   const uint8_t en = pulse_gen.en_mask & sequencer_mask;

   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      generator_t* const gen = &generators[ch_index];

      if (~en & (1 << ch_index)) { // When disabled, hold state at zero
         gen->state_index = 0;
         gen->last_state_time_us = time_us_32();
         continue;
      }

      // Update dynamic parameters
      for (uint8_t i = 0; i < TOTAL_PARAMS; i++)
         parameter_step(ch_index, i);

      // Update "waveform" state
      if ((time_us_32() - gen->last_state_time_us) > state_time_us(ch_index)) {
         gen->last_state_time_us = time_us_32();

         if (++gen->state_index >= STATE_COUNT)
            gen->state_index = 0; // Increment or reset after 4 states (on_ramp, on, off_ramp, off)
      }

      uint16_t power_level = parameter_get(ch_index, PARAM_POWER, TARGET_VALUE);
      if (power_level == 0)
         continue;

      float power = (float)power_level / UINT16_MAX;

      // Scale power level depending on the current "waveform" state (e.g. transition between off and on)
      param_t channel_state = STATE_SEQUENCE[gen->state_index];
      switch (channel_state) {
         case PARAM_ON_RAMP_TIME:    // Ramp power from zero to power value
         case PARAM_OFF_RAMP_TIME: { // Ramp power from power value to zero
            uint32_t ramp_time = state_time_us(ch_index);
            if (ramp_time == 0)
               break;

            uint32_t elapsed = time_us_32() - gen->last_state_time_us;

            float state_modifier = (float)elapsed / ramp_time;
            if (state_modifier > 1.0f)
               state_modifier = 1.0f;

            if (channel_state == PARAM_OFF_RAMP_TIME)
               state_modifier = 1.0f - state_modifier; // Invert if transition is going from on to off.

            power *= state_modifier;
            break;
         }

         case PARAM_OFF_TIME: // If the state is off, continue to next channel without pulsing
            continue;
         default:
            break;
      }

      uint16_t pulse_width = parameter_get(ch_index, PARAM_PULSE_WIDTH, TARGET_VALUE);
      if (pulse_width == 0)
         continue;

      uint8_t audio = pulse_gen.channels[ch_index].audio;
      analog_channel_t audio_src = audio & ~AUDIO_MODE_FLAG;

      // Channel has audio source and a mode, so process audio
      if (audio_src && (audio & AUDIO_MODE_FLAG)) {

         extern float audio_process(analog_channel_t audio_src, bool gen_zcs, uint8_t ch_index, uint16_t pulse_width_us, uint32_t min_period_us,
                                    uint32_t* last_pulse_time_us);

         // Process audio by generating pulses at zero crossings if enabled and return computed audio amplitude.
         bool gen_zcs = !!(audio & AUDIO_MODE_FLAG_PULSE);
         float amplitude = audio_process(audio_src, gen_zcs, ch_index, pulse_width, HZ_TO_US(MAX_FREQUENCY_HZ), &gen->last_pulse_time_us);

         if (audio & AUDIO_MODE_FLAG_POWER) // Apply amplitude to output power
            power *= amplitude;
      }

      // Set channel output power, limit updates to ~2.2 kHz since it takes the DAC about ~110us/ch
      if ((time_us_32() - gen->last_power_time_us) > 110 * CHANNEL_COUNT) {
         gen->last_power_time_us = time_us_32();
         output_power(ch_index, power);
      }

      uint16_t frequency = parameter_get(ch_index, PARAM_FREQUENCY, TARGET_VALUE);
      if (frequency == 0)
         continue;

      uint32_t period_us = DHZ_TO_US(frequency);
      if (period_us < HZ_TO_US(MAX_FREQUENCY_HZ)) // limit max frequency
         period_us = HZ_TO_US(MAX_FREQUENCY_HZ);

      // Generate pulse
      if ((time_us_32() - gen->last_pulse_time_us) > period_us) {
         gen->last_pulse_time_us = time_us_32();

         output_pulse(ch_index, pulse_width, pulse_width, time_us_32() + 110); // ~110us for DAC write
      }
   }
}

// Alarm callback function for disabling channel pulse generation using a channel mask
static int64_t ch_gen_mask_disable_cb(alarm_id_t id, void* user_data) {
   (void)id;
   const uint8_t channel_mask = (int)user_data;
   pulse_gen.en_mask &= ~channel_mask;
   return 0; // Dont reschedule the alarm
}

// Alarm callback function for enabling channel pulse generation using a channel mask
static int64_t ch_gen_mask_enable_cb(alarm_id_t id, void* user_data) {
   (void)id;
   const uint8_t channel_mask = (int)user_data;
   pulse_gen.en_mask |= channel_mask;
   return 0; // Dont reschedule the alarm
}

// Alarm callback function for toggling channel pulse generation using a channel mask
static int64_t ch_gen_mask_toggle_cb(alarm_id_t id, void* user_data) {
   (void)id;
   const uint8_t channel_mask = (int)user_data;
   pulse_gen.en_mask ^= channel_mask;
   return 0; // Dont reschedule the alarm
}

static void execute_action_list_nested(uint8_t al_start, uint8_t al_end, uint8_t nest_count) {
   if (nest_count > 2) {
      LOG_WARN("Max recursion depth reached! Ignoring invocation: al=%u-%u depth=%u", al_start, al_end, nest_count);
      return;
   }

   for (uint8_t i = al_start; i < al_end; i++)
      execute_action(i, nest_count);
}

// Execute the action at the given action slot index
static inline void execute_action(uint8_t a_index, uint8_t nest_count) {
   if (a_index >= MAX_ACTIONS)
      return;

   action_t* const action = &pulse_gen.actions[a_index];

   if (!action->enabled || action->type == ACTION_NONE)
      return;

   switch (action->type) {
      case ACTION_SET:
      case ACTION_INCREMENT:
      case ACTION_DECREMENT: { // set,increment,decrement param+target value for all channels in mask, while keeping it constrained to TARGET_MIN/MAX

         if (action->param >= TOTAL_PARAMS || action->target >= TOTAL_TARGETS)
            return;

         uint16_t val = action->value;
         for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (~action->ch_mask & (1 << ch_index))
               continue;

            if (action->type == ACTION_INCREMENT) {
               val = parameter_get(ch_index, action->param, action->target) + action->value;
            } else if (action->type == ACTION_DECREMENT) {
               val = parameter_get(ch_index, action->param, action->target) - action->value;
            }

            if (action->target == TARGET_VALUE) { // Limit target value between TARGET_MIN and TARGET_MAX
               const uint16_t min = parameter_get(ch_index, action->param, TARGET_MIN);
               const uint16_t max = parameter_get(ch_index, action->param, TARGET_MAX);

               if (val > max) {
                  val = max;
               } else if (val < min) {
                  val = min;
               }
            }

            parameter_set(ch_index, action->param, action->target, val);
         }
         break;
      }
      case ACTION_ENABLE: // Enable channel generation for mask, with optional delayed disable in milliseconds
         pulse_gen.en_mask |= action->ch_mask;
         if (action->value > 0 && action->ch_mask) // add_alarm_in_ms doesn't copy user_data, so use user_data as the value instead of a pointer
            add_alarm_in_ms(action->value, ch_gen_mask_disable_cb, (void*)((int)action->ch_mask), true);
         break;
      case ACTION_DISABLE: // Disable channel generation for mask, with optional delayed enable in milliseconds
         pulse_gen.en_mask &= ~action->ch_mask;
         if (action->value > 0 && action->ch_mask) // add_alarm_in_ms doesn't copy user_data, so use user_data as the value instead of a pointer
            add_alarm_in_ms(action->value, ch_gen_mask_enable_cb, (void*)((int)action->ch_mask), true);
         break;
      case ACTION_TOGGLE: // Toggle channel generation for mask, with optional delayed toggle in milliseconds
         pulse_gen.en_mask ^= action->ch_mask;
         if (action->value > 0 && action->ch_mask) // add_alarm_in_ms doesn't copy user_data, so use user_data as the value instead of a pointer
            add_alarm_in_ms(action->value, ch_gen_mask_toggle_cb, (void*)((int)action->ch_mask), true);
         break;
      case ACTION_EXECUTE:                                                                     // Run another action list from this list
         execute_action_list_nested(action->value >> 8, action->value & 0xff, nest_count + 1); // start:upper byte, end: lower byte
         break;
      case ACTION_PARAM_UPDATE: { // Update parameter step/rate using channel mask
         for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (action->ch_mask & (1 << ch_index))
               parameter_update(ch_index, action->param);
         }
         break;
      }
      default:
         break;
   }
}

void execute_action_list(uint8_t al_start, uint8_t al_end) {
   execute_action_list_nested(al_start, al_end, 0);
}

// Update the parameter value by stepping based on the current parameter mode and step rate.
// Handles condition/actions when parameter reaches extent based on mode.
static inline void parameter_step(uint8_t ch_index, param_t param) {
   parameter_t* p = &generators[ch_index].parameters[param];

   // Get the mode without the flag bits
   const uint16_t mode_raw = parameter_get(ch_index, param, TARGET_MODE);
   const uint16_t mode = mode_raw & ~TARGET_MODE_FLAG;

   // Skip update if parameter is static
   if (mode == TARGET_MODE_DISABLED || parameter_get(ch_index, param, TARGET_RATE) == 0 || p->step == 0)
      return;

   // Only update at required time
   if ((time_us_32() - p->last_update_time_us) < p->update_period_us)
      return;

   p->last_update_time_us = time_us_32();

   // Save current value to check for wrapping
   const uint16_t previous_value = parameter_get(ch_index, param, TARGET_VALUE);
   uint16_t value = previous_value + p->step; // Step value

   const uint16_t min = parameter_get(ch_index, param, TARGET_MIN);
   const uint16_t max = parameter_get(ch_index, param, TARGET_MAX);

   // Check if value reached min/max or wrapped
   const bool end_reached = (value <= min) || (value >= max) || (p->step < 0 && value > previous_value) || (p->step > 0 && value < previous_value);
   if (end_reached) {
      switch (mode) {
         case TARGET_MODE_DOWN_UP:
         case TARGET_MODE_UP_DOWN: { // Invert step direction if UP/DOWN mode
            value = p->step < 0 ? min : max;
            p->step *= -1;
            break;
         }
         case TARGET_MODE_UP_RESET: // Reset to MIN if sawtooth mode
            value = min;
            break;
         case TARGET_MODE_DOWN_RESET: // Reset to MAX if reversed sawtooth mode
            value = max;
            break;
         case TARGET_MODE_UP: // Disable cycling for no-reset modes while keeping flag bits
            value = max;
            parameter_set(ch_index, param, TARGET_MODE, (mode_raw & TARGET_MODE_FLAG) | TARGET_MODE_DISABLED);
            break;
         case TARGET_MODE_DOWN: // Disable cycling for no-reset modes while keeping flag bits
            value = min;
            parameter_set(ch_index, param, TARGET_MODE, (mode_raw & TARGET_MODE_FLAG) | TARGET_MODE_DISABLED);
            break;
         default:
            return;
      }
   }

   parameter_set(ch_index, param, TARGET_VALUE, value); // Update value

   if (end_reached) {
      // Parameter value extent reached, run action list if specified
      const uint16_t al = parameter_get(ch_index, param, TARGET_ACTION_RANGE);
      execute_action_list(al >> 8, al & 0xff); // start:upper byte, end: lower byte
   }
}

void parameter_update(uint8_t ch_index, param_t param) {
   if (ch_index >= CHANNEL_COUNT || param >= TOTAL_PARAMS)
      return;

   // Get the mode without flag bits
   const uint16_t mode = parameter_get(ch_index, param, TARGET_MODE) & ~TARGET_MODE_FLAG;

   // Determine steps and update period based on cycle rate
   const uint16_t rate = parameter_get(ch_index, param, TARGET_RATE);
   if (mode != TARGET_MODE_DISABLED && rate != 0) {
      parameter_t* p = &generators[ch_index].parameters[param];

      const int8_t previous_step = p->step;

      p->step = 1; // Start at 1 step for highest resolution

      const uint16_t min = parameter_get(ch_index, param, TARGET_MIN);
      const uint16_t max = parameter_get(ch_index, param, TARGET_MAX);
      while (p->step < 100) {
         uint32_t delta = (max - min) / p->step;

         if (delta == 0) { // If value range is zero, soft disable stepping
            p->step = 0;
            break;
         }

         // Number of microseconds it takes to go from one extent to another
         // rate is currently in millihertz, making the max rate be ~65 Hz
         uint32_t period = 1000000000 / rate;

         if (period >= delta) {                   // Delay is 1us or greater
            p->update_period_us = period / delta; // Number of microseconds between each value
            break;
         } else {
            p->step++; // Increase step so update period will be 1us or more
         }
      }

      // Invert step direction if MODE_DOWN or out of sync
      if (mode == TARGET_MODE_DOWN_RESET || mode == TARGET_MODE_DOWN || (mode == TARGET_MODE_DOWN_UP && previous_step > 0) ||
          (mode == TARGET_MODE_UP_DOWN && previous_step < 0))
         p->step = -(p->step);
   }
}