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
#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include "swx.h"

#include <lfs.h>

#define LFS_ERR_TIMEOUT (-1)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum lfs_open_flags lfs_open_flags_t;

typedef struct lfs_info lfs_info_t;
typedef struct lfs_fsinfo lfs_fsinfo_t;

extern lfs_t fs_flash; // filesystem in flash

/// Mounts the filesystem
///
/// If format_on_error is true and mounting fails, format and try again.
/// Returns a negative error code on failure.
int lfs_mountf(lfs_t* lfs, struct lfs_config* cfg, bool format_on_error);

/// Mounts the filesystem in flash
///
/// If format_on_error is true and mounting fails, format and try again.
/// Filesystem accessible via fs_flash
/// Returns a negative error code on failure.
int fs_flash_mount(bool format_on_error);

/// Unmounts the filesystem in flash
///
/// Returns a negative error code on failure.
int fs_flash_unmount();

/// Converts an error code into a human readable error string
///
/// Returns a const string or NULL if no error.
const char* lfs_err_msg(int err);

#ifdef __cplusplus
}
#endif

#endif // _FILESYSTEM_H