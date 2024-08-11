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
#ifndef _SWX_H
#define _SWX_H

#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include "version.h"

#include "util/gpio.h"

// Write debugging information to stdout. Includes trailing zero byte to reset receiver line buffer for COBS framing.
#define LOG_WRITE(lvl, fmt, ...)                                                                                                                                         \
   do {                                                                                                                                                                  \
      printf("(" lvl ") " fmt "\n", ##__VA_ARGS__);                                                                                                                      \
      fputc(0, stdout);                                                                                                                                                  \
      fflush(stdout);                                                                                                                                                    \
   } while (0)

#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) LOG_WRITE("D", fmt, ##__VA_ARGS__) // general debugging
#define LOG_FINE(fmt, ...) LOG_WRITE("*", fmt, ##__VA_ARGS__)  // inner loops, etc
#else
#define LOG_DEBUG(fmt, ...)
#define LOG_FINE(fmt, ...)
#endif

#define LOG_INFO(fmt, ...) LOG_WRITE("I", fmt, ##__VA_ARGS__) // general info

#define LOG_WARN(fmt, ...) LOG_WRITE("W", fmt, ##__VA_ARGS__)  // recoverable warnings
#define LOG_ERROR(fmt, ...) LOG_WRITE("E", fmt, ##__VA_ARGS__) // errors (usually not recoverable)

// Errors that are not recoverable
// Note: Using custom panic handler (hence the *** PANIC ***)
#define LOG_FATAL(fmt, ...) panic("\n\n*** PANIC ***\n(F) " fmt "\n", ##__VA_ARGS__)

#define HZ_TO_US(hz) (1000000u / (hz))
#define US_TO_HZ(us) (1000000.0f / (us))

#define KHZ_TO_US(hz) (1000u / (hz))
#define US_TO_KHZ(us) (1000.0f / (us))

#ifndef PIN_ADC_BASE
#define PIN_ADC_BASE (26)
#endif

static inline float fclamp(float val, float min, float max) {
   if (val < min) {
      return min;
   } else if (val > max) {
      return max;
   }
   return val;
}

// Turns off power by unlatching soft power switch
void swx_power_off();

#endif // _SWX_H