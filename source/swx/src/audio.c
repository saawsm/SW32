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

#include <stdlib.h>

#include "output.h"
#include "input.h"

static int32_t last_sample_values[CHANNEL_COUNT] = {0};
static uint32_t last_process_times_us[CHANNEL_COUNT] = {0};

float audio_process(channel_t* ch, uint8_t ch_index) {
   uint16_t sample_count;
   uint16_t* sample_buffer;
   uint32_t capture_end_time_us = 0;

   // Fetch audio from the specific analog channel
   fetch_analog_buffer(ch->audio_src, &sample_count, &sample_buffer, &capture_end_time_us);

   // Skip processing if audio samples are older than previously processed
   if (capture_end_time_us <= last_process_times_us[ch_index])
      return 1.0f;
   last_process_times_us[ch_index] = capture_end_time_us;

   // Find the min, max, and mean sample values
   uint32_t min = UINT32_MAX;
   uint32_t max = 0;
   uint32_t total = 0;
   for (uint16_t x = 0; x < sample_count; x++) {
      if (sample_buffer[x] > max)
         max = sample_buffer[x];

      if (sample_buffer[x] < min)
         min = sample_buffer[x];

      total += sample_buffer[x];
   }

   const uint16_t avg = total / sample_count;

   // Noise filter
   if (abs((int32_t)min - avg) < 7 && abs((int32_t)max - avg) < 7)
      return 0.0f;

   const uint32_t capture_duration_us = get_capture_duration_us(ch->audio_src);      // buffer capture duration
   const uint32_t capture_start_time_us = capture_end_time_us - capture_duration_us; // time when capture started

   const uint32_t sample_duration_us = capture_duration_us / sample_count; // single sample duration

   // Process each sample at roughly the time it happened
   for (uint16_t i = 0; i < sample_count; i++) {
      const uint32_t sample_time_us = capture_start_time_us + (sample_duration_us * i);
      const int32_t value = avg - sample_buffer[i];

      // Check for zero crossing
      if (((value > 0 && last_sample_values[ch_index] <= 0) || (value < 0 && last_sample_values[ch_index] >= 0))) {

         uint32_t min_period = ch->period_us; // dHz -> us
         if (min_period < HZ_TO_US(500))      // clamp to 500 Hz
            min_period = HZ_TO_US(500);

         if (sample_time_us - ch->last_pulse_time_us >= min_period) { // limit pulses to channel period
            ch->last_pulse_time_us = sample_time_us;

            const uint16_t pw = ch->pulse_width_us;
            output_pulse(ch_index, pw, pw, sample_time_us + 20000); // 20 ms in future
         }
      }

      last_sample_values[ch_index] = value;
   }

   return (max - min) / 255.0f; // crude approximation of volume
}