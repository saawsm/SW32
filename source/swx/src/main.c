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

#include <pico/multicore.h>
#include <hardware/i2c.h>

#include "filesystem.h"
#include "input.h"
#include "output.h"
#include "pulse_gen.h"

#include "protocol.h"

#include "hardware/bq25703a.h"

static inline void init() {
   input_init();

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

   const char* const swxVersion = SWX_VERSION_STR;
   LOG_INFO("~~ swx driver %s ~~\n", swxVersion);
   LOG_INFO("Starting up...\n");

   if (clk_success) {
      LOG_DEBUG("sys_clk set to 250MHz\n");
   }
}

void core1_main() {
   while (true) {
      pulse_gen_process();

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

   // Initialize pulse generator
   pulse_gen_init();

   // Initialize flash filesystem by mounting (and optionally formatting if required)
   LOG_DEBUG("Mounting filesystem...\n");
   int err = fs_flash_mount(true);
   if (err) { // should not happen since this is non-removable flash memory
      LOG_FATAL("Mounting failed! err=%u (%s)\n", err, lfs_err_msg(err));
   }


   // Initialize UART and protocol handling
   protocol_init();

   // Start core1
   multicore_reset_core1();
   multicore_launch_core1(core1_main);

   while (true) {
      protocol_process();
   }
}
