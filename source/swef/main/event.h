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
#ifndef _EVENT_H
#define _EVENT_H

#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(INPUT_EVENTS);

typedef enum {
   BOTTON_NONE = 0,

   BUTTON_TOP_LEFT,
   BUTTON_TOP_RIGHT,
   BUTTON_BOTTOM_RIGHT,
   BUTTON_BOTTOM_LEFT,

   BUTTON_ENC,
} input_button_t;

typedef struct {
   input_button_t btn;
   uint8_t count; // number of clicks
} event_input_args_t;

typedef enum {
   EVENT_NONE = 0,

   /************************************/

   EVENT_INPUT_BUTTON_CLICKED = 1 << 0,
   EVENT_INPUT_BUTTON_PRESSED_LONG = 1 << 1,

   EVENT_INPUT_ENC_ROTATED = 1 << 2,

   EVENT_INPUT_CHANNEL_LEVELS_CHANGED = 1 << 3,

   /************************************/

} event_types_t;

#ifdef __cplusplus
}
#endif

#endif // _EVENT_H