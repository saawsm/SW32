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
#include "channel.h"

#define STATE_COUNT (4)
static const gen_param_t STATE_SEQUENCE[STATE_COUNT] = {
    PARAM_ON_RAMP_TIME,
    PARAM_ON_TIME,
    PARAM_OFF_RAMP_TIME,
    PARAM_OFF_TIME,
};

typedef struct {
   bool enabled; // True, if this channel is generating

   uint8_t state_index; // The current "waveform" state (e.g. off, on_ramp, on)

   uint32_t last_state_time_us; // The absolute timestamp for the last "waveform" state change (e.g. off -> on_ramp -> on)
   uint32_t last_pulse_time_us; // The absolute timestamp for the last generated output pulse

   uint32_t period_us; // Waveform period

   float power_level; // Power level of the channel (e.g. front panel level knob) in 0.0 - 1.0
   float power;       // Routine power - PARAM_POWER [0.0 - 1.0]

   uint16_t parameters[TOTAL_PARAMS];
} gen_channel_t;

static gen_channel_t channels[CHANNEL_COUNT] = {0};
static uint32_t last_time_us = 0;

void pulse_gen_init() {
   LOG_DEBUG("Init pulse generator...\n");

   // init defaults
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      channel_set_enabled(ch_index, false);

      // Must use channel_set_param() and channel_set_power() to update computed/cached values
      channel_set_param(ch_index, PARAM_POWER, CHANNEL_POWER_MAX); // note: not max power since other power levels are combined with this one
      channel_set_param(ch_index, PARAM_FREQUENCY, 1800);          // 180 Hz
      channel_set_param(ch_index, PARAM_PULSE_WIDTH, 150);         // 150 us

      channel_set_power_level(ch_index, 1000); // 50% - set channel global power level  (front panel level knob)
   }
}

static inline uint16_t ramp_time_us(const gen_channel_t* const ch) {
   return ch->parameters[STATE_SEQUENCE[ch->state_index]] * 1000u;
}

void pulse_gen_process() {
   const uint32_t time_us = time_us_32();
   const uint32_t delta_us = time_us - last_time_us;

   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      gen_channel_t* const ch = &channels[ch_index];

      if (!ch->enabled)
         continue;

      // Increment state if enough time has passed
      if (time_us_32() - ch->last_state_time_us > ramp_time_us(ch)) {
         ch->last_state_time_us = time_us_32();

         ch->state_index++;
         if (ch->state_index >= STATE_COUNT)
            ch->state_index = 0; // Increment or reset after 4 states (on_ramp, on, off_ramp, off)
      }

      float power_modifier = 1.0f;

      // Scale power level depending on the current state (e.g. transition between off and on)
      const gen_param_t state = STATE_SEQUENCE[ch->state_index];
      switch (state) {
         case PARAM_ON_RAMP_TIME:    // Ramp power from zero to power value
         case PARAM_OFF_RAMP_TIME: { // Ramp power from power value to zero
            uint16_t ramp_us = ramp_time_us(ch);
            if (ramp_us == 0)
               break;

            const uint32_t time_remaining_us = ramp_time_us(ch) - (time_us_32() - ch->last_state_time_us);

            power_modifier = (float)time_remaining_us / ramp_us;
            if (power_modifier > 1.0f)
               power_modifier = 1.0f;

            if (state == PARAM_ON_RAMP_TIME) // Inverse percentage if transition is going from off to on
               power_modifier = 1.0f - power_modifier;
            break;
         }
         case PARAM_OFF_TIME: // If the state is off, continue to next channel
            continue;
         default:
            break;
      }

      power_modifier *= ch->power * ch->power_level;

      const uint16_t real_power = (uint16_t)(power_modifier * CHANNEL_POWER_MAX);

      output_set_power(ch_index, real_power);

      if (time_us_32() - ch->last_pulse_time_us >= ch->period_us - delta_us) {
         ch->last_pulse_time_us = time_us_32();

         const uint16_t pw = ch->parameters[PARAM_PULSE_WIDTH];
         output_pulse(ch_index, pw, pw, time_us_32());
      }
   }

   last_time_us = time_us;
}

void channel_set_enabled(uint8_t ch_index, bool enabled) {
   if (ch_index >= CHANNEL_COUNT)
      return;

   gen_channel_t* const ch = &channels[ch_index];
   if (enabled && !ch->enabled) { // reset state if going from disabled->enabled
      ch->state_index = 0;
      ch->last_state_time_us = time_us_32();
      ch->last_pulse_time_us = time_us_32();
   }

   ch->enabled = enabled;
}

bool channel_get_enabled(uint8_t ch_index) {
   if (ch_index >= CHANNEL_COUNT)
      return false;
   return channels[ch_index].enabled;
}

void channel_set_param(uint8_t ch_index, gen_param_t param, uint16_t value) {
   if (ch_index >= CHANNEL_COUNT || param >= TOTAL_PARAMS)
      return;

   gen_channel_t* const ch = &channels[ch_index];

   switch (param) {
      case PARAM_FREQUENCY: {
         if (value > 5000)
            value = 5000;
         ch->period_us = 10000000ul / value; // dHz -> us
         break;
      }
      case PARAM_POWER: {
         if (value > CHANNEL_POWER_MAX)
            value = CHANNEL_POWER_MAX;
         ch->power = value * (1.0f / CHANNEL_POWER_MAX);
         break;
      }
      default:
         break;
   }

   ch->parameters[param] = value;
}

uint16_t channel_get_param(uint8_t ch_index, gen_param_t param) {
   if (ch_index >= CHANNEL_COUNT || param >= TOTAL_PARAMS)
      return 0;

   return channels[ch_index].parameters[param];
}

void channel_set_power_level(uint8_t ch_index, uint16_t power) {
   if (ch_index >= CHANNEL_COUNT)
      return;

   if (power > CHANNEL_POWER_MAX)
      power = CHANNEL_POWER_MAX;

   channels[ch_index].power_level = power * (1.0f / CHANNEL_POWER_MAX);
}

uint16_t channel_get_power_level(uint8_t ch_index) {
   if (ch_index >= CHANNEL_COUNT)
      return 0;

   return CHANNEL_POWER_MAX * channels[ch_index].power_level;
}
