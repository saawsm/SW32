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
#include "analog_capture.h"

#define NOISE_THRESHOLD (8)

static int32_t last_sample_values[CHANNEL_COUNT][2] = {0};
static uint32_t last_process_times_us[CHANNEL_COUNT] = {0};

float audio_process(analog_channel_t audio_src, bool gen_zcs, uint8_t ch_index, uint16_t pulse_width_us, uint32_t min_period_us, uint32_t* last_pulse_time_us) {
   size_t sample_count;
   uint16_t* sample_buffer;
   uint32_t capture_end_time_us = 0;

   // Fetch audio from the specific analog channel.
   buf_stats_t stats;
   fetch_analog_buffer(audio_src, &sample_count, &sample_buffer, &capture_end_time_us, &stats, true);

   // Skip processing if audio samples are older than previously processed.
   if (capture_end_time_us <= last_process_times_us[ch_index])
      return stats.amplitude; // Return the last computed amplitude, since we are still in the same sample buffer.
   last_process_times_us[ch_index] = capture_end_time_us;

   // Noise filter, ignore very weak signals.
   if (stats.amplitude < 0.05f)
      return 0.0f;

   if (gen_zcs) {
      // Signals that have a period less than the capture duration, should have samples in above/below zero be approx equal.
      // Slow signals that dont complete a full cycle within the capture time will result in an unbalanced above/below zero count.
      bool low_frequency = abs((int32_t)stats.above - (int32_t)stats.below) > 50;

      const uint32_t capture_start_time_us = capture_end_time_us - adc_capture_duration_us; // time when capture started

      // Process each sample at roughly the time it happened
      for (size_t i = 0; i < sample_count; i++) {
         const int32_t value = ADC_ZERO_POINT - sample_buffer[i];

         // Check for rising edge zero crossing
         // Low frequencies, require at least two samples to be rising, to minimize false positives due to noise.
         if ((value > 0 && last_sample_values[ch_index][0] <= 0) && (!low_frequency || (last_sample_values[ch_index][0] >= last_sample_values[ch_index][1]))) {

            const uint32_t sample_time_us = capture_start_time_us + (adc_single_capture_duration_us * i);

            if (sample_time_us - (*last_pulse_time_us) >= min_period_us) { // limit pulse period
               *last_pulse_time_us = sample_time_us;

               output_pulse(ch_index, pulse_width_us, pulse_width_us, sample_time_us + adc_capture_duration_us);
            }
         }

         last_sample_values[ch_index][1] = last_sample_values[ch_index][0];
         last_sample_values[ch_index][0] = value;
      }
   }

   return stats.amplitude;
}