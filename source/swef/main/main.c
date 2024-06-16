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
#include <inttypes.h>
#include <math.h>
#include <wchar.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <nvs_flash.h>

#include <esp_log.h>
#include <esp_check.h>
#include <esp_event.h>

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

#include <fontx.h>
#include <hagl_hal.h>
#include <hagl.h>
#include <font6x9.h>

#include <button.h>
#include <encoder.h>
#include <led_strip.h>

static const char* TAG = "main";

static void pot_task(void* pvParameter);
static void button_task(void* pvParameter);
static void encoder_task(void* pvParameter);
static void leds_task(void* pvParameter);

static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t* out_handle);

static button_t btn_tl = {.pin = CONFIG_SWEF_FP_PIN_BTN_TL, .group = 0, .active_low = true};
static button_t btn_tr = {.pin = CONFIG_SWEF_FP_PIN_BTN_TR, .group = 0, .active_low = true};
static button_t btn_br = {.pin = CONFIG_SWEF_FP_PIN_BTN_BR, .group = 0, .active_low = true};
static button_t btn_bl = {.pin = CONFIG_SWEF_FP_PIN_BTN_BL, .group = 0, .active_low = true};

static button_t btn_enc = {.pin = CONFIG_SWEF_FP_PIN_ROT_ENC_BTN, .group = 1, .active_low = true};

static rotary_encoder_t enc = {
    .btn = &btn_enc,
    .pin_a = CONFIG_SWEF_FP_PIN_ROT_ENC_A,
    .pin_b = CONFIG_SWEF_FP_PIN_ROT_ENC_B,
    .active_low = true,
};

static led_strip_t led_strip = {
    .gpio = CONFIG_SWEF_FP_PIN_LED_STRIP,
    .length = CONFIG_SWEF_FP_LED_STRIP_LENGTH,
};

static hagl_backend_t* display;

static QueueHandle_t btn_event_queue;
static QueueHandle_t enc_event_queue;

static bool adc_calibrated = false;
static adc_cali_handle_t adc_cali_handle = NULL;
static adc_oneshot_unit_handle_t adc_handle = NULL;

#define ADC_CHANNEL_MAP_COUNT (4)
static const uint8_t ADC_CHANNEL_MAP[] = {
    CONFIG_SWEF_FP_POT_CH1_ADC1_CH, //
    CONFIG_SWEF_FP_POT_CH2_ADC1_CH, //
    CONFIG_SWEF_FP_POT_CH3_ADC1_CH, //
    CONFIG_SWEF_FP_POT_CH4_ADC1_CH,
};

// TEMP
static const char* BUTTON_STATE_NAMES[] = {
    [BUTTON_PRESSED] = "pressed",   //
    [BUTTON_RELEASED] = "released", //
    [BUTTON_CLICKED] = "clicked",   //
    [BUTTON_PRESSED_LONG] = "long press",
};

// TEMP
static const char* BUTTON_NAMES[] = {
    "Top Left",     //
    "Top Right",    //
    "Bottom Right", //
    "Bottom Left",
};

void app_main() {
   ESP_LOGI(TAG, "~~ swef ~~");

   // Init non-volatile key-value pair storage, required for WiFi driver.
   ESP_ERROR_CHECK(nvs_flash_init());

   // Init default system event loop
   ESP_ERROR_CHECK(esp_event_loop_create_default());

   ESP_LOGD(TAG, "Init LED strip...");
   ESP_ERROR_CHECK(led_strip_init(&led_strip));
   ESP_ERROR_CHECK(led_strip_flush(&led_strip)); // flush so LEDs are off by default

   // Init ADC1
   adc_oneshot_unit_init_cfg_t init_config1 = {
       .unit_id = ADC_UNIT_1,
   };
   ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc_handle));

   adc_oneshot_chan_cfg_t config = {
       .bitwidth = ADC_BITWIDTH_DEFAULT,
       .atten = ADC_ATTEN_DB_11,
   };

   for (uint8_t i = 0; i < ADC_CHANNEL_MAP_COUNT; i++) {
      ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_MAP[i], &config));
   }

   adc_calibrated = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN_DB_11, &adc_cali_handle);
   if (!adc_calibrated) {
      ESP_LOGW(TAG, "ADC failed calibration!");
   }

   display = hagl_init();
   hagl_put_text(display, L"Hello World", 10, 10, hagl_color(display, 25, 25, 255), font6x9);
   hagl_flush(display);

   // Start tasks for monitoring bottons and pots
   xTaskCreate(pot_task, "pot_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
   xTaskCreate(button_task, "button_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
   xTaskCreate(encoder_task, "encoder_task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
   xTaskCreate(leds_task, "leds_task", configMINIMAL_STACK_SIZE * 3, NULL, 4, NULL);
}

static inline esp_err_t read_adc(adc_channel_t channel, float* value, const float conv_factor) {
   const int SAMPLES = 32;

   uint total = 0;
   int counts;
   for (int i = 0; i < SAMPLES; i++) {
      ESP_RETURN_ON_ERROR(adc_oneshot_read(adc_handle, channel, &counts), TAG, "Failed to oneshot read ADC");
      total += counts;
   }

   counts = total / SAMPLES;

   int voltage;
   ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(adc_cali_handle, counts, &voltage), TAG, "Failed ADC counts to voltage conversion");

   *value = voltage * conv_factor;
   return ESP_OK;
}

static void pot_task(void* pvParameter) {
   ESP_LOGI(TAG, "Monitoring pots...");

   float previous_values[4] = {0};

   float value = 0;
   while (true) {

      for (uint8_t i = 0; i < ADC_CHANNEL_MAP_COUNT; i++) {
         const uint8_t adc_ch = ADC_CHANNEL_MAP[i];

         esp_err_t ret = read_adc(adc_ch, &value, 1.0f);
         if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT) {
            ESP_ERROR_CHECK(ret);
         } else if (fabs(previous_values[i] - value) > 0.01f) {
            previous_values[i] = value;

            ESP_LOGI(TAG, "Channel %u Pot = %f", (i + 1), value);
         }
      }

      vTaskDelay(pdMS_TO_TICKS(250));
   }
}

static void encoder_task(void* pvParameter) {
   ESP_LOGI(TAG, "Monitoring encoders...");

   enc_event_queue = xQueueCreate(5, sizeof(rotary_encoder_event_t));

   ESP_ERROR_CHECK(rotary_encoder_init(enc_event_queue));
   ESP_ERROR_CHECK(rotary_encoder_add(&enc));

   int pos = 0;

   rotary_encoder_event_t e;
   while (true) {
      xQueueReceive(enc_event_queue, &e, portMAX_DELAY);

      pos += e.dir;

      ESP_LOGI(TAG, "Rotate: %d (%d)\n", e.dir, pos);
   }

   ESP_ERROR_CHECK(rotary_encoder_free());
   vQueueDelete(enc_event_queue);
}

static void button_task(void* pvParameter) {
   ESP_LOGI(TAG, "Monitoring buttons...");

   btn_event_queue = xQueueCreate(5, sizeof(button_event_t));

   ESP_ERROR_CHECK(button_init(btn_event_queue));
   ESP_ERROR_CHECK(button_add(&btn_enc));

   ESP_ERROR_CHECK(button_add(&btn_tl));
   ESP_ERROR_CHECK(button_add(&btn_bl));
   ESP_ERROR_CHECK(button_add(&btn_tr));
   ESP_ERROR_CHECK(button_add(&btn_br));

   button_event_t e;
   while (true) {
      xQueueReceive(btn_event_queue, &e, portMAX_DELAY);

      uint8_t idx = 0;
      if (e.sender == &btn_tl) {
         idx = 0;
      } else if (e.sender == &btn_tr) {
         idx = 1;
      } else if (e.sender == &btn_br) {
         idx = 2;
      } else if (e.sender == &btn_bl) {
         idx = 3;
      }

      if (e.count > 1) {
         ESP_LOGI(TAG, "Button %s was %s, %u times\n", BUTTON_NAMES[idx], BUTTON_STATE_NAMES[e.type], e.count);
      } else {
         ESP_LOGI(TAG, "Button %s was %s, %u times - delta %" PRIu32 " ms\n", BUTTON_NAMES[idx], BUTTON_STATE_NAMES[e.type], e.count, e.delta_ms);
      }
   }

   ESP_ERROR_CHECK(button_free());
   vQueueDelete(btn_event_queue);
}

static void leds_task(void* pvParameter) {
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
