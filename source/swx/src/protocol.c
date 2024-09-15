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
#include "protocol.h"

#include <cobs.h>

#include <pico/bootrom.h>
#include <hardware/uart.h>

#include "message.h"
#include "filesystem.h"
#include "pulse_gen.h"
#include "output.h"
#include "trigger.h"
#include "analog_capture.h"

static const char* const cobs_encode_status_text[] = {
    [COBS_ENCODE_OK] = "ok",
    [COBS_ENCODE_NULL_POINTER] = "null pointer",
    [COBS_ENCODE_OUT_BUFFER_OVERFLOW] = "out buffer overflow",
};

static const char* const cobs_decode_status_text[] = {
    [COBS_DECODE_OK] = "ok",
    [COBS_DECODE_NULL_POINTER] = "null pointer",
    [COBS_DECODE_OUT_BUFFER_OVERFLOW] = "out buffer overflow",
    [COBS_DECODE_ZERO_BYTE_IN_INPUT] = "zero byte in input",
    [COBS_DECODE_INPUT_TOO_SHORT] = "input too short",
};

typedef struct {
   size_t index;
   uint8_t buffer[MSG_FRAME_SIZE];
} ringbuffer_t;

static ringbuffer_t comm_uart = {0};
static ringbuffer_t comm_stdio = {0};

static uint8_t frame_dec[MSG_SIZE] = {0};       // decoded frame
static uint8_t frame_enc[MSG_FRAME_SIZE] = {0}; // encoded frame

void protocol_init() {
   LOG_DEBUG("Init protocol...");

   uart_init(UART_PORT, UART_BAUD);
   uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);

   gpio_set_function(PIN_TXD1, GPIO_FUNC_UART);
   gpio_set_function(PIN_RXD1, GPIO_FUNC_UART);
}

static inline size_t command_min_arg_length(uint8_t cmd) {
   switch (cmd) {
      case MSG_ID_REQUEST_VERSION:
         return 0;
      case MSG_ID_UPDATE_MAX_POWER:
         return 3;
      case MSG_ID_REQUEST_MAX_POWER:
         return 1;
      case MSG_ID_UPDATE_REQUIRE_ZERO:
         return 1;
      case MSG_ID_REQUEST_REQUIRE_ZERO:
         return 0;
      case MSG_ID_UPDATE_CH_AUDIO:
         return 2;
      case MSG_ID_REQUEST_CH_AUDIO:
         return 1;
      case MSG_ID_UPDATE_CH_EN_MASK:
         return 1;
      case MSG_ID_REQUEST_CH_EN_MASK:
         return 0;
      case MSG_ID_UPDATE_CH_PARAM:
         return 4;
      case MSG_ID_REQUEST_CH_PARAM:
         return 2;
      case MSG_ID_CH_PARAM_UPDATE:
         return 2;
      case MSG_ID_REQUEST_CH_STATUS:
         return 1;
      case MSG_ID_UPDATE_SEQ:
         return 2;
      case MSG_ID_REQUEST_SEQ:
         return 0;
      case MSG_ID_UPDATE_SEQ_COUNT:
         return 1;
      case MSG_ID_REQUEST_SEQ_COUNT:
         return 0;
      case MSG_ID_RESET_SEQ_INDEX:
         return 0;
      case MSG_ID_UPDATE_SEQ_PERIOD:
         return 2;
      case MSG_ID_REQUEST_SEQ_PERIOD:
         return 0;
      case MSG_ID_UPDATE_ACTION:
         return 8;
      case MSG_ID_REQUEST_ACTION:
         return 1;
      case MSG_ID_RUN_ACTION_LIST:
         return 2;
      case MSG_ID_UPDATE_TRIGGER:
         return 10;
      case MSG_ID_REQUEST_TRIGGER:
         return 1;
      case MSG_ID_REQUEST_TRIGGER_STATE:
         return 0;
      case MSG_ID_SHUTDOWN:
         return 0;
      case MSG_ID_RESET_TO_USB_BOOT:
         return 0;
      case MSG_ID_REQUEST_MIC_PIP_EN:
         return 0;
      case MSG_ID_UPDATE_MIC_PIP_EN:
         return 1;
      default:
         return 0;
   }
}

static void process_frame(comm_channel_t ch, ringbuffer_t* rb) {
   if (rb->index < 2) { // buffer not large enough to be a valid frame
      rb->index = 0;
      return;
   }

   // Decode COBS encoded ring buffer frame
   cobs_decode_result ret = cobs_decode(frame_dec, sizeof(frame_dec), rb->buffer, rb->index);
   rb->index = 0; // wrap ringbuffer

   if (ret.status != COBS_DECODE_OK) {
      LOG_WARN("Frame decode failed! ret=%u (%s)", ret.status, cobs_decode_status_text[ret.status]);
      return;
   } else if (ret.out_len == 0 || frame_dec[0] != MSG_FRAME_START) {
      LOG_WARN("Frame missing starting byte! Expected %u, got %u!", MSG_FRAME_START, frame_dec[0]);
      return;
   }

   const uint8_t cmd = frame_dec[1];
   uint8_t* data = &frame_dec[2];

   size_t rlen = command_min_arg_length(cmd);
   if ((ret.out_len - 2) < rlen) { // -2 for MSG_FRAME_START and cmd byte
      LOG_WARN("Message length invalid! Expected %u, got %u!", rlen, (ret.out_len - 2));
      return;
   }

   switch (cmd) {
      case MSG_ID_REQUEST_VERSION: {
         PROTO_REPLY(ch, MSG_ID_VERSION, SWX_VERSION_PCB_REV, SWX_VERSION_MAJOR, SWX_VERSION_MINOR);
      } break;
      case MSG_ID_REQUEST_ERR: {
         PROTO_REPLY(ch, MSG_ID_ERR, U16_U8(swx_err));
      } break;
      case MSG_ID_UPDATE_MAX_POWER: {
         uint8_t ch_mask = data[0];

         uint16_t value = U8_U16(data, 1);

         float pwr = (float)value / UINT16_MAX;

         for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (ch_mask & (1 << ch_index))
               channels[ch_index].max_power = pwr;
         }
         LOG_FINE("Update max_power: ch_mask=%u value=%f", ch_mask, pwr);
      } break;
      case MSG_ID_REQUEST_MAX_POWER: {
         uint8_t ch_mask = data[0];
         for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (ch_mask & (1 << ch_index)) {
               float max_power = channels[ch_index].max_power;

               LOG_FINE("Fetch max_power: ch=%u value=%f", ch_index, max_power);

               uint16_t value = UINT16_MAX * max_power;

               PROTO_REPLY(ch, MSG_ID_UPDATE_MAX_POWER, (1 << ch_index), U16_U8(value));
            }
         }
      } break;
      case MSG_ID_UPDATE_REQUIRE_ZERO: {
         uint8_t mask = data[0];
         require_zero_mask |= mask;
         LOG_FINE("Update require_zero: value=%u", mask);
      } break;
      case MSG_ID_REQUEST_REQUIRE_ZERO: {
         LOG_FINE("Fetch require_zero: value=%u", require_zero_mask);
         PROTO_REPLY(ch, MSG_ID_UPDATE_REQUIRE_ZERO, require_zero_mask);
      } break;
      case MSG_ID_UPDATE_CH_AUDIO: {
         uint8_t ch_mask = data[0];
         uint8_t val = data[1];

         uint8_t audio_src = val & ~AUDIO_MODE_FLAG;

         if (audio_src < TOTAL_ANALOG_CHANNELS) {
            for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
               if (ch_mask & (1 << ch_index)) {
                  uint8_t* audio = &pulse_gen.channels[ch_index].audio;

                  if (*audio != val && audio_src) // require zero if audio src changed
                     require_zero_mask |= (1 << ch_index);

                  *audio = val;
               }
            }
            LOG_FINE("Update audio_src: ch_mask=%u value=%u", ch_mask, val);
         }
      } break;
      case MSG_ID_REQUEST_CH_AUDIO: {
         uint8_t ch_mask = data[0];
         for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (ch_mask & (1 << ch_index)) {
               uint8_t audio = pulse_gen.channels[ch_index].audio;

               LOG_FINE("Fetch audio: ch=%u value=%u", ch_index, audio);

               PROTO_REPLY(ch, MSG_ID_UPDATE_CH_AUDIO, (1 << ch_index), audio);
            }
         }
      } break;
      case MSG_ID_UPDATE_MIC_GAIN: {
         uint8_t value = data[0];

         gain_preamp_set(value);

         LOG_FINE("Update preamp: value=%u", value);
      } break;
      case MSG_ID_REQUEST_MIC_GAIN: {
         uint8_t value = gain_preamp_get();

         LOG_FINE("Fetch preamp: value=%u", value);

         PROTO_REPLY(ch, MSG_ID_UPDATE_MIC_GAIN, value);
      } break;
      case MSG_ID_UPDATE_GAIN: {
         uint8_t ach = data[0];
         uint8_t value = data[1];

         if (ach < TOTAL_ANALOG_CHANNELS) {
            gain_set(ach, value);

            LOG_FINE("Update gain: ch=%u value=%u", ach, value);
         }
      } break;
      case MSG_ID_REQUEST_GAIN: {
         uint8_t ach = data[0];
         if (ach < TOTAL_ANALOG_CHANNELS) {
            uint8_t value = gain_get(ach);

            LOG_FINE("Fetch gain: ch=%u value=%u", ach, value);

            PROTO_REPLY(ch, MSG_ID_UPDATE_GAIN, ach, value);
         }
      } break;
      case MSG_ID_UPDATE_CH_EN_MASK: {
         uint8_t mask = data[0];
         require_zero_mask |= pulse_gen.en_mask ^ mask; // require zero if enable changed
         pulse_gen.en_mask = mask;
         LOG_FINE("Update en_mask: value=%u", mask);
      } break;
      case MSG_ID_REQUEST_CH_EN_MASK: {
         LOG_FINE("Fetch en_mask: value=%u", pulse_gen.en_mask);
         PROTO_REPLY(ch, MSG_ID_UPDATE_CH_EN_MASK, pulse_gen.en_mask);
      } break;
      case MSG_ID_UPDATE_CH_PARAM: {
         uint8_t ch_mask = data[0];
         uint8_t param = data[1] >> 4;
         uint8_t target = data[1] & 0xf;

         if (param < TOTAL_PARAMS && target < TOTAL_TARGETS) {
            uint16_t value = U8_U16(data, 2);

            for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
               if (ch_mask & (1 << ch_index)) {
                  if (target != TARGET_MODE && parameter_get(ch_index, param, TARGET_MODE) & TARGET_MODE_FLAG_READONLY)
                     continue;
                  parameter_set(ch_index, param, target, value);
               }
            }

            LOG_FINE("Update param: ch_mask=%u param=%u target=%u value=%u", ch_mask, param, target, value);
         }
      } break;
      case MSG_ID_REQUEST_CH_PARAM: {
         uint8_t ch_mask = data[0];
         uint8_t param = data[1] >> 4;
         uint8_t target = data[1] & 0xf;

         if (param < TOTAL_PARAMS && target < TOTAL_TARGETS) {
            for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
               if (ch_mask & (1 << ch_index)) {
                  uint16_t value = parameter_get(ch_index, param, target);

                  LOG_FINE("Fetch param: ch=%u param=%u target=%u value=%u", ch_index, param, target, value);

                  PROTO_REPLY(ch, MSG_ID_UPDATE_CH_PARAM, (1 << ch_index), data[1], U16_U8(value));
               }
            }
         }
      } break;
      case MSG_ID_CH_PARAM_UPDATE: {
         uint8_t ch_mask = data[0];
         uint8_t param = data[1];

         uint8_t pstart = param;
         uint8_t pend = param + 1;

         if (param < TOTAL_PARAMS || param == 0xff) {

            if (param == 0xff) {
               pstart = 0;
               pend = TOTAL_PARAMS;
            }

            for (uint8_t i = pstart; i < pend; i++) {
               for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
                  if (ch_mask & (1 << ch_index))
                     parameter_update(ch_index, i);
               }
               LOG_FINE("Param update: ch_mask=%u param=%u", ch_mask, i);
            }
         }
      } break;
      case MSG_ID_REQUEST_CH_STATUS: {
         uint8_t ch_mask = data[0];

         for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
            if (ch_mask & (1 << ch_index)) {
               channel_status_t status = channels[ch_index].status;

               LOG_FINE("Fetch status: ch=%u value=%u", ch_index, status);

               PROTO_REPLY(ch, MSG_ID_CH_STATUS, (1 << ch_index), status);
            }
         }
      } break;
      case MSG_ID_UPDATE_SEQ: {
         bool wrap = !!data[0];

         // MAX_SEQUENCES might change so use size_t to prevent type-limits warning.
         size_t count = data[1];

         if (count > MAX_SEQUENCES)
            count = MAX_SEQUENCES;

         memcpy(pulse_gen.sequencer.masks, &data[2], count);

         if (wrap)
            pulse_gen.sequencer.count = count;

         LOG_FINE("Update seq: count=%u wrap=%u", count, wrap);
      } break;
      case MSG_ID_REQUEST_SEQ: {
         uint8_t msg[4 + MAX_SEQUENCES] = {MSG_FRAME_START, MSG_ID_UPDATE_SEQ, false, MAX_SEQUENCES};

         memcpy(&msg[4], pulse_gen.sequencer.masks, MAX_SEQUENCES);

         protocol_write_frame(ch, msg, sizeof(msg));
      } break;
      case MSG_ID_UPDATE_SEQ_COUNT: {
         uint8_t count = data[0];
         pulse_gen.sequencer.count = count;
         LOG_FINE("Update seq: count=%u", count);
      } break;
      case MSG_ID_REQUEST_SEQ_COUNT: {
         uint8_t count = pulse_gen.sequencer.count;
         LOG_FINE("Fetch seq: count=%u", count);
         PROTO_REPLY(ch, MSG_ID_UPDATE_SEQ_COUNT, count);
      } break;
      case MSG_ID_RESET_SEQ_INDEX: {
         LOG_FINE("Reset seq: index=%u", pulse_gen.sequencer.index);
         pulse_gen.sequencer.index = 0;
      } break;
      case MSG_ID_UPDATE_SEQ_PERIOD: {
         uint16_t period_ms = U8_U16(data, 0);

         pulse_gen.sequencer.period_us = period_ms * 1000u;

         LOG_FINE("Update seq_period: value=%u", period_ms);
      } break;
      case MSG_ID_REQUEST_SEQ_PERIOD: {
         uint16_t period_ms = pulse_gen.sequencer.period_us / 1000u;

         LOG_FINE("Fetch seq_period: value=%u", period_ms);

         PROTO_REPLY(ch, MSG_ID_UPDATE_SEQ_PERIOD, U16_U8(period_ms));
      } break;
      case MSG_ID_UPDATE_ACTION: {
         uint8_t a_index = data[0];
         bool en = !!data[1];
         uint8_t type = data[2];
         uint8_t ch_mask = data[3];
         uint8_t param = data[4];
         uint8_t target = data[5];
         if (a_index < MAX_ACTIONS && param < TOTAL_PARAMS && target < TOTAL_TARGETS) {
            uint16_t value = U8_U16(data, 6);

            action_t* action = &pulse_gen.actions[a_index];
            action->enabled = en;
            action->type = type;
            action->ch_mask = ch_mask;
            action->param = param;
            action->target = target;
            action->value = value;

            LOG_FINE("Update action: index=%u en=%u type=%u ch_mask=%u param=%u target=%u value=%u", a_index, en, type, ch_mask, param, target, value);
         }
      } break;
      case MSG_ID_REQUEST_ACTION: {
         uint8_t a_index = data[0];
         if (a_index < MAX_ACTIONS) {
            action_t* action = &pulse_gen.actions[a_index];

            LOG_FINE("Fetch action: index=%u en=%u type=%u ch_mask=%u param=%u target=%u value=%u", a_index, action->enabled, action->type, action->ch_mask,
                     action->param, action->target, action->value);

            PROTO_REPLY(ch, MSG_ID_UPDATE_ACTION, a_index, action->enabled, action->type, action->ch_mask, action->param, action->target, U16_U8(action->value));
         }
      } break;
      case MSG_ID_RUN_ACTION_LIST: {
         uint8_t al_start = data[0];
         uint8_t al_end = data[1];

         if (al_start < MAX_ACTIONS && al_end < MAX_ACTIONS) {
            LOG_FINE("Execute action list: %u -> %u", al_start, al_end);

            execute_action_list(al_start, al_end);
         }
      } break;
      case MSG_ID_UPDATE_TRIGGER: {
         uint8_t trig_index = data[0];

         uint8_t input_invert_mask = data[1] >> 4;
         uint8_t input_mask = data[1] & 0xf;

         bool repeating = !!(data[2] & (1 << 7));
         bool result_inv = !!(data[2] & (1 << 6));
         uint8_t op = data[2] & 0b00111111;

         bool enabled = !!(data[3] & (1 << 7));
         bool threshold_invert = !!(data[3] & (1 << 6));
         bool require_both = !!(data[3] & (1 << 5));
         uint8_t input_audio = data[3] & 0b00011111;

         uint16_t threshold = U8_U16(data, 4);

         uint16_t min_period_ms = U8_U16(data, 6);
         uint8_t al_start = data[8];
         uint8_t al_end = data[9];

         if (trig_index < MAX_TRIGGERS && op < TOTAL_TRIGGER_OPS && input_audio < TOTAL_ANALOG_CHANNELS) {
            trigger_t* trigger = &triggers[trig_index];
            trigger->enabled = enabled;
            trigger->input_mask = input_mask;
            trigger->input_audio = input_audio;
            trigger->input_invert_mask = input_invert_mask;
            trigger->output_invert = result_inv;
            trigger->op = op;
            trigger->threshold_invert = threshold_invert;
            trigger->require_both = require_both;
            trigger->threshold = (float)threshold / UINT16_MAX;
            trigger->repeating = repeating;
            trigger->min_period_us = min_period_ms * 1000u;
            trigger->action_start_index = al_start;
            trigger->action_end_index = al_end;

            LOG_FINE("Update trigger: index=%u en=%u iim=%u im=%u repeat=%u inv=%u op=%u tinv=%u both=%u audio=%u threshold=%u min_period_ms=%u al=%u-%u", trig_index,
                     enabled, input_invert_mask, input_mask, repeating, result_inv, op, threshold_invert, require_both, input_audio, threshold, min_period_ms, al_start,
                     al_end);
         }
      } break;
      case MSG_ID_REQUEST_TRIGGER: {
         uint8_t trig_index = data[0];

         if (trig_index < MAX_TRIGGERS) {
            trigger_t* trigger = &triggers[trig_index];

            uint16_t min_period_ms = trigger->min_period_us / 1000u;
            uint16_t threshold = MIN(trigger->threshold, 1.0f) * UINT16_MAX;

            LOG_FINE("Fetch trigger: index=%u iim=%u im=%u repeat=%u inv=%u op=%u min_period_ms=%u al=%u-%u", trig_index, trigger->input_invert_mask, trigger->input_mask,
                     trigger->repeating, trigger->output_invert, trigger->op, min_period_ms, trigger->action_start_index, trigger->action_end_index);

            uint8_t input = (trigger->input_invert_mask << 4) | (trigger->input_mask & 0xf);
            uint8_t operation = (trigger->repeating << 7) | (trigger->output_invert << 6) | (trigger->op & 0b00111111);
            uint8_t audio = (trigger->enabled << 7) | (trigger->threshold_invert << 6) | (trigger->require_both << 5) | (trigger->input_audio & 0b00011111);

            PROTO_REPLY(ch, MSG_ID_UPDATE_TRIGGER, trig_index, input, operation, audio, U16_U8(threshold), U16_U8(min_period_ms), trigger->action_start_index,
                        trigger->action_end_index);
         }
      } break;
      case MSG_ID_REQUEST_TRIGGER_STATE: {
         LOG_FINE("Fetch trigger input: state=%u", trig_input_states);
         PROTO_REPLY(ch, MSG_ID_TRIGGER_STATE, trig_input_states);
      } break;
      case MSG_ID_SHUTDOWN: {
         swx_power_off();
      } break;
      case MSG_ID_RESET_TO_USB_BOOT: {
         LOG_FINE("Resetting to USB boot...");
         output_scram();
         reset_usb_boot(0, 0);
      } break;
      case MSG_ID_REQUEST_MIC_PIP_EN: {
         bool enabled = mic_pip_enabled();

         LOG_FINE("Fetch mic_pip state: en=%u", enabled);

         PROTO_REPLY(ch, MSG_ID_UPDATE_MIC_PIP_EN, enabled);
      } break;
      case MSG_ID_UPDATE_MIC_PIP_EN: {
         bool enabled = !!data[0];

         mic_pip_enable(enabled);

         LOG_FINE("Update mic_pip state: en=%u", enabled);
      } break;
      default: {
         LOG_WARN("Unknown message: id=%u", cmd);
      } break;
   }
}

static inline bool rb_push(ringbuffer_t* rb, char c) {
   if (c == 0) // frame boundary marker, process frame
      return true;

   if (rb->index >= MSG_FRAME_SIZE)
      rb->index = 0;

   rb->buffer[rb->index++] = c;
   return false;
}

void protocol_process() {
   // Read UART
   while (uart_is_readable(UART_PORT)) {
      if (rb_push(&comm_uart, uart_getc(UART_PORT)))
         process_frame(COMM_UART, &comm_uart);
   }

   // Read STDIO
   int cc = 0;
   while ((cc = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      if (rb_push(&comm_stdio, (char)cc))
         process_frame(COMM_STDIO, &comm_stdio);
   }
}

static inline void write(comm_channel_t ch, uint8_t* src, size_t len) {
   switch (ch) {
      case COMM_UART:
         uart_write_blocking(UART_PORT, src, len);
         break;
      case COMM_STDIO:
         fwrite(src, 1, len, stdout);
         fflush(stdout);
         break;
      default:
         break;
   }
}

void protocol_write_frame(comm_channel_t ch, uint8_t* src, size_t len) {
   if (len == 0)
      return;

   cobs_encode_result ret = cobs_encode(frame_enc, sizeof(frame_enc), src, len);
   if (ret.status != COBS_ENCODE_OK) {
      LOG_FATAL("Frame encode failed! ret=%u (%s)", ret.status, cobs_encode_status_text[ret.status]); // should not happen

   } else if (ret.out_len == 0) {
      return;
   }

   // append frame boundary marker
   frame_enc[ret.out_len++] = 0;

   write(ch, frame_enc, ret.out_len);
}