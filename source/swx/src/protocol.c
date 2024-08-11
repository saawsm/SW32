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

#include <hardware/uart.h>

#include "message.h"
#include "output.h"

#define U16_U8(value) ((value) >> 8), ((value) & 0xff)

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

static ringbuffer_t rb_uart = {0};
static ringbuffer_t rb_stdio = {0};

static uint8_t frame_dec[MSG_SIZE] = {0};       // decoded frame
static uint8_t frame_enc[MSG_FRAME_SIZE] = {0}; // encoded frame

void protocol_init() {
   uart_init(UART_PORT, UART_BAUD);
   uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);

   gpio_set_function(PIN_TXD1, GPIO_FUNC_UART);
   gpio_set_function(PIN_RXD1, GPIO_FUNC_UART);
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

   const uint8_t cmd = frame_dec[1] & ~MSG_UPDATE;
   const bool update = frame_dec[1] & MSG_UPDATE;
   const uint8_t* data = &frame_dec[2];

   switch (cmd) {
      case MSG_CMD_INFO: {
         uint8_t msg[] = {
             MSG_FRAME_START,           //
             MSG_CMD_INFO | MSG_UPDATE, //
             SWX_VERSION_PCB_REV,       //
             SWX_VERSION_MAJOR,         //
             SWX_VERSION_MINOR,         //
             CHANNEL_COUNT,             //
             U16_U8(CHANNEL_POWER_MAX),
         };
         protocol_write_frame(ch, msg, sizeof(msg));
      } break;
      case MSG_CMD_MAX_POWER: {
         uint8_t ch_mask = data[0];
         if (update) { // update max power

            uint16_t value = (data[1] << 8) | data[2];
            if (value > CHANNEL_POWER_MAX)
               value = CHANNEL_POWER_MAX;

            float pwr = (float)value / CHANNEL_POWER_MAX;

            for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
               if (ch_mask & (1 << ch_index)) {
                  channels[ch_index].max_power = pwr;
                  LOG_FINE("Update max_power: ch=%u value=%f", ch_index, pwr);
               }
            }
         } else { // fetch max power
            for (size_t ch_index = 0; ch_index < CHANNEL_COUNT; ch_index++) {
               if (ch_mask & (1 << ch_index)) {
                  float max_power = channels[ch_index].max_power;

                  LOG_FINE("Fetch max_power: ch=%u value=%f", ch_index, max_power);

                  uint16_t value = CHANNEL_POWER_MAX * max_power;

                  uint8_t msg[] = {
                      MSG_FRAME_START,                //
                      MSG_CMD_MAX_POWER | MSG_UPDATE, //
                      ch_index,
                      U16_U8(value),
                  };
                  protocol_write_frame(ch, msg, sizeof(msg));
               }
            }
         }
      } break;
      default:
         break;
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
      if (rb_push(&rb_uart, uart_getc(UART_PORT)))
         process_frame(COMM_UART, &rb_uart);
   }

   // Read STDIO
   int cc = 0;
   while ((cc = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      if (rb_push(&rb_stdio, (char)cc))
         process_frame(COMM_STDIO, &rb_stdio);
   }
}

static inline void write(comm_channel_t ch, uint8_t* src, size_t len) {
   switch (ch) {
      case COMM_UART: {
         uart_write_blocking(UART_PORT, src, len);
         break;
      }
      default: // TEMP: Replace with USB CDC
         fwrite(src, 1, len, stdout);
         fflush(stdout);
         break;
   }
}

void protocol_write_frame(comm_channel_t ch, uint8_t* src, size_t len) {
   if (len == 0)
      return;

   cobs_encode_result ret = cobs_encode(frame_enc, sizeof(frame_enc), src, len);
   if (ret.status != COBS_ENCODE_OK)
      LOG_FATAL("Frame encode failed! ret=%u (%s)", ret.status, cobs_encode_status_text[ret.status]); // should not happen
   else if (ret.out_len == 0)
      return;

   // append frame boundary marker
   frame_enc[ret.out_len++] = 0;

   write(ch, frame_enc, ret.out_len);
}