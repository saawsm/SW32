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
#include <stdio.h>

#include <esp_check.h>
#include <esp_event.h>

#include <nvs_flash.h>

#include "input.h"

static const char* TAG = "main";

void app_main() {
   ESP_LOGI(TAG, "~~ swef ~~");

   // Init non-volatile key-value pair storage, required for WiFi driver.
   ESP_ERROR_CHECK(nvs_flash_init());

   // Init default system event loop
   ESP_ERROR_CHECK(esp_event_loop_create_default());

   ESP_ERROR_CHECK(input_init());
}