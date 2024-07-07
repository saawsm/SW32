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

#include <stdarg.h>

#include <hardware/uart.h>

#include <cobs.h>

#include "message.h"

typedef struct {
   size_t index;
   uint8_t buffer[MSG_FRAME_SIZE];
} ringbuffer_t;

static ringbuffer_t rb_uart = {0};
static ringbuffer_t rb_stdio = {0};

static uint8_t frame[MSG_FRAME_SIZE] = {0}; // decoded frame

void protocol_init() {
   uart_init(UART_PORT, UART_BAUD);
   uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);

   gpio_set_function(PIN_TXD1, GPIO_FUNC_UART);
   gpio_set_function(PIN_RXD1, GPIO_FUNC_UART);
}

static void process_frame(comm_channel_t ch, ringbuffer_t* rb) {
   if (rb->index == 0)
      return;

   cobs_decode_result ret = cobs_decode(frame, sizeof(frame), rb->buffer, rb->index);

   if (ret.status == COBS_DECODE_OK) {
      // [id:8][seq:8][len:8][payload:<len>][crc:16]
      uint8_t id = frame[0];
      uint8_t seq = frame[1];
      uint8_t len = frame[2];
      uint8_t* payload = &frame[3];
      uint16_t crc = *(uint16_t*)&payload[len];

      // TODO: Parse message

   } else {
      LOG_INFO("Invalid frame: %u\n", ret.status);
   }

   rb->index = 0;
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
   // read UART
   while (uart_is_readable(UART_PORT)) {
      if (rb_push(&rb_uart, uart_getc(UART_PORT)))
         process_frame(COMM_UART, &rb_uart);
   }

   // read STDIO
   int cc = 0;
   while ((cc = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      if (rb_push(&rb_stdio, (char)cc))
         process_frame(COMM_NONE, &rb_stdio);
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
   if (ch == COMM_NONE || len == 0)
      return;

   uint8_t frame[MSG_FRAME_SIZE];

   // the frame will always have a termination byte (0x00) so subtract one
   cobs_encode_result ret = cobs_encode(frame, sizeof(frame) - 1, src, len);
   if (ret.status != COBS_ENCODE_OK)
      LOG_FATAL("Encode frame failed: ret=%u\n", ret.status); // should not happen

   // add termination byte to frame
   frame[ret.out_len++] = 0;

   write(ch, frame, ret.out_len);
}

static const char log_level_symbols[] = {
    [LOG_LEVEL_NONE] = ' ',  //
    [LOG_LEVEL_FINE] = '*',  //
    [LOG_LEVEL_DEBUG] = 'D', //
    [LOG_LEVEL_INFO] = 'I',  //
    [LOG_LEVEL_WARN] = 'W',  //
    [LOG_LEVEL_ERROR] = 'E', //
    [LOG_LEVEL_FATAL] = 'F',
};

void write_log(log_level_t level, char* fmt, ...) {
   if (level > 9)
      level = 9;

   char buffer[255];

   va_list args;
   va_start(args, fmt);
   int len = vsnprintf(buffer + 4, sizeof(buffer) - 4, fmt, args); // +4 bytes for level prefix
   va_end(args);

   if (len <= 0)
      return;

   buffer[0] = '(';
   buffer[1] = log_level_symbols[level];
   buffer[2] = ')';
   buffer[3] = ' ';

   len += 4;

   fwrite(buffer, 1, len, stdout);
   fflush(stdout);
}
