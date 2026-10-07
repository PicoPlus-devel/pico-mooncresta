// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Couriersud, Stephane Humbert, Robbbert
/*
 * Moon Cresta (Nichibutsu, 1980) machine: memory map, inputs, frame timing,
 * palette and the tilemap/sprite/bullet/star renderer.
 *
 * Ported from MAME's src/mame/galaxian/galaxian.cpp and galaxian_v.cpp, for
 * the `mooncrst` set only (upright cabinet, Galaxian hardware with Nichibutsu's
 * graphics banking and encrypted program ROMs).
 *
 * Hardware summary:
 *   Z80 at 3.072 MHz. A latch at b000 lets the start of VBLANK raise NMI.
 *   384 x 264 frame at a 6.144 MHz pixel clock (60.61 Hz), 256 x 224 visible.
 *   A 32x32 tilemap of 8x8 2bpp tiles whose columns scroll individually,
 *   8 sprites of 16x16, 7 shells and 1 missile, and a starfield. The monitor
 *   is mounted on its side (MAME ROT90), so the upright picture is 224 x 256.
 */
#ifndef MOONCRESTA_H
#define MOONCRESTA_H

#include <stdint.h>

#include "mooncresta_sound.h"
#include "z80.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MCR_CYCLES_PER_LINE 192 /* 384 pixel clocks at twice the CPU clock */
#define MCR_LINES 264
#define MCR_FIRST_LINE 16 /* first visible line */
#define MCR_VISIBLE_LINES 224
#define MCR_CYCLES_PER_FRAME (MCR_CYCLES_PER_LINE * MCR_LINES) /* 50688 */

#define MCR_RAW_WIDTH 256
#define MCR_RAW_HEIGHT 224

/* Inputs, active high as on the board */
#define MCR_IN_COIN 0x01
#define MCR_IN_START1 0x02
#define MCR_IN_START2 0x04
#define MCR_IN_LEFT 0x08
#define MCR_IN_RIGHT 0x10
#define MCR_IN_FIRE 0x20

/* The ROM set, laid out as MAME's regions. maincpu is decrypted on load. */
typedef struct
{
    uint8_t maincpu[0x4000];
    uint8_t gfx[0x2000];
    uint8_t prom[0x20];
} mcr_roms_t;

/* What the renderer needs, copied at the start of VBLANK */
typedef struct
{
    uint8_t vram[0x400] __attribute__((aligned(4))); /* copied word by word */
    uint8_t obj[0x80] __attribute__((aligned(4)));   /* scroll/colour, sprites, bullets */
    uint8_t gfxbank[3];
    uint8_t stars_enabled;
    uint32_t star_origin;
} mcr_video_t;

typedef struct mcr
{
    z80_t cpu;
    const mcr_roms_t *roms;
    uint8_t ram[0x400] __attribute__((aligned(4)));
    uint8_t vram[0x400] __attribute__((aligned(4)));
    uint8_t obj[0x100] __attribute__((aligned(4)));
    uint8_t gfxbank[3];
    uint8_t nmi_enabled;
    uint8_t stars_enabled;
    uint8_t inputs; /* MCR_IN_* */
    uint8_t in1_dsw; /* IN1 bits 5-7 */
    uint8_t dsw;     /* IN2 */
    uint32_t star_origin;
    uint32_t frame;
    uint32_t star_origin_frame;

    mcr_video_t video;
    mcr_sound_t sound;

    int16_t *audio;
    int audio_samples; /* per frame */
    int audio_pos;
    int slice_end; /* frame cycle at which the current CPU run ends */
} mcr_t;

void mcr_init(mcr_t *m, const mcr_roms_t *roms, int samplerate, int samples_per_frame);
void mcr_reset(mcr_t *m);

/* Runs one frame (50688 CPU cycles) and renders samples_per_frame samples into
 * audio. m->video holds the picture of this frame afterwards. */
void mcr_run_frame(mcr_t *m, int16_t *audio);

/* The program ROMs of `mooncrst` are encrypted; this decodes them in place.
 * The loader calls it once, when the set becomes complete. */
void mcr_decrypt(uint8_t *maincpu, int length);

/* ---------------------------------------------------------------------------
 * Rendering
 * ------------------------------------------------------------------------ */

typedef enum
{
    MCR_ORIENT_ROTATED = 0,  /* Tate mode Off: turned upright for a normal monitor */
    MCR_ORIENT_TATE_CW = 1,  /* Bottom left: raw raster; monitor turned clockwise */
    MCR_ORIENT_TATE_CCW = 2, /* Bottom right: raw raster rotated 180; monitor turned counter-clockwise */
} mcr_orient_t;

typedef enum
{
    MCR_FMT_RGB444 = 0, /* 0000 RRRR GGGG BBBB (PicoDVI) */
    MCR_FMT_RGB555 = 1, /* 0RRR RRGG GGGB BBBB (HSTX) */
    MCR_FMT_RGB565 = 2, /* RRRR RGGG GGGB BBBB (host harness) */
} mcr_pixfmt_t;

#define MCR_MAX_STARS 320 /* the 2^17-1 period holds 256 */

typedef struct
{
    uint16_t pal[32];
    uint16_t star_pal[64];
    uint16_t bullet[2]; /* shell, missile */
    uint16_t black;
    /* 2bpp graphics packed with pixel i at bits 2i..2i+1.
     * Tiles (512): rows[code][y] runs along x; cols[code][x] runs along y.
     * Sprites (128): the same in 32-bit words of 16 pixels. */
    uint16_t tile_rows[512][8];
    uint16_t tile_cols[512][8];
    uint32_t spr_rows[128][16];
    uint32_t spr_cols[128][16];
    /* The enabled stars of one RNG period: offset into the period, colour */
    uint32_t star_ofs[MCR_MAX_STARS];
    uint8_t star_col[MCR_MAX_STARS];
    int nstars;
    uint8_t palette_rgb[32][3];
} mcr_gfx_t;

void mcr_gfx_init(mcr_gfx_t *g, const mcr_roms_t *roms, mcr_pixfmt_t fmt);

/* Draws the game area of the frame into a 320x240 canvas (stride in pixels).
 * Rotated mode shows the 224x256 picture as 224x240, centred, by dropping
 * every 16th row; tate shows the 256x224 raster centred.
 *
 * The borders around the game area are not touched, so anything drawn there
 * (the FPS counter) survives from frame to frame. Paint them with
 * mcr_render_borders() whenever the orientation changes or something else
 * has drawn over the canvas. */
void mcr_render(const mcr_gfx_t *g, const mcr_video_t *v, mcr_orient_t orient, uint16_t *fb, int stride);
void mcr_render_borders(const mcr_gfx_t *g, mcr_orient_t orient, uint16_t *fb, int stride);

#ifdef __cplusplus
}
#endif

#endif
