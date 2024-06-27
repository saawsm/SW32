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

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   /// The intensity of the signal as a percent (0 to 2000). See CHANNEL_POWER_MAX
   PARAM_POWER = 0,

   /// The frequency of pulses generated in dHz (decihertz, 1 Hz = 10 dHz)
   PARAM_FREQUENCY,

   /// The duration of each pulse (0 us to 500 us).
   PARAM_PULSE_WIDTH,

   /// The number of milliseconds the output is on.
   PARAM_ON_TIME,

   /// The duration in milliseconds to smoothly ramp intensity level when going from off to on. `_|‾`
   PARAM_ON_RAMP_TIME,

   /// The number of milliseconds the output is off.
   PARAM_OFF_TIME,

   /// The duration in milliseconds to smoothly ramp intensity level when going from on to off. `‾|_`
   PARAM_OFF_RAMP_TIME,

   TOTAL_PARAMS // Number of parameters in enum, used for arrays
} gen_param_t;

void pulse_gen_init();

void pulse_gen_process();

void pulse_gen_enable(uint8_t ch_index, bool enabled);
bool pulse_gen_enabled(uint8_t ch_index);

void pulse_gen_set_param(uint8_t ch_index, gen_param_t param, uint16_t value);
uint16_t pulse_gen_get_param(uint8_t ch_index, gen_param_t param);

void pulse_gen_set_power_level(uint8_t ch_index, uint16_t power);
uint16_t pulse_gen_get_power_level(uint8_t ch_index);

#ifdef __cplusplus
}
#endif

#endif // _PULSE_GEN_H