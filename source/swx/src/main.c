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

#include <task.h>

static inline void init() {

   bool clk_success = set_sys_clock_khz(250000, false); // try set clock to 250MHz
   stdio_init_all();                                    // needs to be called after setting clock

   LOG_INFO("~~ swx driver %s ~~\n", SWX_VERSION_STR);
   LOG_INFO("Starting up...\n");

   if (clk_success) {
      LOG_DEBUG("sys_clk set to 250MHz\n");
   }
}

static void run_task(void* arg) {
   (void)arg;

   while (true) {

      vTaskDelay(pdMS_TO_TICKS(250));
   }
}

int main() {
   // Initialize hardware
   init();

   xTaskCreate(run_task, "run_task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);

   // Startup FreeRTOS
   vTaskStartScheduler();

   panic("Unsupported! Task scheduler returned!"); // should never reach here
}