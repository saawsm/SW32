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
#include "swx.h"

#include <stdarg.h>

#include <pico/multicore.h>
#include <hardware/i2c.h>

#include "filesystem.h"
#include "analog_capture.h"
#include "trigger.h"
#include "output.h"
#include "pulse_gen.h"

#include "protocol.h"

static inline void init() {
   init_gpio(PIN_PWR_CTRL, GPIO_IN, false);
   gpio_pull_up(PIN_PWR_CTRL); // latch board power on

   init_gpio(PIN_DISCH_EN, GPIO_OUT, false); // TODO: Move to usb-pd
   gpio_disable_pulls(PIN_DISCH_EN);

   init_gpio(PIN_INT_PD, GPIO_IN, false); // active low input   TODO: Move to usb-pd
   gpio_disable_pulls(PIN_INT_PD);

   // Init normal I2C bus
   i2c_init(I2C_PORT, I2C_FREQ);
   gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
   gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
   gpio_disable_pulls(PIN_I2C_SDA); // use hardware pullups
   gpio_disable_pulls(PIN_I2C_SCL);

   bool clk_success = set_sys_clock_khz(250000, false); // try set clock to 250MHz
   stdio_init_all();                                    // needs to be called after setting clock

   static const char* const swxVersion = SWX_VERSION_STR;
   LOG_INFO("~~ swx driver %s ~~", swxVersion);
   LOG_INFO("Starting up...");

   if (clk_success) {
      LOG_DEBUG("sys_clk set to 250MHz");
   }
}

void core1_main() {
   multicore_lockout_victim_init();

   while (true) {
      output_process_power();
      output_process_pulse();
   }
}

int main() {
   // Initialize hardware
   init();

   // Initialize output driver
   output_init();

   // Initialize free running ADC capture
   // Needs to be called after output_init(), since output_init() briefly uses the ADC during output calibration
   analog_capture_init();

   // Start core1 (needs to start before any file system operations so multicore lockout victim is set)
   multicore_reset_core1();
   multicore_launch_core1(core1_main);

   // Initialize flash filesystem by mounting (and optionally formatting if required)
   LOG_DEBUG("Mounting filesystem...");
   int err = fs_flash_mount(true);
   if (err) { // should not happen - mounting and formatting failed
      LOG_FATAL("Mounting failed! err=%u (%s)", err, lfs_err_msg(err));
   }

   // Initialize parametric pulse generation
   pulse_gen_init();

   // Initialize input trigger handling
   trigger_init();

   // Initialize UART and protocol handling
   protocol_init();

   while (true) {
      protocol_process();

      pulse_gen_process();
      trigger_process();
   }
}

void swx_power_off() {
   LOG_FINE("Shutdown...");
   gpio_put(PIN_PWR_CTRL, 0);
   gpio_set_dir(PIN_PWR_CTRL, GPIO_OUT);
}

// Custom exit() handler
void __attribute__((noreturn)) _exit(__unused int status) {
   stdio_flush();
   sleep_ms(10); // fixes log message not getting printed during a panic() or LOG_FATAL()

   while (1) {
      __breakpoint();
   }
}

// Custom panic() handler
void __attribute__((noreturn)) __printflike(1, 0) PICO_PANIC_FUNCTION(const char* fmt, ...) {
   output_scram(); // shutdown driver board

   va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);

   _exit(1);
}