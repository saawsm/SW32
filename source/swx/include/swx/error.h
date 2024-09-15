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
#ifndef _SWX_ERROR_H
#define _SWX_ERROR_H

#define SWX_ERR_HW_POT (1 << 0)
#define SWX_ERR_HW_DAC (1 << 1)
#define SWX_ERR_HW_OUTPUT (1 << 2)

#define SWX_ERR_CAL (1 << 5)

#define SWX_ERR_FS (1 << 15)

#define SWX_ERR_GROUP_OUTPUT (SWX_ERR_HW_DAC | SWX_ERR_HW_OUTPUT | SWX_ERR_CAL)

#endif // _SWX_ERROR_H