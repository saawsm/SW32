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
#include "trigger.h"

#include "pulse_gen.h"
#include "analog_capture.h"

static uint32_t last_update_time_us = 0;

typedef struct {
   bool last_result;
   uint32_t last_exec_time_us;
} state_t;

static state_t states[MAX_TRIGGERS] = {0};

trigger_t triggers[MAX_TRIGGERS] = {0};

uint8_t trig_input_states = 0;

void trigger_init() {
   LOG_DEBUG("Init triggers...");

   init_gpio(PIN_TRIG_A1, GPIO_IN, false); // active low input
   init_gpio(PIN_TRIG_A2, GPIO_IN, false); // active low input
   init_gpio(PIN_TRIG_B1, GPIO_IN, false); // active low input
   init_gpio(PIN_TRIG_B2, GPIO_IN, false); // active low input
   gpio_disable_pulls(PIN_TRIG_A1);
   gpio_disable_pulls(PIN_TRIG_A2);
   gpio_disable_pulls(PIN_TRIG_B1);
   gpio_disable_pulls(PIN_TRIG_B2);
}

void trigger_process() {
   // only process triggers every 10 ms
   if ((time_us_32() - last_update_time_us) < 10000u)
      return;
   last_update_time_us = time_us_32();

   // get trigger IO state as bit field (LSB is trigger 1)
   trig_input_states = (gpio_get(PIN_TRIG_A1) << 0) | (gpio_get(PIN_TRIG_A2) << 1) | (gpio_get(PIN_TRIG_B1) << 2) | (gpio_get(PIN_TRIG_B2) << 3);

   for (size_t trig_index = 0; trig_index < MAX_TRIGGERS; trig_index++) {
      trigger_t* trigger = &triggers[trig_index];

      bool has_input = trigger->input_mask && trigger->op;
      bool has_input_audio = trigger->input_audio;

      if (!trigger->enabled || (!has_input && !has_input_audio) || trigger->action_start_index == trigger->action_end_index)
         continue;

      bool result = false;
      if (has_input) {
         const uint8_t trig_state = (trig_input_states & trigger->input_mask) ^ trigger->input_invert_mask;

         switch (trigger->op) {
            case TRIGGER_OP_OOO: // t1 || t2 || t3 || t4
               result = !!trig_state;
               break;
            case TRIGGER_OP_OOA: // t1 || t2 || t3 && t4
               result = !!(trig_state & 0b0011) || ((trig_state & 0b1100) == 0b1100);
               break;
            case TRIGGER_OP_OAO: // t1 || t2 && t3 || t4
               result = !!(trig_state & 0b1001) || ((trig_state & 0b0110) == 0b0110);
               break;
            case TRIGGER_OP_OAA: // t1 || t2 && t3 && t4
               result = !!(trig_state & 0b0001) || ((trig_state & 0b1110) == 0b1110);
               break;
            case TRIGGER_OP_AOO: // t1 && t2 || t3 || t4
               result = !!(trig_state & 0b1100) || ((trig_state & 0b0011) == 0b0011);
               break;
            case TRIGGER_OP_AOA: // t1 && t2 || t3 && t4
               result = ((trig_state & 0b1100) == 0b1100) || ((trig_state & 0b0011) == 0b0011);
               break;
            case TRIGGER_OP_AAO: // t1 && t2 && t3 || t4
               result = !!(trig_state & 0b1000) || ((trig_state & 0b0111) == 0b0111);
               break;
            case TRIGGER_OP_AAA: // t1 && t2 && t3 && t4
               result = (trig_state == 0b1111);
               break;
            default:
               continue;
         }

         result ^= trigger->output_invert;
      }

      if (has_input_audio) {
         size_t samples;
         uint16_t* buffer;
         uint32_t capture_end_time_us;
         buf_stats_t stats;

         fetch_analog_buffer(trigger->input_audio, &samples, &buffer, &capture_end_time_us, &stats, true);

         bool peaked = (trigger->threshold > stats.amplitude) ^ trigger->threshold_invert;
         result = trigger->require_both ? (result && peaked) : (result || peaked);
      }

      state_t* const state = &states[trig_index];
      if (trigger->repeating || result != state->last_result) {
         state->last_result = result;

         if (result && (time_us_32() - state->last_exec_time_us) >= trigger->min_period_us) {
            state->last_exec_time_us = time_us_32();
            execute_action_list(trigger->action_start_index, trigger->action_end_index);
         }
      }
   }
}