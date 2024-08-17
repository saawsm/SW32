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
#ifndef _SWX_VERSION_H
#define _SWX_VERSION_H

#define	SWX_STRING(x)	#x
#define	SWX_XSTRING(x)	SWX_STRING(x)

#define SWX_VERSION_BITS (8)

#define SWX_VERSION_PCB_REV 1
#define SWX_VERSION_MAJOR 1
#define SWX_VERSION_MINOR 0

#define SWX_VERSION ((SWX_VERSION_PCB_REV << SWX_VERSION_BITS * 2) | (SWX_VERSION_MAJOR << SWX_VERSION_BITS * 1) | (SWX_VERSION_MINOR << SWX_VERSION_BITS * 0))

#define SWX_VERSION_STR ("v" SWX_XSTRING(SWX_VERSION_PCB_REV) "." SWX_XSTRING(SWX_VERSION_MAJOR) "." SWX_XSTRING(SWX_VERSION_MINOR))

#endif // _SWX_VERSION_H