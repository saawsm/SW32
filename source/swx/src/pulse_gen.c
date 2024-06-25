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

#include <libfixmath/fixmath.h>
#include "output.h"

#define MIN_STEP_US (10)

#define STATE_COUNT (4)
static const gen_param_t STATE_SEQUENCE[STATE_COUNT] = {
    PARAM_ON_RAMP_TIME,
    PARAM_ON_TIME,
    PARAM_OFF_RAMP_TIME,
    PARAM_OFF_TIME,
};

typedef struct {
   bool enabled; // True, if this channel is generating

   uint8_t state_index;         // The current "waveform" state (e.g. off, on_ramp, on)
   uint32_t next_state_time_us; // The absolute timestamp for the next "waveform" state change (e.g. off -> on_ramp -> on)

   fix16_t old_y; // last waveform state (see zero_crossing_between_time_points())

   fix16_t power; // "global" power level for channel (front panel level knob) in 0.0 - 1.0

   fix16_t cached_param_power;
   fix16_t cached_param_phase;
   fix16_t cached_param_frequency;

   uint16_t parameters[TOTAL_PARAMS];
} gen_channel_t;

static const fix16_t FIX16_MIN_STEP_US = F16(MIN_STEP_US);
static const fix16_t FIX16_MAX_POWER = F16(CHANNEL_POWER_MAX);

static gen_channel_t channels[CHANNEL_COUNT] = {0};
static uint32_t last_time_us = 0;

void pulse_gen_init() {
   LOG_DEBUG("Init pulse generator...\n");

   // init defaults
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      channels[ch_index].enabled = false;

      // Must use pulse_gen_set_param() and pulse_gen_set_power() to update computed/cached values
      pulse_gen_set_param(ch_index, PARAM_POWER, CHANNEL_POWER_MAX); // note: not max power since other power levels are combined with this one
      pulse_gen_set_param(ch_index, PARAM_FREQUENCY, 1800);          // 180 Hz
      pulse_gen_set_param(ch_index, PARAM_PULSE_WIDTH, 150);         // 150 us

      pulse_gen_set_power(ch_index, 0); // set channel global power level  (front panel level knob)
   }
}

static inline uint16_t ramp_time_us(const gen_channel_t* const ch) {
   return ch->parameters[STATE_SEQUENCE[ch->state_index]] * 1000u;
}

static inline uint32_t calc_next_state_time_us(const gen_channel_t* const ch) {
   return time_us_32() + ramp_time_us(ch);
}

static inline fix16_t sine_half_cycle(fix16_t time, fix16_t frequency, fix16_t phase_rad) {
   // sin((PI * frequency * time) + phase_rad)
   return fix16_sin(fix16_add(fix16_mul(fix16_pi, fix16_mul(frequency, time)), phase_rad));
}

// Between "start" and "end" time, check if a zero crossing occurred with the given frequency and phase.
// old_y* is the waveform state of the previous call so we can detect zero crossings between function call boundaries.
static inline bool zero_crossing_between_time_points(uint32_t start_us, uint32_t end_us, uint16_t frequency, uint16_t phase_rad, fix16_t* const old_y) {
   const uint32_t step = (end_us - start_us) / 4;

   // subdivide time delta by ten, limiting minimum step to 10us
   const fix16_t time_step_us = (step < MIN_STEP_US) ? FIX16_MIN_STEP_US : fix16_from_int(step);

   const fix16_t start = fix16_from_int(start_us);
   const fix16_t end = fix16_from_int(end_us);

   fix16_t y = 0;
   for (fix16_t x = start; x < end; x += time_step_us) {
      y = fix16_floor(sine_half_cycle(x, frequency, phase_rad));
      if (*old_y != y)
         return true;
   }

   *old_y = y;
   return false;
}

void pulse_gen_process() {
   const uint32_t time_us = time_us_32();

   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      gen_channel_t* const ch = &channels[ch_index];

      if (!ch->enabled)
         continue;

      if (time_us > ch->next_state_time_us) {
         if (++(ch->state_index) >= STATE_COUNT)
            ch->state_index = 0; // Increment or reset after 4 states (on_ramp, on, off_ramp, off)

         // Set the next state time based on the state parameter (on_ramp_time, on_time, off_ramp_time, off_time)
         ch->next_state_time_us = calc_next_state_time_us(ch);
      }

      fix16_t power_modifier = fix16_one;

      // Scale power level depending on the current state (e.g. transition between off and on)
      const gen_param_t state = STATE_SEQUENCE[ch->state_index];
      switch (state) {
         case PARAM_ON_RAMP_TIME:    // Ramp power from zero to power value
         case PARAM_OFF_RAMP_TIME: { // Ramp power from power value to zero
            uint16_t ramp_us = ramp_time_us(ch);
            if (ramp_us == 0)
               break;

            uint32_t time_remaining_us = ch->next_state_time_us - time_us;

            power_modifier = fix16_min(fix16_div(fix16_from_int(time_remaining_us), fix16_from_int(ramp_us)), fix16_one);

            if (state == PARAM_ON_RAMP_TIME) // Invert percentage if transition is going from off to on
               power_modifier = fix16_one - power_modifier;
            break;
         }
         case PARAM_OFF_TIME: // If the state is off, continue to next channel
            continue;
         default:
            break;
      }

      power_modifier = fix16_mul(power_modifier, ch->cached_param_power);
      power_modifier = fix16_mul(power_modifier, ch->power);

      const uint16_t real_power = fix16_to_int(fix16_mul(power_modifier, FIX16_MAX_POWER));
      output_set_power(ch_index, real_power);

      if (zero_crossing_between_time_points(last_time_us, time_us, ch->cached_param_frequency, ch->cached_param_phase, &ch->old_y)) {
         const uint16_t pw = ch->parameters[PARAM_PULSE_WIDTH];
         output_pulse(ch_index, pw, pw, time_us);
      }
   }

   last_time_us = time_us;
}

void pulse_gen_enable(uint8_t ch_index, bool enabled) {
   if (ch_index >= CHANNEL_COUNT)
      return;

   gen_channel_t* const ch = &channels[ch_index];
   if (enabled && !ch->enabled) { // reset state if going from disabled->enabled
      ch->state_index = 0;
      ch->next_state_time_us = calc_next_state_time_us(ch);
   }

   ch->enabled = enabled;
}

bool pulse_gen_enabled(uint8_t ch_index) {
   if (ch_index >= CHANNEL_COUNT)
      return false;
   return channels[ch_index].enabled;
}

void pulse_gen_set_param(uint8_t ch_index, gen_param_t param, uint16_t value) {
   if (ch_index >= CHANNEL_COUNT || param >= TOTAL_PARAMS)
      return;

   gen_channel_t* const ch = &channels[ch_index];

   switch (param) {
      case PARAM_FREQUENCY: {
         if (value > 5000)
            value = 5000;
         ch->cached_param_frequency = fix16_div(fix16_from_int(value), fix16_from_int(10000000));
         break;
      }
      case PARAM_POWER: {
         if (value > CHANNEL_POWER_MAX)
            value = CHANNEL_POWER_MAX;
         ch->cached_param_power = fix16_div(fix16_from_int(value), FIX16_MAX_POWER);
         break;
      }
      case PARAM_PHASE:
         ch->cached_param_phase = fix16_deg_to_rad(fix16_div(fix16_from_int(value), fix16_from_int(100)));
         break;
      default:
         break;
   }
   ch->parameters[param] = value;
}

uint16_t pulse_gen_get_param(uint8_t ch_index, gen_param_t param) {
   if (ch_index >= CHANNEL_COUNT || param >= TOTAL_PARAMS)
      return 0;

   return channels[ch_index].parameters[param];
}

void pulse_gen_set_power(uint8_t ch_index, uint16_t power) {
   if (ch_index >= CHANNEL_COUNT)
      return;

   if (power > CHANNEL_POWER_MAX)
      power = CHANNEL_POWER_MAX;

   // convert into fix16_t percent
   channels[ch_index].power = fix16_div(fix16_from_int(power), FIX16_MAX_POWER);
}

uint16_t pulse_gen_get_power(uint8_t ch_index) {
   if (ch_index >= CHANNEL_COUNT)
      return 0;

   // convert from a fix16_t percent into value between 0 - CHANNEL_POWER_MAX
   return fix16_to_int(fix16_mul(channels[ch_index].power, FIX16_MAX_POWER));
}
