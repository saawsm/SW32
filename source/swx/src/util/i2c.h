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
#ifndef _I2C_H
#define _I2C_H

#include "../swx.h"

#include <hardware/i2c.h>

#ifndef I2C_DEVICE_TIMEOUT
#define I2C_DEVICE_TIMEOUT (2000)
#endif

#ifdef __cplusplus
extern "C" {
#endif

int i2c_scan(i2c_inst_t* i2c);
bool i2c_check(i2c_inst_t* i2c, uint8_t addr);

int i2c_write(i2c_inst_t* i2c, uint8_t addr, const uint8_t* src, size_t len, bool nostop, uint timeout_us);
int i2c_read(i2c_inst_t* i2c, uint8_t addr, uint8_t* dst, size_t len, bool nostop, uint timeout_us);

#ifdef __cplusplus
}
#endif

#endif // _I2C_H