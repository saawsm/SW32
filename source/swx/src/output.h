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
#ifndef _OUTPUT_H
#define _OUTPUT_H

#include "swx.h"
#include "channel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   const uint8_t pin_gate_a; // GPIO pin for NFET gate A
   const uint8_t pin_gate_b; // GPIO pin for NFET gate B

   const uint8_t dac_channel;

   uint16_t cal_value;

   channel_status_t status;

   bool enabled;               // True, if this channel is generating output from the pulse generator (period_us) or the audio_src
   analog_channel_t audio_src; // Analog channel for audio pulse generation

   uint32_t last_power_time_us; // The absolute timestamp for the last power set
   uint32_t last_pulse_time_us; // The absolute timestamp for the last generated output pulse

   uint32_t period_us;      // Pulse period in microseconds, see. HZ_TO_US() macro
   uint16_t pulse_width_us; // Pulse width in microseconds

   float power_level; // Power level of channel (e.g. front panel level knob) range [0.0 - 1.0]
   float power;       // Routine power level, range [0.0 - 1.0]
} channel_t;

extern channel_t channels[CHANNEL_COUNT];

void output_init();
void output_scram();

void pulse_gen_process();

void output_process_power();
void output_process_pulse();

bool output_pulse(uint8_t ch_index, uint16_t pos_us, uint16_t neg_us, uint32_t abs_time_us);
void output_set_power(uint8_t ch_index, uint16_t power);

bool output_drv_enable(bool enabled);
bool output_drv_enabled();

bool check_output_board_missing();

#ifdef __cplusplus
}
#endif

#endif // _OUTPUT_H