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
#include "filesystem.h"

#ifdef LFS_THREADSAFE
#include <pico/mutex.h>
#endif

#include <hardware/flash.h>
#include <hardware/regs/addressmap.h>
#include <hardware/sync.h>

// Size of filesystem in bytes
#define FS_SIZE (256 * 1024)

// Location of the filesystem in flash, located towards the end of the flash
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FS_SIZE)

static const uint8_t* const NOCACHE_FLASH = (const uint8_t*)(XIP_NOCACHE_NOALLOC_BASE + FLASH_TARGET_OFFSET);

static int flash_read(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size) {
   memcpy(buffer, NOCACHE_FLASH + (block * c->block_size) + off, size);
   return LFS_ERR_OK;
}

static int flash_prog(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size) {
   const uint32_t state = save_and_disable_interrupts();
   {
      const uint32_t offset = (uint32_t)FLASH_TARGET_OFFSET + (block * c->block_size) + off;
      flash_range_program(offset, buffer, size);
   }
   restore_interrupts(state);
   return LFS_ERR_OK;
}

static int flash_erase(const struct lfs_config* c, lfs_block_t block) {
   const uint32_t state = save_and_disable_interrupts();
   {
      const uint32_t offset = (uint32_t)FLASH_TARGET_OFFSET + (block * c->block_size);
      flash_range_erase(offset, c->block_size);
   }
   restore_interrupts(state);
   return LFS_ERR_OK;
}

// flash_prog() does not cache writes, so this function does nothing
static int flash_sync(const struct lfs_config* c) {
   (void)c;
   return 0;
}

#ifdef LFS_THREADSAFE

static int lock(const struct lfs_config* cfg) {
   return mutex_enter_timeout_ms(cfg->context, 5000) ? LFS_ERR_OK : LFS_ERR_TIMEOUT;
}

static int unlock(const struct lfs_config* cfg) {
   mutex_exit(cfg->context);
   return LFS_ERR_OK;
}

auto_init_mutex(fs_flash_mutex);

#endif

lfs_t fs_flash;

// littlefs config for flash
static struct lfs_config cfg_flash = {
    .read = flash_read,
    .prog = flash_prog,
    .erase = flash_erase,
    .sync = flash_sync,

#ifdef LFS_THREADSAFE
    .context = &fs_flash_mutex,
    .lock = lock,
    .unlock = unlock,
#endif

    .read_size = 1,
    .prog_size = FLASH_PAGE_SIZE,
    .block_size = FLASH_SECTOR_SIZE,
    .block_count = FS_SIZE / FLASH_SECTOR_SIZE,
    .cache_size = FLASH_SECTOR_SIZE / 4,
    .lookahead_size = 32, // 1 MiB lookahead (32 * 8 * BLOCK_SIZE)
    .block_cycles = 500,
};

int lfs_mountf(lfs_t* lfs, struct lfs_config* cfg, bool format_on_error) {
   int err = lfs_mount(lfs, cfg);

   if (err && format_on_error) {
      err = lfs_format(lfs, cfg);
      if (!err)
         return fs_flash_mount(false); // only try format once
   }
   return err;
}

int fs_flash_mount(bool format_on_error) {
   return lfs_mountf(&fs_flash, &cfg_flash, format_on_error);
}

int fs_flash_unmount() {
   return lfs_unmount(&fs_flash);
}

const char* lfs_err_msg(int err) {
   // errors are all negative, negate, so they can be stored in an array
   static const char* const msgs[] = {
       [-LFS_ERR_OK] = NULL,
       [-LFS_ERR_TIMEOUT] = "Mutex timeout",
       [-LFS_ERR_IO] = "Operation error",
       [-LFS_ERR_CORRUPT] = "Corrupted",
       [-LFS_ERR_NOENT] = "No directory entry",
       [-LFS_ERR_EXIST] = "Entry exists",
       [-LFS_ERR_NOTDIR] = "Entry not dir",
       [-LFS_ERR_ISDIR] = "Entry is dir",
       [-LFS_ERR_NOTEMPTY] = "Dir not empty",
       [-LFS_ERR_BADF] = "Bad file number",
       [-LFS_ERR_FBIG] = "File too large",
       [-LFS_ERR_INVAL] = "Invalid parameter",
       [-LFS_ERR_NOSPC] = "No space left on device",
       [-LFS_ERR_NOMEM] = "Out of memory",
       [-LFS_ERR_NOATTR] = "No data/attr available",
       [-LFS_ERR_NAMETOOLONG] = "File name too long",
   };
   if (err > 0)
      return NULL;
   return msgs[-err];
}