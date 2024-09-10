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
#include "analog_capture.h"

#include <hardware/adc.h>
#include <hardware/irq.h>
#include <hardware/dma.h>
#include <hardware/clocks.h>

#include "util/i2c.h"
#include "hardware/mcp443x.h"

// The number of analog samples per second per channel. Since 4 ADC channels are being sampled the actual sample rate is 4 times larger.
#define ADC_SAMPLES_PER_SECOND (44100)

// Number of ADC channels sampled
#define ADC_SAMPLED_CHANNELS (4)

#define ADC_CAPTURE_COUNT (1024)                                    // Total samples captured per DMA buffer
#define ADC_SAMPLE_COUNT (ADC_CAPTURE_COUNT / ADC_SAMPLED_CHANNELS) // Number of samples per ADC channel

static void init_pingpong_dma(const uint channel1, const uint channel2, uint dreq, const volatile void* read_addr, volatile void* write_addr1, volatile void* write_addr2,
                              uint transfer_count, enum dma_channel_transfer_size size, uint irq_num, irq_handler_t handler);
static void dma_adc_handler();
static inline bool write_pot(mcp443x_channel_t ch, uint8_t value);

static uint dma_adc_ch1;
static uint dma_adc_ch2;

// DMA Ping-Pong Buffers
static uint16_t adc_capture_buf[2][ADC_CAPTURE_COUNT] __attribute__((aligned(ADC_CAPTURE_COUNT * sizeof(uint16_t))));

volatile uint8_t buf_adc_ready;
volatile uint32_t buf_adc_done_time_us;

// ADC consumer buffers, updated on request
static uint16_t adc_buffers[ADC_SAMPLED_CHANNELS][ADC_SAMPLE_COUNT];

static uint32_t adc_end_time_us; // Cached version of: buf_adc_done_time_us

const uint32_t adc_capture_duration_us = ADC_CAPTURE_COUNT * (1000000ul / (ADC_SAMPLES_PER_SECOND));
const uint32_t adc_single_capture_duration_us = adc_capture_duration_us / ADC_SAMPLE_COUNT;

// Lookup Table: Analog channel -> ADC round robin stripe offset
static const uint8_t adc_stripe_offsets[] = {
    [ANALOG_CHANNEL_AUDIO_MIC] = (PIN_ADC_AUDIO_MIC - PIN_ADC_BASE),
    [ANALOG_CHANNEL_AUDIO_LEFT] = (PIN_ADC_AUDIO_LEFT - PIN_ADC_BASE),
    [ANALOG_CHANNEL_AUDIO_RIGHT] = (PIN_ADC_AUDIO_RIGHT - PIN_ADC_BASE),
    [ANALOG_CHANNEL_SENSE] = (PIN_ADC_SENSE - PIN_ADC_BASE),
};

// Lookup Table: Analog channel -> capture_buf
static uint16_t* const buffers[] = {
    [ANALOG_CHANNEL_AUDIO_MIC] = adc_buffers[0],
    [ANALOG_CHANNEL_AUDIO_LEFT] = adc_buffers[1],
    [ANALOG_CHANNEL_AUDIO_RIGHT] = adc_buffers[2],
    [ANALOG_CHANNEL_SENSE] = adc_buffers[3],
};

// Cached sample buffer statistics. Computed when a new buffer is ready.
static buf_stats_t buf_stats[TOTAL_ANALOG_CHANNELS] = {0};

void analog_capture_init() {
   LOG_DEBUG("Init analog capture...");

   init_gpio(PIN_PIP_EN, GPIO_OUT, true); // active low output
   gpio_disable_pulls(PIN_PIP_EN);

   adc_gpio_init(PIN_ADC_AUDIO_LEFT);
   adc_gpio_init(PIN_ADC_AUDIO_RIGHT);
   adc_gpio_init(PIN_ADC_AUDIO_MIC);
   adc_gpio_init(PIN_ADC_SENSE);

   // Check if digi-pot is reachable at address, if not, crash.
   if (!i2c_check(I2C_PORT, I2C_ADDRESS_POT)) {
      LOG_FATAL("No response from POT @ address 0x%02x", I2C_ADDRESS_POT);
   }

   for (size_t i = 0; i < MCP443X_MAX_CHANNELS; i++)
      write_pot(i, 0);

   LOG_DEBUG("Init freerunning ADC...");
   adc_init();
   adc_select_input(0);
   adc_set_round_robin((1 << (PIN_ADC_AUDIO_LEFT - PIN_ADC_BASE)) | (1 << (PIN_ADC_AUDIO_RIGHT - PIN_ADC_BASE)) | (1 << (PIN_ADC_AUDIO_MIC - PIN_ADC_BASE)) |
                       (1 << (PIN_ADC_SENSE - PIN_ADC_BASE)));

   adc_fifo_setup(true,  // Write each completed conversion to the sample FIFO
                  true,  // Enable DMA data request (DREQ)
                  1,     // DREQ (and IRQ) asserted when at least 1 sample present
                  false, // Don't collect error bit
                  false  // Don't reduce samples
   );

   uint32_t div = clock_get_hz(clk_adc) / (ADC_SAMPLES_PER_SECOND * ADC_SAMPLED_CHANNELS);
   adc_set_clkdiv(div - 1);

   // Setup ping-pong DMA for ADC FIFO writing to adc_capture_buf, wrapping once filled
   dma_adc_ch1 = dma_claim_unused_channel(true);
   dma_adc_ch2 = dma_claim_unused_channel(true);
   init_pingpong_dma(dma_adc_ch1, dma_adc_ch2, DREQ_ADC, &adc_hw->fifo, adc_capture_buf[0], adc_capture_buf[1], ADC_CAPTURE_COUNT, DMA_SIZE_16, DMA_IRQ_0,
                     dma_adc_handler);

   // Start channel 1
   buf_adc_ready = 0;
   dma_channel_start(dma_adc_ch1);

   adc_run(true); // start free-running sampling
}

static inline bool write_pot(mcp443x_channel_t ch, uint8_t value) {
   uint8_t buffer[2];

   const size_t len = mcp443x_build_write_cmd(buffer, sizeof(buffer), ch, value);
   if (len == 0)
      LOG_FATAL("MCP443X build cmd failed!"); // should not happen

   const int ret = i2c_write(I2C_PORT, I2C_ADDRESS_POT, buffer, len, false, I2C_DEVICE_TIMEOUT);
   if (ret <= 0) {
      LOG_ERROR("Digipot write failed! ch=%u ret=%d", ch, ret);
      return false;
   }
   return true;
}

void gain_preamp_set(uint8_t value) {
   write_pot(MCP443X_CHANNEL_4, value);
}

void gain_set(analog_channel_t channel, uint8_t value) {
   static const int8_t analog_gain_channels[] = {
       [ANALOG_CHANNEL_NONE] = -1,
       [ANALOG_CHANNEL_SENSE] = -1,
       [ANALOG_CHANNEL_AUDIO_RIGHT] = MCP443X_CHANNEL_1,
       [ANALOG_CHANNEL_AUDIO_LEFT] = MCP443X_CHANNEL_2,
       [ANALOG_CHANNEL_AUDIO_MIC] = MCP443X_CHANNEL_3,
   };

   int8_t ch = analog_gain_channels[channel];
   if (ch < 0)
      return;

   write_pot(ch, value);
}

// Find the min, max, above/below zero count, and amplitude for the given sample buffer.
static inline void mmaba(uint16_t* samples, size_t count, buf_stats_t* stats) {
   stats->min = UINT32_MAX;
   stats->max = 0;

   for (size_t x = 0; x < count; x++) {
      if (samples[x] > stats->max)
         stats->max = samples[x];

      if (samples[x] < stats->min)
         stats->min = samples[x];

      if (samples[x] > ADC_ZERO_POINT) {
         stats->above++;
      } else {
         stats->below++;
      }
   }

   // Determine output level from buffer.
   // The capture period determines the minimum frequency for a full cycle.
   // So we can handle lower frequencies (with only a partial cycle captured) use
   // the highest value from the zero point, instead of delta between min and max.
   int32_t above_max = stats->max - ADC_ZERO_POINT;
   int32_t below_min = ADC_ZERO_POINT - stats->min;

   int32_t level = MAX(above_max, below_min);
   if (level < 0)
      level = 0;

   stats->amplitude = (float)level / ADC_ZERO_POINT;
}

bool fetch_analog_buffer(analog_channel_t channel, size_t* samples, uint16_t** buffer, uint32_t* capture_end_time_us, buf_stats_t* stats, bool update_stats) {
   switch (channel) {
      case ANALOG_CHANNEL_SENSE:
         update_stats = false;
         // fall through
      case ANALOG_CHANNEL_AUDIO_LEFT:
      case ANALOG_CHANNEL_AUDIO_RIGHT:
      case ANALOG_CHANNEL_AUDIO_MIC: {
         const uint8_t ready = buf_adc_ready; // Local copy, since volatile

         // Check if this channel has new or unprocessed buffer data available
         const bool available = ready & (1 << channel);
         if (available) {
            const uint16_t* src = adc_capture_buf[ready & 1];   // Source DMA buffer
            const uint8_t offset = adc_stripe_offsets[channel]; // ADC round robin offset

            // Unravel interleaved capture buffer
            for (size_t x = 0; x < ADC_SAMPLE_COUNT; x++)
               buffers[channel][x] = (src[(x * ADC_SAMPLED_CHANNELS) + offset] & 0xFFF);

            adc_end_time_us = buf_adc_done_time_us; // Cache done time

            buf_adc_ready &= ~(1 << channel); // Clear flag

            // Update and cache stats
            if (update_stats)
               mmaba(buffers[channel], ADC_SAMPLE_COUNT, &buf_stats[channel]);
         }

         *capture_end_time_us = adc_end_time_us;
         *buffer = buffers[channel];
         *samples = ADC_SAMPLE_COUNT;
         *stats = buf_stats[channel];
         return available;
      }
      default:
         *capture_end_time_us = 0;
         *buffer = NULL;
         *samples = 0;
         *stats = buf_stats[0];
         return false;
   }
}

static inline uint32_t log2i(uint32_t n) {
   uint32_t level = 0;
   while (n >>= 1)
      level++;
   return level;
}

static void __not_in_flash_func(dma_adc_handler)() {
   if (dma_channel_get_irq0_status(dma_adc_ch1)) {
      adc_select_input(0);

      buf_adc_ready = 0xFE; // lookup index: 0
      buf_adc_done_time_us = time_us_32();

      dma_channel_acknowledge_irq0(dma_adc_ch1);
   } else if (dma_channel_get_irq0_status(dma_adc_ch2)) {
      adc_select_input(0);

      buf_adc_ready = 0xFF; // lookup index: 1
      buf_adc_done_time_us = time_us_32();

      dma_channel_acknowledge_irq0(dma_adc_ch2);
   }
}

static void init_pingpong_dma(const uint channel1, const uint channel2, uint dreq, const volatile void* read_addr, volatile void* write_addr1, volatile void* write_addr2,
                              uint transfer_count, enum dma_channel_transfer_size size, uint irq_num, irq_handler_t handler) {
   // Ring bit must be log2 of total bytes transferred
   const uint32_t ring_bit = log2i(transfer_count * (size == DMA_SIZE_32 ? 4 : (size == DMA_SIZE_16 ? 2 : 1)));

   // Channel 1
   dma_channel_config c1 = dma_channel_get_default_config(channel1);
   channel_config_set_transfer_data_size(&c1, size);

   channel_config_set_read_increment(&c1, false); // read_addr
   channel_config_set_write_increment(&c1, true); // write_addr1

   channel_config_set_ring(&c1, true, ring_bit); // Wrap write addr every n bits
   channel_config_set_dreq(&c1, dreq);

   channel_config_set_chain_to(&c1, channel2); // Start channel 2 once finshed

   if (irq_num == DMA_IRQ_0) { // not: dma_irqn_set_channel_enabled
      dma_channel_set_irq0_enabled(channel1, true);
   } else {
      dma_channel_set_irq1_enabled(channel1, true);
   }

   dma_channel_configure(channel1, &c1, write_addr1, read_addr, transfer_count, false);

   // Channel 2
   dma_channel_config c2 = dma_channel_get_default_config(channel2);
   channel_config_set_transfer_data_size(&c2, size);

   channel_config_set_read_increment(&c2, false); // read_addr
   channel_config_set_write_increment(&c2, true); // write_addr2

   channel_config_set_ring(&c2, true, ring_bit); // Wrap write addr every n bits
   channel_config_set_dreq(&c2, dreq);

   channel_config_set_chain_to(&c2, channel1); // Start channel 1 once finshed

   if (irq_num == DMA_IRQ_0) { // not: dma_irqn_set_channel_enabled
      dma_channel_set_irq0_enabled(channel2, true);
   } else {
      dma_channel_set_irq1_enabled(channel2, true);
   }

   dma_channel_configure(channel2, &c2, write_addr2, read_addr, transfer_count, false);

   // Add IRQ handler
   irq_add_shared_handler(irq_num, handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
   irq_set_enabled(irq_num, true);
}
