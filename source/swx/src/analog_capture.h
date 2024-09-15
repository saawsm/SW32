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
#ifndef _ANALOG_CAPTURE_H
#define _ANALOG_CAPTURE_H

#include "swx.h"
#include "channel.h"

// The number of analog samples per second per channel. Since 4 ADC channels are being sampled the actual sample rate is 4 times larger.
#define ADC_SAMPLES_PER_SECOND (30720)

// Number of ADC channels sampled
#define ADC_SAMPLED_CHANNELS (4)

#define ADC_CAPTURE_COUNT (8192)                                    // Total samples captured per DMA buffer
#define ADC_SAMPLE_COUNT (ADC_CAPTURE_COUNT / ADC_SAMPLED_CHANNELS) // Number of samples per ADC channel

#define ADC_ZERO_POINT (2047) // ~1.65V

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
   uint32_t min;
   uint32_t max;
   uint32_t above;
   uint32_t below;
   float amplitude;
} buf_stats_t;

extern const uint32_t adc_capture_duration_us;
extern const uint32_t adc_single_capture_duration_us;

void analog_capture_init();

bool fetch_analog_buffer(analog_channel_t channel, size_t* samples, uint16_t** buffer, uint32_t* capture_end_time_us, buf_stats_t* stats, bool update_stats);

void gain_preamp_set(uint8_t value);
uint8_t gain_preamp_get();

void gain_set(analog_channel_t channel, uint8_t value);
uint8_t gain_get(analog_channel_t channel);

static inline void mic_pip_enable(bool enabled) {
   gpio_put(PIN_PIP_EN, !enabled); // active low
}

static inline bool mic_pip_enabled() {
   return !gpio_get(PIN_PIP_EN); // active low
}

#ifdef __cplusplus
}
#endif

#endif // _ANALOG_CAPTURE_H