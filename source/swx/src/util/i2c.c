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
#include "i2c.h"

#include <pico/mutex.h>

#define I2C_MUTEX_TIMEOUT (10000)

#ifdef I2C_MUTEX_TIMEOUT
auto_init_mutex(mutex_i2c0);
auto_init_mutex(mutex_i2c1);

static inline mutex_t* get_mutex(i2c_inst_t* i2c) {
   return (i2c_hw_index(i2c) == 0) ? &mutex_i2c0 : &mutex_i2c1;
}
#endif

int i2c_scan(i2c_inst_t* i2c) {
   LOG_DEBUG("Scanning I2C devices...");

   for (uint8_t address = 0; address <= 0x7F; address++) {
      if (i2c_check(i2c, address)) {
         LOG_DEBUG("Found device at 0x%02x", address);
      }
   }

   LOG_DEBUG("Done.");

   return 0;
}

int i2c_write(i2c_inst_t* i2c, uint8_t addr, const uint8_t* src, size_t len, bool nostop, uint timeout_us) {
#ifdef I2C_MUTEX_TIMEOUT
   if (!mutex_enter_timeout_us(get_mutex(i2c), I2C_MUTEX_TIMEOUT)) {
      LOG_WARN("i2c_write: addr=%u - Mutex timeout!", addr);
      return -1;
   }
#endif

   int ret = i2c_write_timeout_us(i2c, addr, src, len, nostop, timeout_us);

#ifdef I2C_MUTEX_TIMEOUT
   mutex_exit(get_mutex(i2c));
#endif
   return ret;
}

int i2c_read(i2c_inst_t* i2c, uint8_t addr, uint8_t* dst, size_t len, bool nostop, uint timeout_us) {
#ifdef I2C_MUTEX_TIMEOUT
   if (!mutex_enter_timeout_us(get_mutex(i2c), I2C_MUTEX_TIMEOUT)) {
      LOG_WARN("i2c_read: addr=%u - Mutex timeout!", addr);
      return -1;
   }
#endif

   int ret = i2c_read_timeout_us(i2c, addr, dst, len, nostop, timeout_us);

#ifdef I2C_MUTEX_TIMEOUT
   mutex_exit(get_mutex(i2c));
#endif
   return ret;
}

bool i2c_check(i2c_inst_t* i2c, uint8_t addr) {
   // I2C reserves some addresses for special purposes. These are 1111XXX and 0000XXX.
   if ((addr & 0x78) == 0 || (addr & 0x78) == 0x78)
      return false;

#ifdef I2C_MUTEX_TIMEOUT
   if (!mutex_enter_timeout_us(get_mutex(i2c), I2C_MUTEX_TIMEOUT)) {
      LOG_WARN("i2c_read: addr=%u - Mutex timeout!", addr);
      return -1;
   }
#endif

   // Perform one byte dummy read using the address.
   // If a slave acknowledges, the number of bytes transferred is returned.
   // If the address is ignored, the function returns -2.
   uint8_t data;
   const bool ack = i2c_read_timeout_us(i2c, addr, &data, 1, false, I2C_DEVICE_TIMEOUT) > 0;

#ifdef I2C_MUTEX_TIMEOUT
   mutex_exit(get_mutex(i2c));
#endif
   return ack;
}
