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

#define CHANNEL_POWER_MAX (2000)

#ifdef __cplusplus
extern "C" {
#endif

void output_init();

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