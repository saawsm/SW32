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

// See pico_enable_stdio_usb() and pico_enable_stdio_uart() in CMakeLists for log targets
#ifndef NDEBUG
#define LOG_DEBUG(...) printf(__VA_ARGS__) // general debugging
#define LOG_FINE(...) printf(__VA_ARGS__)  // inner loops, etc
#else
#define LOG_DEBUG(...)
#define LOG_FINE(...)
#endif

#define LOG_INFO(...) printf(__VA_ARGS__) // general info

#define LOG_WARN(...) printf(__VA_ARGS__)  // recoverable warnings
#define LOG_ERROR(...) printf(__VA_ARGS__) // errors (usually not recoverable)

// Errors that are not recoverable
// Alternative to panic() that has unreliable stdio output
#define LOG_FATAL(...)                                                                                                                                                   \
   do {                                                                                                                                                                  \
      LOG_ERROR("\n*** PANIC ***\n");                                                                                                                                    \
      LOG_ERROR(__VA_ARGS__);                                                                                                                                            \
      sleep_ms(10);                                                                                                                                                      \
      extern void _exit(int status);                                                                                                                                     \
      _exit(1);                                                                                                                                                          \
   } while (0);

#define HZ_TO_US(hz) (1000000ul / (hz))
#define KHZ_TO_US(hz) (1000ul / (hz))

// Turns off power by unlatching soft power switch
void swx_power_off();

#endif // _SWX_H