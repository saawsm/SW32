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

#include <hardware/i2c.h>

#include "output.h"
#include "pulse_gen.h"

#include "hardware/bq25703a.h"

static inline void init() {
   init_gpio(PIN_PWR_CTRL, GPIO_IN, false);
   gpio_pull_up(PIN_PWR_CTRL); // latch board power on

   init_gpio(PIN_DRV_EN, GPIO_OUT, false);
   init_gpio(PIN_PIP_EN, GPIO_OUT, true); // active low output
   init_gpio(PIN_DISCH_EN, GPIO_OUT, false);

   init_gpio(PIN_INT_PD, GPIO_IN, false); // active low input

   init_gpio(PIN_TRIG_A1, GPIO_IN, false); // active low input
   init_gpio(PIN_TRIG_A2, GPIO_IN, false); // active low input
   init_gpio(PIN_TRIG_B1, GPIO_IN, false); // active low input
   init_gpio(PIN_TRIG_B2, GPIO_IN, false); // active low input

   // Init normal I2C bus
   i2c_init(I2C_PORT, I2C_FREQ);
   gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
   gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
   gpio_disable_pulls(PIN_I2C_SDA); // use hardware pullups
   gpio_disable_pulls(PIN_I2C_SCL);

   // Init DAC I2C bus
   i2c_init(I2C_PORT_DAC, I2C_FREQ_DAC);
   gpio_set_function(PIN_I2C_SDA_DAC, GPIO_FUNC_I2C);
   gpio_set_function(PIN_I2C_SCL_DAC, GPIO_FUNC_I2C);
   gpio_disable_pulls(PIN_I2C_SDA_DAC); // use hardware pullups
   gpio_disable_pulls(PIN_I2C_SCL_DAC);

   bool clk_success = set_sys_clock_khz(250000, false); // try set clock to 250MHz
   stdio_init_all();                                    // needs to be called after setting clock

   LOG_INFO("~~ swx driver %s ~~\n", SWX_VERSION_STR);
   LOG_INFO("Starting up...\n");

   if (clk_success) {
      LOG_DEBUG("sys_clk set to 250MHz\n");
   }
}

static void __always_inline(run)() {
   //
}

int main() {
   // Initialize hardware
   init();



   while (true) {
      run();
   }
}