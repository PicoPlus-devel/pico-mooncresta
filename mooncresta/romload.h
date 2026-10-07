/*
 * Moon Cresta ROM set loader.
 *
 * Finds the 13 files of MAME's `mooncrst` set in a directory, either inside a
 * zip (merged, split or non-merged; the clone subfolders of a merged set are
 * simply not matched) or as loose files. Files are identified by size and
 * CRC32 only, so their names do not matter. When the set becomes complete,
 * the program ROMs are decrypted in place.
 *
 * The file system is reached through mcr_io_t, so the same code runs on FatFs
 * on the board and on stdio in the host harness. Large buffers are taken from
 * the heap, never the stack: the inflate state alone is about 11 KB.
 */
#ifndef ROMLOAD_H
#define ROMLOAD_H

#include <stdint.h>

#include "mooncresta.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCR_ROMFILE_COUNT 13
#define MCR_ROMS_ALL ((1u << MCR_ROMFILE_COUNT) - 1)

typedef struct
{
    const char *name; /* MAME name, used in messages only */
    uint32_t crc;
    uint16_t size;
    uint8_t region; /* 0 maincpu, 1 gfx, 2 prom */
    uint16_t offset;
} mcr_romfile_t;

extern const mcr_romfile_t mcr_romfiles[MCR_ROMFILE_COUNT];

typedef void (*mcr_dir_cb)(void *arg, const char *name, uint32_t size, int is_dir);

typedef struct
{
    void *ctx;
    /* Calls cb for every entry of dir; returns 0 when dir cannot be opened. */
    int (*list_dir)(void *ctx, const char *dir, mcr_dir_cb cb, void *arg);
    void *(*open)(void *ctx, const char *path);
    uint32_t (*size)(void *ctx, void *file);
    /* Returns the number of bytes read, or -1. */
    int (*read_at)(void *ctx, void *file, uint32_t offset, void *buf, uint32_t len);
    void (*close)(void *ctx, void *file);
} mcr_io_t;

/* Searches dir: mooncrst.zip first, then any other .zip, then loose files.
 * `found` is the mask of files already loaded (bit i = mcr_romfiles[i]); the
 * updated mask is returned, MCR_ROMS_ALL when the set is complete. */
uint32_t mcr_romload(mcr_roms_t *roms, const mcr_io_t *io, const char *dir, uint32_t found);

#ifdef __cplusplus
}
#endif

#endif
