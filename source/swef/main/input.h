/*
 * swef
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
#ifndef _INPUT_H
#define _INPUT_H

#include <esp_err.h>

#define CHANNEL_COUNT (4)

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t channel_levels[CHANNEL_COUNT];

esp_err_t input_init();

#ifdef __cplusplus
}
#endif

#endif // _INPUT_H