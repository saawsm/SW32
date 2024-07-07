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

typedef enum {
   LOG_LEVEL_NONE = 0,
   LOG_LEVEL_FINE,
   LOG_LEVEL_DEBUG,
   LOG_LEVEL_INFO,
   LOG_LEVEL_WARN,
   LOG_LEVEL_ERROR,
   LOG_LEVEL_FATAL,
} log_level_t;

extern void write_log(log_level_t level, char* fmt, ...);

#ifndef NDEBUG
#define LOG_DEBUG(...) write_log(LOG_LEVEL_DEBUG, __VA_ARGS__) // general debugging
#define LOG_FINE(...) write_log(LOG_LEVEL_FINE, __VA_ARGS__)   // inner loops, etc
#else
#define LOG_DEBUG(...)
#define LOG_FINE(...)
#endif

#define LOG_INFO(...) write_log(LOG_LEVEL_INFO, __VA_ARGS__) // general info

#define LOG_WARN(...) write_log(LOG_LEVEL_WARN, __VA_ARGS__)   // recoverable warnings
#define LOG_ERROR(...) write_log(LOG_LEVEL_ERROR, __VA_ARGS__) // errors (usually not recoverable)

// Errors that are not recoverable
// Alternative to panic() that has unreliable stdio output
#define LOG_FATAL(...)                                                                                                                                                   \
   do {                                                                                                                                                                  \
      write_log(LOG_LEVEL_FATAL, "\n\n*** PANIC ***\n");                                                                                                                 \
      write_log(LOG_LEVEL_FATAL, __VA_ARGS__);                                                                                                                           \
      sleep_ms(10);                                                                                                                                                      \
      extern void _exit(int status);                                                                                                                                     \
      _exit(1);                                                                                                                                                          \
   } while (0)

#define HZ_TO_US(hz) (1000000ul / (hz))
#define KHZ_TO_US(hz) (1000ul / (hz))

#ifndef PIN_ADC_BASE
#define PIN_ADC_BASE (26)
#endif

// Turns off power by unlatching soft power switch
void swx_power_off();

#endif // _SWX_H