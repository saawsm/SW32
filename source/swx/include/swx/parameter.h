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
#ifndef _PARAMETER_H
#define _PARAMETER_H

#include <stdint.h>

#define MAX_SEQUENCES (255)
#define MAX_ACTIONS (255)
#define MAX_TRIGGERS (64)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   /// The intensity of the signal as a percent (0 to 65535).
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
} param_t;

typedef enum {
   /// The actual parameter value.
   TARGET_VALUE = 0,

   /// The minimum the parameter value can be for parameter cycling.
   TARGET_MIN,

   /// The maximum the parameter value can be for parameter cycling.
   TARGET_MAX,

   /// The frequency of parameter cycling in mHz (millihertz, 1 Hz = 1000 mHz)
   TARGET_RATE,

   /// The cycling mode the parameter is using. Determines how the value resets when it reaches min/max.
   /// Provides bit flags (see TARGET_MODE_FLAG*), these act as hints for user menu behaviour/appearance of targets.
   TARGET_MODE,

   /// Execute actions between start/end indices when value reaches min/max. Lower byte contains end index, upper byte contains start index.
   /// Disable by setting upper and lower bytes equal
   TARGET_ACTION_RANGE,

   TOTAL_TARGETS // Number of targets in enum, used for arrays

} target_t;

typedef enum {
   TARGET_MODE_DISABLED = 0,

   /// Ramp smoothly between minimum and maximum (defaults to incrementing)
   TARGET_MODE_UP_DOWN,

   // Ramp smoothly between maximum and minimum (same as UP_DOWN, but starts with decrementing)
   TARGET_MODE_DOWN_UP,

   /// Ramp smoothly from minimum to maximum and then reset at minimum again.
   TARGET_MODE_UP_RESET,

   /// Ramp smoothly from maximum to minimum and then reset at maximum again.
   TARGET_MODE_DOWN_RESET,

   /// Ramp smoothly from minimum to maximum and then disable cycling.
   TARGET_MODE_UP,

   /// Ramp smoothly from maximum to minimum and then disable cycling.
   TARGET_MODE_DOWN,

} target_mode_t;

#define TARGET_MODE_FLAG (3 << 6)          // Mask bits.
#define TARGET_MODE_FLAG_HIDDEN (1 << 6)   // If set, param target should be hidden in user menus.
#define TARGET_MODE_FLAG_READONLY (2 << 6) // If set, param target should be readonly in user menus.

typedef enum {
   ACTION_NONE = 0,

   /// Set a parameter target value.
   ACTION_SET,

   /// Increment a parameter target value. Action value is the amount to increment by.
   ACTION_INCREMENT,

   /// Decrement a parameter target value. Action value is the amount to decrement by.
   ACTION_DECREMENT,

   /// Enable pulse generation on one or more channels. If value is above zero, delay in milliseconds before disabling generation.
   ACTION_ENABLE,

   /// Disable pulse generation on one or more channels. If value is above zero, delay in milliseconds before enabling generation.
   ACTION_DISABLE,

   /// Toggle pulse generation on one or more channels. If value is above zero, delay in milliseconds before toggling generation again.
   ACTION_TOGGLE,

   /// Run another action list. Value contains index range for list. Upper byte is start index, lower byte is end index.
   ACTION_EXECUTE,

   /// Update a parameter for one or more channels.
   ACTION_PARAM_UPDATE,

   TOTAL_ACTION_TYPES, // Number of action types in enum.
} action_type_t;

typedef enum {
   TRIGGER_OP_DDD = 0, // disabled
   TRIGGER_OP_OOO,     // t1 || t2 || t3 || t4
   TRIGGER_OP_OOA,     // t1 || t2 || t3 && t4
   TRIGGER_OP_OAO,     // t1 || t2 && t3 || t4
   TRIGGER_OP_OAA,     // t1 || t2 && t3 && t4
   TRIGGER_OP_AOO,     // t1 && t2 || t3 || t4
   TRIGGER_OP_AOA,     // t1 && t2 || t3 && t4
   TRIGGER_OP_AAO,     // t1 && t2 && t3 || t4
   TRIGGER_OP_AAA,     // t1 && t2 && t3 && t4
   TOTAL_TRIGGER_OPS,
} trigger_op_t;

#define AUDIO_MODE_FLAG (3 << 6)       // Mask bits.
#define AUDIO_MODE_FLAG_POWER (1 << 6) // If set, audio processor will modulate power levels based on volume.
#define AUDIO_MODE_FLAG_PULSE (2 << 6) // If set, audio processor will generate pulses for each zero crossing.

#ifdef __cplusplus
}
#endif

#endif // _PARAMETER_H