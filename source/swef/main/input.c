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
#include "input.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <esp_check.h>

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#include <button.h>
#include <encoder.h>

#include <led_strip.h>

#include "event.h"

ESP_EVENT_DEFINE_BASE(INPUT_EVENTS);

static const char* TAG = "input";

static button_t btn_tl = {.pin = CONFIG_SWEF_PIN_BTN_TL, .group = 0, .active_low = true, .ctx = (void*)BUTTON_TOP_LEFT};
static button_t btn_tr = {.pin = CONFIG_SWEF_PIN_BTN_TR, .group = 0, .active_low = true, .ctx = (void*)BUTTON_TOP_RIGHT};
static button_t btn_br = {.pin = CONFIG_SWEF_PIN_BTN_BR, .group = 0, .active_low = true, .ctx = (void*)BUTTON_BOTTOM_RIGHT};
static button_t btn_bl = {.pin = CONFIG_SWEF_PIN_BTN_BL, .group = 0, .active_low = true, .ctx = (void*)BUTTON_BOTTOM_LEFT};

static button_t btn_enc = {.pin = CONFIG_SWEF_PIN_ENC_BTN, .group = 1, .active_low = true, .ctx = (void*)BUTTON_ENC};

static rotary_encoder_t enc = {
    .btn = &btn_enc,
    .pin_a = CONFIG_SWEF_PIN_ENC_A,
    .pin_b = CONFIG_SWEF_PIN_ENC_B,
    .active_low = true,
};

static led_strip_t led_strip = {
    .gpio = CONFIG_SWEF_PIN_LED_STRIP,
    .length = CONFIG_SWEF_LED_STRIP_LENGTH,
};

static QueueHandle_t enc_events = NULL;
static QueueHandle_t btn_events = NULL;

static adc_cali_handle_t adc_cali_handle = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;
static bool adc_calibrated = false;

static const adc_channel_t ADC_CHANNEL_MAP[CHANNEL_COUNT] = {
    CONFIG_SWEF_POT_CH1_ADC1_CH,
    CONFIG_SWEF_POT_CH2_ADC1_CH,
    CONFIG_SWEF_POT_CH3_ADC1_CH,
    CONFIG_SWEF_POT_CH4_ADC1_CH,
};

uint32_t channel_levels[CHANNEL_COUNT] = {0};

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t* out_handle) {
   adc_cali_handle_t handle = NULL;
   esp_err_t ret = ESP_FAIL;
   bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
   if (!calibrated) {
      ESP_LOGI(TAG, "ADC calibration scheme version is %s", "Curve Fitting");
      adc_cali_curve_fitting_config_t cali_config = {
          .unit_id = unit,
          .atten = atten,
          .bitwidth = ADC_BITWIDTH_DEFAULT,
      };
      ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
      if (ret == ESP_OK)
         calibrated = true;
   }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
   if (!calibrated) {
      ESP_LOGI(TAG, "ADC calibration scheme version is %s", "Line Fitting");
      adc_cali_line_fitting_config_t cali_config = {
          .unit_id = unit,
          .atten = atten,
          .bitwidth = ADC_BITWIDTH_DEFAULT,
      };
      ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
      if (ret == ESP_OK)
         calibrated = true;
   }
#endif

   *out_handle = handle;
   if (ret == ESP_OK) {
      ESP_LOGI(TAG, "ADC calibration success");
   } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
      ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
   } else {
      ESP_LOGE(TAG, "Invalid arg or no memory");
   }

   return calibrated;
}

static inline esp_err_t read_adc(adc_channel_t channel, uint32_t* millivolts) {
   const int SAMPLES = 32;

   uint32_t total = 0;
   int counts;
   for (int i = 0; i < SAMPLES; i++) {
      ESP_RETURN_ON_ERROR(adc_oneshot_read(adc_handle, channel, &counts), TAG, "Failed oneshot ADC read");
      total += counts;
   }

   counts = total / SAMPLES;

   int mv;
   ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(adc_cali_handle, counts, &mv), TAG, "Failed ADC cali conversion");

   *millivolts = mv;
   return ESP_OK;
}

static void pot_task(void* arg) {
   ESP_LOGI(TAG, "Monitoring pots...");

   while (true) {
      bool changed = false;

      for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
         const adc_channel_t channel = ADC_CHANNEL_MAP[i];

         uint32_t value = 0;
         if (read_adc(channel, &value) != ESP_OK) {
            ESP_LOGW(TAG, "ADC read failed: ch=%u (%u)", channel, i);

         } else if (channel_levels[i] != value) {
            changed = true;

            channel_levels[i] = value;
            ESP_LOGI(TAG, "Channel %u Pot = %lu", (i + 1), value);
         }
      }

      if (changed)
         esp_event_post(INPUT_EVENTS, EVENT_INPUT_CHANNEL_LEVELS_CHANGED, channel_levels, sizeof(channel_levels), pdMS_TO_TICKS(100));

      vTaskDelay(pdMS_TO_TICKS(100)); // sample level knobs every 100ms (~10 Hz)
   }
}

static void encoder_task(void* arg) {
   ESP_LOGI(TAG, "Monitoring encoders...");

   int pos = 0;

   rotary_encoder_event_t e;
   while (true) {
      xQueueReceive(enc_events, &e, portMAX_DELAY); // block until encoder event

      pos += e.dir;

      int8_t dir = e.dir;
      esp_event_post(INPUT_EVENTS, EVENT_INPUT_ENC_ROTATED, &dir, sizeof(dir), pdMS_TO_TICKS(100));

      ESP_LOGD(TAG, "Encoder rotated: %d (%d)\n", e.dir, pos);
   }
}

static void button_task(void* arg) {
   ESP_LOGI(TAG, "Monitoring buttons...");

   event_input_args_t args = {0};

   static const char* NAMES[] = {
       [BUTTON_TOP_LEFT] = "Top left",         //
       [BUTTON_TOP_RIGHT] = "Top right",       //
       [BUTTON_BOTTOM_LEFT] = "Bottom left",   //
       [BUTTON_BOTTOM_RIGHT] = "Bottom right", //
       [BUTTON_ENC] = "Encoder",
   };
   static const char* TYPES[] = {
       [BUTTON_CLICKED] = "clicked",         //
       [BUTTON_PRESSED] = "pressed",         //
       [BUTTON_PRESSED_LONG] = "long press", //
       [BUTTON_RELEASED] = "released",
   };

   button_event_t e;
   while (true) {
      xQueueReceive(btn_events, &e, portMAX_DELAY); // block until button event

      args.btn = (input_button_t)e.sender->ctx;
      args.count = e.count;

      ESP_LOGD(TAG, "%s button was %s - %u times", NAMES[args.btn], TYPES[e.type], args.count);

      switch (e.type) { // forward events into the esp event loop
         case BUTTON_CLICKED: {
            esp_event_post(INPUT_EVENTS, EVENT_INPUT_BUTTON_CLICKED, &args, sizeof(args), pdMS_TO_TICKS(100));
            break;
         }
         case BUTTON_PRESSED_LONG: {
            esp_event_post(INPUT_EVENTS, EVENT_INPUT_BUTTON_PRESSED_LONG, &args, sizeof(args), pdMS_TO_TICKS(100));
            break;
         }

         case BUTTON_RELEASED:
         case BUTTON_PRESSED:
         default:
            break;
      }
   }
}

static void led_task(void* arg) {
   const rgb_t green = {255, 0, 0};
   const rgb_t red = {0, 255, 0};

   int index = 0;

   while (true) {

      ESP_ERROR_CHECK(led_strip_fill(&led_strip, 0, led_strip.length - 1, red)); // Set all LEDs to red (indices are clamped to strip length)

      ESP_ERROR_CHECK(led_strip_set(&led_strip, index, green)); // set LED at index to green (index is clamped to strip length)

      ESP_ERROR_CHECK(led_strip_flush(&led_strip));

      index = (index + 1) % led_strip.length; // increment index with wrap around

      vTaskDelay(1000 / portTICK_PERIOD_MS);
   }
}

esp_err_t input_init() {
   ESP_LOGD(TAG, "Init LED strip...");
   ESP_RETURN_ON_ERROR(led_strip_init(&led_strip), TAG, "Failed led strip init");
   led_strip_flush(&led_strip); // Flush so LEDs are off by default

   ESP_LOGD(TAG, "Init rotary encoder...");
   enc_events = xQueueCreate(5, sizeof(rotary_encoder_event_t));
   ESP_RETURN_ON_FALSE(enc_events, ESP_ERR_NO_MEM, TAG, "Failed to create encoder queue");

   ESP_RETURN_ON_ERROR(rotary_encoder_init(enc_events), TAG, "Encoder init failed");
   ESP_RETURN_ON_ERROR(rotary_encoder_add(&enc), TAG, "Adding encoder failed");

   ESP_LOGD(TAG, "Init buttons...");
   btn_events = xQueueCreate(5, sizeof(button_event_t));
   ESP_RETURN_ON_FALSE(btn_events, ESP_ERR_NO_MEM, TAG, "Failed to create button queue");

   ESP_RETURN_ON_ERROR(button_init(btn_events), TAG, "Button init failed");
   ESP_RETURN_ON_ERROR(button_add(&btn_enc), TAG, "Adding button failed");
   ESP_RETURN_ON_ERROR(button_add(&btn_tl), TAG, "Adding button failed");
   ESP_RETURN_ON_ERROR(button_add(&btn_bl), TAG, "Adding button failed");
   ESP_RETURN_ON_ERROR(button_add(&btn_tr), TAG, "Adding button failed");
   ESP_RETURN_ON_ERROR(button_add(&btn_br), TAG, "Adding button failed");

   ESP_LOGD(TAG, "Init ADC...");
   adc_oneshot_unit_init_cfg_t unit_config = {.unit_id = ADC_UNIT_1};
   ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_config, &adc_handle), TAG, "Failed creating ADC handle");

   adc_oneshot_chan_cfg_t chan_config = {
       .bitwidth = ADC_BITWIDTH_DEFAULT,
       .atten = ADC_ATTEN_DB_12,
   };

   // Configure each ADC channel used by the output level pots
   for (uint8_t i = 0; i < CHANNEL_COUNT; i++) {
      ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_MAP[i], &chan_config), TAG, "Failed ADC channel config: ch=%u (%u)", ADC_CHANNEL_MAP[i], i);
   }

   adc_calibrated = adc_calibration_init(unit_config.unit_id, chan_config.atten, &adc_cali_handle);
   if (!adc_calibrated) {
      ESP_LOGW(TAG, "ADC calibration failed!"); // ESP module might be an early revision
   }

   // Start tasks for monitoring buttons and pots
   xTaskCreate(pot_task, "pot_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
   xTaskCreate(button_task, "button_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
   xTaskCreate(encoder_task, "encoder_task", configMINIMAL_STACK_SIZE * 3, NULL, 6, NULL);

   xTaskCreate(led_task, "led_task", configMINIMAL_STACK_SIZE * 3, NULL, 4, NULL);
   return ESP_OK;
}