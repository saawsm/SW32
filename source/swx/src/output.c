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
#include "output.h"

#include <stdlib.h>

#include <pico/util/queue.h>
#include <hardware/adc.h>

#include "util/i2c.h"

#include "hardware/mcp4728.h"
#define DAC_MAX_VALUE MCP4728_MAX_VALUE

#include "pulse_gen.pio.h"
#define CHANNEL_PIO_PROGRAM (pio_pulse_gen_program)
#define CHANNEL_PIO (pio0)

#define CH(pinGateA, pinGateB, dacChannel)                                                                                                                               \
   {                                                                                                                                                                     \
       .pin_gate_a = (pinGateA),                                                                                                                                         \
       .pin_gate_b = (pinGateB),                                                                                                                                         \
       .dac_channel = (dacChannel),                                                                                                                                      \
       .status = CHANNEL_INVALID,                                                                                                                                        \
       .max_power = 0.0f,                                                                                                                                                \
   }

static bool calibrate();
static float read_voltage();
static bool write_dac(const channel_t* ch, uint16_t value);
static bool set_drive_enabled(bool enabled);

typedef struct {
   uint8_t channel;
   float power;
} pwr_cmd_t;

typedef struct {
   uint32_t abs_time_us;
   uint16_t pos_us;
   uint16_t neg_us;
} pulse_t;

channel_t channels[CHANNEL_COUNT] = {
    CH(PIN_CH1_GA, PIN_CH1_GB, CH1_DAC_CH),
#if CHANNEL_COUNT > 1
    CH(PIN_CH2_GA, PIN_CH2_GB, CH2_DAC_CH),
#if CHANNEL_COUNT > 2
    CH(PIN_CH3_GA, PIN_CH3_GB, CH3_DAC_CH),
#if CHANNEL_COUNT > 3
    CH(PIN_CH4_GA, PIN_CH4_GB, CH4_DAC_CH),
#endif
#endif
#endif
};

uint8_t require_zero_mask = 0xff;

static bool drv_enabled;
static uint pio_offset;

static queue_t pulse_queues[CHANNEL_COUNT];
static queue_t power_queue;

static uint32_t last_pulse_time_us = 0;

void output_init() {
   LOG_DEBUG("Init output...");

   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++)
      queue_init(&pulse_queues[ch_index], sizeof(pulse_t), 64);

   queue_init(&power_queue, sizeof(pwr_cmd_t), 16);

   // Init drive enable pin
   init_gpio(PIN_DRV_EN, GPIO_OUT, 0);
   gpio_disable_pulls(PIN_DRV_EN);
   set_drive_enabled(false);

   // Init DAC I2C bus
   i2c_init(I2C_PORT_DAC, I2C_FREQ_DAC);
   gpio_set_function(PIN_I2C_SDA_DAC, GPIO_FUNC_I2C);
   gpio_set_function(PIN_I2C_SCL_DAC, GPIO_FUNC_I2C);
   gpio_disable_pulls(PIN_I2C_SDA_DAC); // using hardware pullups
   gpio_disable_pulls(PIN_I2C_SCL_DAC);

   // Init ADC
   adc_gpio_init(PIN_ADC_SENSE);
   adc_init();

   // Init channels
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      LOG_DEBUG("Init channel: ch=%u", ch_index);

      channel_t* ch = &channels[ch_index];

      // Ensure gate pins are off
      init_gpio(ch->pin_gate_a, GPIO_OUT, 0);
      init_gpio(ch->pin_gate_b, GPIO_OUT, 0);

      // Claim PIO state machine
      pio_sm_claim(CHANNEL_PIO, ch_index);
   }

   LOG_DEBUG("Load PIO pulse gen program");
   if (pio_can_add_program(CHANNEL_PIO, &CHANNEL_PIO_PROGRAM)) {
      pio_offset = pio_add_program(CHANNEL_PIO, &CHANNEL_PIO_PROGRAM);
   } else {
      // Panic if program cant be added. This should not normally occur.
      LOG_FATAL("PIO program cant be added! No program space!");
   }

   // Ensure DAC is reachable at address, if not, panic since this is a fatal error that should not normally happen (DAC is not removable)
   if (!i2c_check(I2C_PORT_DAC, I2C_ADDRESS_DAC)) {
      LOG_FATAL("No response from DAC @ address 0x%02x", I2C_ADDRESS_DAC);
   }

   // If output board is missing, fail initialization
   if (check_output_board_missing()) {
      LOG_ERROR("Output board not installed! Disabling all channels...");
      output_scram();
   } else {
      calibrate();
   }
}

void output_scram() {
   // ensure drive power and nfet gates are off, before entering inf loop
   set_drive_enabled(false);

   for (size_t i = 0; i < CHANNEL_COUNT; i++) {
      channels[i].status = CHANNEL_FAULT;

      pio_sm_set_enabled(CHANNEL_PIO, i, false);

      // since pins are used by PIO mux them back to SIO
      init_gpio(channels[i].pin_gate_a, GPIO_OUT, 0);
      init_gpio(channels[i].pin_gate_b, GPIO_OUT, 0);
   }

   for (size_t i = 0; i < CHANNEL_COUNT; i++) {
      if (!write_dac(&channels[i], DAC_MAX_VALUE)) // turn off channels
         break;
   }
}

static bool calibrate() {
   LOG_INFO("Starting channel self-test calibration...");

   // Enable PSU directly since set_drive_enabled() is disabled before calibration is complete
   gpio_put(PIN_DRV_EN, 1);
   sleep_ms(100); // Stabilize

   bool success = true;
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      channel_t* ch = &channels[ch_index];

      // Calibration of already calibrated channels (CHANNEL_READY) is not supported
      if (ch->status != CHANNEL_INVALID)
         continue;

      LOG_DEBUG("Calibrating channel: ch=%u", ch_index);

      float voltage = read_voltage();
      if (voltage > 0.015f) { // 15mV
         LOG_ERROR("Precalibration overvoltage! ch=%u voltage=%.3fv", ch_index, voltage);
         break;

      } else {
         LOG_DEBUG("Precalibration voltage: ch=%u voltage=%.3fv", ch_index, voltage);

         for (uint16_t dacValue = 4000; dacValue > 2000; dacValue -= 10) {
            write_dac(ch, dacValue);
            sleep_us(100); // Stabilize

            // Switch on both nfets
            gpio_put(ch->pin_gate_a, 1);
            gpio_put(ch->pin_gate_b, 1);

            sleep_us(50); // Stabilize, then sample feedback voltage

            voltage = read_voltage();

            // Switch off both nfets
            gpio_put(ch->pin_gate_a, 0);
            gpio_put(ch->pin_gate_b, 0);

            LOG_FINE("Calibrating: ch=%u dac=%d voltage=%.3fv", ch_index, dacValue, voltage);

            // Check if the voltage isn't higher than expected
            if (voltage > CH_CAL_THRESHOLD_OVER) {
               LOG_ERROR("Calibration overvoltage! ch=%u dac=%d voltage=%.3fv", ch_index, dacValue, voltage);
               break;

            } else if (voltage > CH_CAL_THRESHOLD_OK) { // self test ok
               LOG_DEBUG("Calibration OK: ch=%u dac=%d voltage=%.3fv", ch_index, dacValue, voltage);
               ch->cal_value = dacValue;
               ch->status = CHANNEL_READY;
               break;
            }

            sleep_ms(5);
         }
      }

      // Switch off power
      write_dac(ch, DAC_MAX_VALUE);

      if (ch->status == CHANNEL_READY) {
         // Init PIO state machine with pulse gen program
         // Must be done here, since PIO uses different GPIO muxing compared to regular GPIO
         pulse_gen_program_init(CHANNEL_PIO, ch_index, pio_offset, ch->pin_gate_a, ch->pin_gate_b);
         pio_sm_set_enabled(CHANNEL_PIO, ch_index, true); // Start state machine
      } else {
         success = false;
         ch->status = CHANNEL_FAULT;
         LOG_ERROR("Calibration failed! ch=%u", ch_index);
         break;
      }
   }

   // Disable PSU since we are done with calibration
   set_drive_enabled(false);

   if (success) {
      LOG_INFO("Calibration successful!");
   } else {
      LOG_ERROR("Calibration failed for one or more channels!");
   }
   return success;
}

static int cmpfunc(const void* a, const void* b) {
   return *(uint16_t*)a - *(uint16_t*)b;
}

static float read_voltage() {
   const uint32_t MAX_SAMPLES = 10;
   const uint32_t TRIM_AMOUNT = 2;
   const float conv_factor = 3.3f / (1 << 12);

   static_assert(MAX_SAMPLES > (TRIM_AMOUNT * 2));

   adc_select_input(PIN_ADC_BASE - PIN_ADC_SENSE);

   uint16_t readings[MAX_SAMPLES];
   for (size_t i = 0; i < MAX_SAMPLES; i++) // ~2us/sample
      readings[i] = adc_read();

   // Ignore n highest and lowest values. Average the rest
   uint32_t total = 0;
   qsort(readings, MAX_SAMPLES, sizeof(uint16_t), cmpfunc);
   for (uint8_t index = TRIM_AMOUNT; index < (MAX_SAMPLES - TRIM_AMOUNT); index++)
      total += readings[index];

   uint16_t counts = total / (MAX_SAMPLES - (TRIM_AMOUNT * 2));
   return conv_factor * counts;
}

static bool write_dac(const channel_t* ch, uint16_t value) {
   uint8_t buffer[3];

   const size_t len = mcp4728_build_write_cmd(buffer, sizeof(buffer), ch->dac_channel, value, MCP4728_VREF_VDD, MCP4728_GAIN_ONE, MCP4728_PD_NORMAL, MCP4728_UDAC_FALSE);
   if (len == 0)
      LOG_FATAL("MCP4728 build cmd failed!"); // should not happen

   const int ret = i2c_write(I2C_PORT_DAC, I2C_ADDRESS_DAC, buffer, len, false, I2C_DEVICE_TIMEOUT);
   if (ret <= 0) {
      LOG_ERROR("DAC write failed! ch=%u ret=%d", ch->dac_channel, ret);
      return false;
   }
   return true;
}

void output_process_pulse() {
   static const uint16_t PW_MAX = (1 << PULSE_GEN_BITS) - 1;

   pulse_t pulse;
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {

      if (!queue_try_peek(&pulse_queues[ch_index], &pulse)) {
         // Disable drive power if queue is empty, and more than 30 seconds since last output pulse.
         if (drv_enabled && (time_us_32() - last_pulse_time_us) > 30000000u)
            set_drive_enabled(false);

         continue;
      }

      if (time_us_32() >= pulse.abs_time_us) {
         queue_try_remove(&pulse_queues[ch_index], &pulse);

         // Ignore pulses if drive power is disabled, wait time above 1 second, not ready, or requires zeroing.
         if ((require_zero_mask & (1 << ch_index)) || channels[ch_index].status != CHANNEL_READY || pulse.abs_time_us > time_us_32() + 1000000u)
            continue;

         if (pio_sm_is_tx_fifo_full(CHANNEL_PIO, ch_index)) {
            LOG_WARN("Pulse queue full! ch=%u", ch_index);
            continue;
         }

         if (pulse.pos_us > PW_MAX)
            pulse.pos_us = PW_MAX;
         if (pulse.neg_us > PW_MAX)
            pulse.neg_us = PW_MAX;

         static_assert(PULSE_GEN_BITS <= 16); // Ensure we can fit the bits.
         uint32_t val = (pulse.pos_us << PULSE_GEN_BITS) | (pulse.neg_us);
         pio_sm_put(CHANNEL_PIO, ch_index, val);

         last_pulse_time_us = time_us_32();

         if (!drv_enabled)
            set_drive_enabled(true);
      }
   }
}

void output_process_power() {
   if (i2c_get_write_available(I2C_PORT_DAC) < 5) // break, if I2C is going to have blocking writes
      return;

   pwr_cmd_t cmd;
   if (queue_try_remove(&power_queue, &cmd)) {
      channel_t* const ch = &channels[cmd.channel];

      if (ch->status != CHANNEL_READY)
         return;

      float pwr = fclamp(cmd.power, 0.0f, 1.0f) * fclamp(ch->max_power, 0.0f, 1.0f);

      if (require_zero_mask & (1 << cmd.channel)) {
         if (ch->max_power <= 0.01f) {
            require_zero_mask &= ~(1 << cmd.channel);
         } else {
            pwr = 0.0f;
         }
      }

      int16_t dacValue = (ch->cal_value + CH_CAL_OFFSET) - (2000 * pwr);

      if (dacValue < 0 || dacValue > DAC_MAX_VALUE) {
         LOG_WARN("Invalid power calculated! ch=%u pwr=%f dac=%d", cmd.channel, pwr, dacValue);
         return;
      }

      write_dac(ch, (uint16_t)dacValue);
   }
}

bool output_pulse(uint8_t ch_index, uint16_t pos_us, uint16_t neg_us, uint32_t abs_time_us) {
   if (ch_index >= CHANNEL_COUNT)
      return false;

   pulse_t pulse = {
       .pos_us = pos_us,
       .neg_us = neg_us,
       .abs_time_us = abs_time_us,
   };

   return queue_try_add(&pulse_queues[ch_index], &pulse);
}

bool output_power(uint8_t ch_index, float power) {
   if (ch_index >= CHANNEL_COUNT)
      return false;

   pwr_cmd_t cmd = {.channel = ch_index, .power = power};
   return queue_try_add(&power_queue, &cmd);
}

bool check_output_board_missing() {
   if (!drv_enabled) {
      const uint32_t state = save_and_disable_interrupts();

      gpio_set_dir(PIN_DRV_EN, GPIO_IN); // Hi-Z drive enable pin
      gpio_disable_pulls(PIN_DRV_EN);

      const bool no_board = gpio_get(PIN_DRV_EN);

      gpio_set_dir(PIN_DRV_EN, GPIO_OUT);
      gpio_put(PIN_DRV_EN, drv_enabled);

      restore_interrupts(state);
      return no_board;
   } else {
      return false;
   }
}

static bool set_drive_enabled(bool enabled) {
   if (enabled != drv_enabled) {
      if (enabled) { // prevent output power enable if any channel is faulted or is uninitialized
         for (uint8_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (channels[ch_index].status != CHANNEL_READY) {
               set_drive_enabled(false);
               return false;
            }
         }
         LOG_INFO("Enabling drive power...");
      } else {
         LOG_INFO("Disabling drive power...");
      }
   }

   drv_enabled = enabled;

   gpio_set_dir(PIN_DRV_EN, GPIO_OUT);
   gpio_put(PIN_DRV_EN, enabled);
   return true;
}
