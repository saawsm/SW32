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
      .pin_gate_a = (pinGateA), .pin_gate_b = (pinGateB), .dac_channel = (dacChannel), .status = CHANNEL_INVALID, .power = 1.0f, .power_level = 0.5f,                    \
      .pulse_width_us = 150, .period_us = HZ_TO_US(180.0f), .audio_src = ANALOG_CHANNEL_NONE, .enabled = false                                                           \
   }

extern float audio_process(channel_t* ch, uint8_t ch_index);

static bool calibrate();
static bool write_dac(const channel_t* ch, uint16_t value);

typedef struct {
   uint8_t channel;
   uint16_t power;
} pwr_cmd_t;

typedef struct {
   uint32_t abs_time_us;

   uint8_t channel;

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

static bool drv_enabled;
static uint pio_offset;

static queue_t pulse_queue;
static queue_t power_queue;

static pulse_t pulse;
static bool fetch_pulse = false;

void output_init() {
   LOG_DEBUG("Init output...");

   queue_init(&pulse_queue, sizeof(pulse_t), 16);
   queue_init(&power_queue, sizeof(pwr_cmd_t), 16);

   // Init drive enable pin
   init_gpio(PIN_DRV_EN, GPIO_OUT, 0);
   gpio_disable_pulls(PIN_DRV_EN);
   output_drv_enable(false);

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
   if (!i2c_check(I2C_PORT_DAC, CH_DAC_ADDRESS)) {
      LOG_FATAL("No response from DAC @ address 0x%02x", CH_DAC_ADDRESS);
   }

   // If output board is missing, fail initialization
   if (check_output_board_missing()) {
      LOG_ERROR("Output board not installed! Disabling all channels...");

      for (size_t i = 0; i < CHANNEL_COUNT; i++)
         channels[i].status = CHANNEL_FAULT;

   } else {
      // Kick-start processing queued channel pulses by fetching the first one if calibration was successful
      fetch_pulse = calibrate();
   }
}

void output_scram() {
   // ensure drive power and nfet gates are off, before entering inf loop
   output_drv_enable(false);

   for (size_t i = 0; i < CHANNEL_COUNT; i++) {
      channels[i].status = CHANNEL_FAULT;

      // since pins are used by PIO mux them back to SIO
      init_gpio(channels[i].pin_gate_a, GPIO_OUT, 0);
      init_gpio(channels[i].pin_gate_b, GPIO_OUT, 0);
   }

   for (size_t i = 0; i < CHANNEL_COUNT; i++) {
      if (!write_dac(&channels[i], DAC_MAX_VALUE)) // turn off channels
         break;
   }

   pio_remove_program(CHANNEL_PIO, &CHANNEL_PIO_PROGRAM, pio_offset);
}

void pulse_gen_process() {
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      channel_t* const ch = &channels[ch_index];

      if (!ch->enabled)
         continue;

      float audio_power = 1.0f;

      // Channel has audio source, so process audio instead of running pulse gen
      if (ch->audio_src != ANALOG_CHANNEL_NONE) {
         audio_power = audio_process(ch, ch_index);

      } else if (time_us_32() - ch->last_pulse_time_us >= ch->period_us) { // otherwise pulse output every period_us
         ch->last_pulse_time_us = time_us_32();

         const uint16_t pw = ch->pulse_width_us;
         output_pulse(ch_index, pw, pw, time_us_32());
      }

      // Set channel output power, limit update rate to ~2.2 kHz since it takes the DAC about ~110us/ch
      if (time_us_32() - ch->last_power_time_us >= CHANNEL_COUNT * 110) {
         ch->last_power_time_us = time_us_32();

         const uint16_t real_power = (uint16_t)(CHANNEL_POWER_MAX * fclamp(ch->power_level * ch->power * audio_power, 0.0f, 1.0f));
         output_set_power(ch_index, real_power);
      }
   }
}

void output_process_pulse() {
   static const uint16_t PW_MAX = (1 << PULSE_GEN_BITS) - 1;

   for (size_t i = 0; i < CHANNEL_COUNT * 8; i++) { // FIFO is 8 deep
      if (fetch_pulse) {
         if (queue_try_remove(&pulse_queue, &pulse)) {
            if (pulse.abs_time_us < time_us_32() + 1000000ul) // ignore pulses with wait times above 1 second
               fetch_pulse = false;
         }
      } else if (time_us_32() >= pulse.abs_time_us) {
         const channel_t* ch = &channels[pulse.channel];

         if (ch->status == CHANNEL_READY) {
            if (!pio_sm_is_tx_fifo_full(CHANNEL_PIO, pulse.channel)) {
               static_assert(PULSE_GEN_BITS <= 16); // Ensure we can fit the bits

               if (pulse.pos_us > PW_MAX)
                  pulse.pos_us = PW_MAX;
               if (pulse.neg_us > PW_MAX)
                  pulse.neg_us = PW_MAX;

               uint32_t val = (pulse.pos_us << PULSE_GEN_BITS) | (pulse.neg_us);
               pio_sm_put(CHANNEL_PIO, pulse.channel, val);
            } else {
               LOG_WARN("PIO pulse queue full! ch=%u", pulse.channel);
            }
         }

         fetch_pulse = true;
      }
   }
}

void output_process_power() {
   for (size_t i = 0; i < CHANNEL_COUNT; i++) {
      if (i2c_get_write_available(I2C_PORT_DAC) < 5) // break, if I2C is going to have blocking writes
         break;

      pwr_cmd_t cmd;
      if (queue_try_remove(&power_queue, &cmd)) {
         const channel_t* ch = &channels[cmd.channel];

         if (ch->status != CHANNEL_READY)
            continue;

         int16_t dacValue = (ch->cal_value + CH_CAL_OFFSET) - cmd.power;

         if (dacValue < 0 || dacValue > DAC_MAX_VALUE) {
            LOG_WARN("Invalid power calculated! ch=%u pwr=%u dac=%d - ERROR!", cmd.channel, cmd.power, dacValue);
            continue;
         }

         write_dac(ch, (uint16_t)dacValue);
      }
   }
}

bool output_pulse(uint8_t ch_index, uint16_t pos_us, uint16_t neg_us, uint32_t abs_time_us) {
   if (ch_index >= CHANNEL_COUNT)
      return false;

   pulse_t pulse = {
       .channel = ch_index,
       .pos_us = pos_us,
       .neg_us = neg_us,
       .abs_time_us = abs_time_us,
   };

   return queue_try_add(&pulse_queue, &pulse);
}

void output_set_power(uint8_t ch_index, uint16_t power) {
   if (ch_index >= CHANNEL_COUNT)
      return;

   pwr_cmd_t cmd = {.channel = ch_index, .power = power};
   queue_try_add(&power_queue, &cmd);
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

bool output_drv_enable(bool enabled) {
   if (enabled != drv_enabled) {
      if (enabled) { // prevent output power enable if any channel is faulted or is uninitialized
         for (uint8_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (channels[ch_index].status != CHANNEL_READY) {
               output_drv_enable(false);
               return false;
            }
         }
         LOG_INFO("Enabling power...");
      } else {
         LOG_INFO("Disabling power...");
      }
   }

   drv_enabled = enabled;

   gpio_set_dir(PIN_DRV_EN, GPIO_OUT);
   gpio_put(PIN_DRV_EN, enabled);
   return true;
}

bool output_drv_enabled() {
   return drv_enabled;
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
   for (size_t i = 0; i < MAX_SAMPLES; i++)
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

   const int ret = i2c_write(I2C_PORT_DAC, CH_DAC_ADDRESS, buffer, sizeof(buffer), false, I2C_DEVICE_TIMEOUT);
   if (ret <= 0) {
      LOG_ERROR("MCP4728: ch=%u ret=%d - I2C write failed!", ch->dac_channel, ret);
      return false;
   }
   return true;
}

static bool calibrate() {
   LOG_INFO("Starting channel self-test calibration...");

   // Enable PSU directly since output_drv_enable() is disabled before calibration is complete
   gpio_put(PIN_DRV_EN, 1);
   sleep_ms(100); // stabilize

   bool success = true;
   for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
      channel_t* ch = &channels[ch_index];

      // Calibration of already calibrated channels (CHANNEL_READY) is not supported
      if (ch->status != CHANNEL_INVALID)
         continue;

      LOG_DEBUG("Calibrating channel: ch=%u", ch_index);

      float voltage = read_voltage();
      if (voltage > 0.015f) { // 15mV
         LOG_ERROR("Precalibration overvoltage! ch=%u voltage=%.3fv - ERROR!", ch_index, voltage);
         break;

      } else {
         LOG_DEBUG("Precalibration voltage: ch=%u voltage=%.3fv - OK", ch_index, voltage);

         bool gate_flip = false;
         for (uint16_t dacValue = 4000; dacValue > 2000; dacValue -= 10) {
            write_dac(ch, dacValue);
            sleep_us(100); // Stabilize

            // Switch on one nfet leg and alternate each iteration
            gpio_put(ch->pin_gate_a, gate_flip);
            gpio_put(ch->pin_gate_b, !gate_flip);

            sleep_us(50); // Stabilize, then sample feedback voltage

            voltage = read_voltage();

            // Switch off both nfets
            gpio_put(ch->pin_gate_a, 0);
            gpio_put(ch->pin_gate_b, 0);

            LOG_FINE("Calibrating: ch=%u dac=%d voltage=%.3fv", ch_index, dacValue, voltage);

            // Check if the voltage isn't higher than expected
            if (voltage > CH_CAL_THRESHOLD_OVER) {
               LOG_ERROR("Calibration overvoltage! ch=%u dac=%d voltage=%.3fv - ERROR!", ch_index, dacValue, voltage);
               break;

            } else if (voltage > CH_CAL_THRESHOLD_OK) { // self test ok
               LOG_DEBUG("Calibration: ch=%u dac=%d voltage=%.3fv - OK", ch_index, dacValue, voltage);
               ch->cal_value = dacValue;
               ch->status = CHANNEL_READY;
               break;
            }

            sleep_ms(5);
            gate_flip = !gate_flip;
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
         LOG_ERROR("Calibration failed! ch=%u - ERROR!", ch_index);
         break;
      }
   }

   // Disable PSU since we are done with calibration
   output_drv_enable(false);

   if (success) {
      LOG_INFO("Calibration successful!");
   } else {
      LOG_ERROR("Calibration failed for one or more channels!");
   }
   return success;
}
