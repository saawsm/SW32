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

#ifndef CHANNEL_COUNT
#define CHANNEL_COUNT (4)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   const uint8_t pin_gate_a; // GPIO pin for NFET gate A
   const uint8_t pin_gate_b; // GPIO pin for NFET gate B

   const uint8_t dac_channel;

   uint16_t cal_value;

   channel_status_t status;

   float max_power; // Maximum power level (e.g. front panel knobs), range [0.0, 1.0]
} channel_t;

extern channel_t channels[CHANNEL_COUNT];

// Bitmask indicating if a channel needs max_power to be less than 1% to enable output (bit will be reset to zero)
extern uint8_t require_zero_mask;

void output_init();
void output_scram();

void output_process_power();
void output_process_pulse();

bool output_pulse(uint8_t ch_index, uint16_t pos_us, uint16_t neg_us, uint32_t abs_time_us);
bool output_power(uint8_t ch_index, float power);

bool check_output_board_missing();

#ifdef __cplusplus
}
#endif

#endif // _OUTPUT_H