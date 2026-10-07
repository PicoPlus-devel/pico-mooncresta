// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Couriersud, Stephane Humbert, Robbbert
/*
 * Moon Cresta machine and video, ported from MAME's galaxian.cpp and
 * galaxian_v.cpp. See mooncresta.h.
 */
#include "mooncresta.h"

#include <string.h>

#include "mcr_port.h"

/* IN1 bits 6-7: bonus life at 30000, English. IN2: 1 coin 1 credit on both
 * slots. Cabinet (IN0 bit 5) upright. */
#define MCR_IN1_DSW_DEFAULT 0x80
#define MCR_IN2_DSW_DEFAULT 0x00

#define STAR_RNG_PERIOD ((1 << 17) - 1)

/* ---------------------------------------------------------------------------
 * Memory map (mooncrst_map)
 *
 *   0000-3fff  ROM
 *   8000-83ff  RAM (mirrored at 8400)
 *   9000-93ff  video RAM (mirrored at 9400)
 *   9800-98ff  object RAM (mirrored up to 9fff)
 *   a000       IN0           a000-a002  graphics bank     a003  coin counter
 *                            a004-a007  background LFO
 *   a800       IN1           a800-a807  sound latch
 *   b000       IN2 (DSW)     b000  NMI enable  b004  stars  b006/7  flip
 *   b800       watchdog      b800  pitch
 *
 * ROM, RAM, video RAM and object RAM go through the CPU's page table; only
 * a000-bfff and the unmapped space reach the callbacks.
 * ------------------------------------------------------------------------ */

/* Bring the sound stream up to the CPU's current position in the frame. */
static void MCR_HOT(sound_catchup)(mcr_t *m)
{
    if (!m->audio)
        return;
    int cycle = m->slice_end - m->cpu.icount;
    if (cycle < 0)
        cycle = 0;
    int target = (int)(((int64_t)cycle * m->audio_samples) / MCR_CYCLES_PER_FRAME);
    if (target > m->audio_samples)
        target = m->audio_samples;
    if (target > m->audio_pos)
    {
        mcr_sound_render(&m->sound, m->audio + m->audio_pos, target - m->audio_pos);
        m->audio_pos = target;
    }
}

static uint8_t MCR_HOT(mem_read)(z80_t *cpu, uint16_t addr)
{
    mcr_t *m = (mcr_t *)cpu->user;
    uint8_t v = 0;
    switch (addr & 0xf800)
    {
    case 0xa000:
        /* IN0: coin 1, coin 2, P1 left, right, fire; bit 5 cabinet (upright) */
        if (m->inputs & MCR_IN_COIN)
            v |= 0x01;
        if (m->inputs & MCR_IN_LEFT)
            v |= 0x04;
        if (m->inputs & MCR_IN_RIGHT)
            v |= 0x08;
        if (m->inputs & MCR_IN_FIRE)
            v |= 0x10;
        return v;
    case 0xa800:
        /* IN1: start 1, start 2, P2 left, right, fire, then DIP switches.
         * The game reads player 2 from here even on an upright cabinet, so
         * the same controls drive both players' bits. */
        v = m->in1_dsw;
        if (m->inputs & MCR_IN_START1)
            v |= 0x01;
        if (m->inputs & MCR_IN_START2)
            v |= 0x02;
        if (m->inputs & MCR_IN_LEFT)
            v |= 0x04;
        if (m->inputs & MCR_IN_RIGHT)
            v |= 0x08;
        if (m->inputs & MCR_IN_FIRE)
            v |= 0x10;
        return v;
    case 0xb000:
        return m->dsw;
    default:
        return 0xff; /* watchdog and unmapped */
    }
}

static void MCR_HOT(mem_write)(z80_t *cpu, uint16_t addr, uint8_t data)
{
    mcr_t *m = (mcr_t *)cpu->user;
    const int ofs = addr & 7;
    switch (addr & 0xf800)
    {
    case 0xa000:
        if (ofs < 3)
            m->gfxbank[ofs] = data;
        else if (ofs >= 4)
        {
            sound_catchup(m);
            mcr_sound_lfo_w(&m->sound, ofs - 4, data);
        }
        break; /* a003: coin counter */
    case 0xa800:
        sound_catchup(m);
        mcr_sound_w(&m->sound, ofs, data);
        break;
    case 0xb000:
        if (ofs == 0)
        {
            /* the latched D0 goes to CLEAR on the interrupt flip-flop */
            m->nmi_enabled = data & 1;
            if (!m->nmi_enabled)
                z80_set_nmi_line(&m->cpu, 0);
        }
        else if (ofs == 4)
        {
            if (!m->stars_enabled && (data & 1))
            {
                /* the shift register is released from CLR: the origin of
                 * this frame is 0 minus the clocks counted so far */
                int cycle = m->slice_end - m->cpu.icount;
                if (cycle < 0)
                    cycle = 0;
                int vpos = (MCR_FIRST_LINE + cycle / MCR_CYCLES_PER_LINE) % MCR_LINES;
                int hpos = (cycle % MCR_CYCLES_PER_LINE) * 2 * 3; /* MAME counts 3 per pixel */
                m->star_origin = STAR_RNG_PERIOD - (uint32_t)(vpos * 512 + hpos);
                m->star_origin_frame = m->frame;
            }
            m->stars_enabled = data & 1;
        }
        /* b006/b007: flip screen, used only by the cocktail cabinet */
        break;
    case 0xb800:
        sound_catchup(m);
        mcr_sound_pitch_w(&m->sound, data);
        break;
    default:
        break; /* ROM and unmapped */
    }
}

static uint8_t io_in(z80_t *cpu, uint16_t port)
{
    (void)cpu;
    (void)port;
    return 0xff;
}

static void io_out(z80_t *cpu, uint16_t port, uint8_t data)
{
    (void)cpu;
    (void)port;
    (void)data;
}

void mcr_decrypt(uint8_t *rom, int length)
{
    /* decode_mooncrst */
    for (int offs = 0; offs < length; offs++)
    {
        uint8_t data = rom[offs];
        uint8_t res = data;
        if (data & 0x02)
            res ^= 0x40;
        if (data & 0x20)
            res ^= 0x04;
        if (!(offs & 1))
            /* bitswap<8>(res, 7,2,5,4,3,6,1,0) */
            res = (res & 0xbb) | ((res & 0x04) << 4) | ((res & 0x40) >> 4);
        rom[offs] = res;
    }
}

void mcr_init(mcr_t *m, const mcr_roms_t *roms, int samplerate, int samples_per_frame)
{
    memset(m, 0, sizeof(*m));
    m->roms = roms;
    m->in1_dsw = MCR_IN1_DSW_DEFAULT;
    m->dsw = MCR_IN2_DSW_DEFAULT;
    m->audio_samples = samples_per_frame;

    z80_init(&m->cpu);
    m->cpu.user = m;
    m->cpu.read = mem_read;
    m->cpu.write = mem_write;
    m->cpu.in = io_in;
    m->cpu.out = io_out;
    for (int p = 0; p < 0x40; p++)
        m->cpu.rd_page[p] = &roms->maincpu[p << 8];
    for (int p = 0; p < 8; p++)
    {
        m->cpu.rd_page[0x80 + p] = m->cpu.wr_page[0x80 + p] = &m->ram[(p & 3) << 8];
        m->cpu.rd_page[0x90 + p] = m->cpu.wr_page[0x90 + p] = &m->vram[(p & 3) << 8];
        m->cpu.rd_page[0x98 + p] = m->cpu.wr_page[0x98 + p] = m->obj;
    }

    mcr_sound_init(&m->sound, samplerate);
    mcr_reset(m);
}

void mcr_reset(mcr_t *m)
{
    /* RAM keeps its contents over a reset on the real board; clearing it
     * makes Reset Game behave like a power cycle, as in the other games */
    memset(m->ram, 0, sizeof(m->ram));
    memset(m->vram, 0, sizeof(m->vram));
    memset(m->obj, 0, sizeof(m->obj));
    memset(m->gfxbank, 0, sizeof(m->gfxbank));
    m->nmi_enabled = 0;
    m->stars_enabled = 0;
    m->star_origin = 0;
    m->star_origin_frame = m->frame;
    z80_set_nmi_line(&m->cpu, 0);
    m->cpu.icount = 0;
    z80_reset(&m->cpu);
    mcr_sound_reset(&m->sound);
    memset(&m->video, 0, sizeof(m->video));
}

static void MCR_HOT(run_until)(mcr_t *m, int cycle)
{
    int start = m->slice_end;
    m->slice_end = cycle;
    z80_run(&m->cpu, cycle - start);
}

/* Word copy for the per-frame paths: memcpy lives in flash. The core is built
 * with -fno-tree-loop-distribute-patterns so this stays a loop. */
static inline void copy_words(void *dst, const void *src, int bytes)
{
    uint32_t *d = (uint32_t *)dst;
    const uint32_t *s = (const uint32_t *)src;
    for (int i = 0; i < bytes / 4; i++)
        d[i] = s[i];
}

void MCR_HOT(mcr_run_frame)(mcr_t *m, int16_t *audio)
{
    m->audio = audio;
    m->audio_pos = 0;
    m->slice_end = 0;

    /* visible lines 16-239 */
    run_until(m, MCR_VISIBLE_LINES * MCR_CYCLES_PER_LINE);

    /* VBLANK starts: this is when MAME draws the screen. The stars move one
     * RNG clock back per frame (the frame is 2^17 - 2 clocks long). */
    if (m->frame != m->star_origin_frame)
    {
        uint32_t delta = (m->frame - m->star_origin_frame) % STAR_RNG_PERIOD;
        m->star_origin = (m->star_origin + STAR_RNG_PERIOD - delta) % STAR_RNG_PERIOD;
        m->star_origin_frame = m->frame;
    }
    copy_words(m->video.vram, m->vram, sizeof(m->video.vram));
    copy_words(m->video.obj, m->obj, sizeof(m->video.obj));
    m->video.gfxbank[0] = m->gfxbank[0];
    m->video.gfxbank[1] = m->gfxbank[1];
    m->video.gfxbank[2] = m->gfxbank[2];
    m->video.stars_enabled = m->stars_enabled;
    m->video.star_origin = m->star_origin;
    m->frame++;

    /* the VBLANK interrupt line is clocked here while NMI is enabled */
    if (m->nmi_enabled)
        z80_set_nmi_line(&m->cpu, 1);
    run_until(m, MCR_CYCLES_PER_FRAME);

    /* finish the frame's audio; the CPU's overshoot belongs to the next one */
    m->slice_end = MCR_CYCLES_PER_FRAME;
    m->cpu.icount = m->cpu.icount > 0 ? 0 : m->cpu.icount;
    if (audio && m->audio_pos < m->audio_samples)
        mcr_sound_render(&m->sound, audio + m->audio_pos, m->audio_samples - m->audio_pos);
    m->audio = NULL;
}

/* ---------------------------------------------------------------------------
 * Palette (galaxian_palette): the colour PROM through three resistor nets,
 * weighted as MAME's compute_resistor_weights() does with autoscaling.
 * ------------------------------------------------------------------------ */

static void resistor_weights(const double *r, int count, double pulldown, double *w)
{
    for (int n = 0; n < count; n++)
    {
        double r0 = 1.0 / pulldown;
        double r1 = 1.0 / 1e12; /* no pull-up */
        for (int j = 0; j < count; j++)
        {
            if (j == n)
                r1 += 1.0 / r[j];
            else
                r0 += 1.0 / r[j];
        }
        r0 = 1.0 / r0;
        r1 = 1.0 / r1;
        double v = 255.0 * r0 / (r1 + r0);
        w[n] = v < 0 ? 0 : (v > 255 ? 255 : v);
    }
}

static uint16_t pack(int r, int g, int b, mcr_pixfmt_t fmt)
{
    switch (fmt)
    {
    case MCR_FMT_RGB444:
        return (uint16_t)(((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
    case MCR_FMT_RGB555:
        return (uint16_t)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
    default:
        return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

static void build_palette(mcr_gfx_t *g, const uint8_t *prom, mcr_pixfmt_t fmt)
{
    static const double rgb_res[3] = {1000, 470, 220};
    double rw[3], gw[3], bw[2];
    resistor_weights(rgb_res, 3, 470, rw);
    resistor_weights(rgb_res, 3, 470, gw);
    resistor_weights(rgb_res + 1, 2, 470, bw);
    /* autoscale to the network with the greatest output */
    double max = 0, sum;
    sum = rw[0] + rw[1] + rw[2];
    if (sum > max)
        max = sum;
    sum = gw[0] + gw[1] + gw[2];
    if (sum > max)
        max = sum;
    sum = bw[0] + bw[1];
    if (sum > max)
        max = sum;
    const double scale = 255.0 / max;

    for (int i = 0; i < 32; i++)
    {
        const uint8_t p = prom[i];
        int r = (int)((rw[0] * ((p >> 0) & 1) + rw[1] * ((p >> 1) & 1) + rw[2] * ((p >> 2) & 1)) * scale + 0.5);
        int gg = (int)((gw[0] * ((p >> 3) & 1) + gw[1] * ((p >> 4) & 1) + gw[2] * ((p >> 5) & 1)) * scale + 0.5);
        int b = (int)((bw[0] * ((p >> 6) & 1) + bw[1] * ((p >> 7) & 1)) * scale + 0.5);
        g->palette_rgb[i][0] = (uint8_t)r;
        g->palette_rgb[i][1] = (uint8_t)gg;
        g->palette_rgb[i][2] = (uint8_t)b;
        g->pal[i] = pack(r, gg, b, fmt);
    }

    /* Stars: 150 and 100 Ohm per component, compressed into 194..255 */
    const int minval = 255 * 130 / 150;
    const int midval = 255 * 130 / 100;
    const int maxval = 255 * 130 / 60;
    const int starmap[4] = {0, minval, minval + (255 - minval) * (midval - minval) / (maxval - minval), 255};
    for (int i = 0; i < 64; i++)
    {
        int r = starmap[(((i >> 4) & 1) << 1) | ((i >> 5) & 1)];
        int gg = starmap[(((i >> 2) & 1) << 1) | ((i >> 3) & 1)];
        int b = starmap[(((i >> 0) & 1) << 1) | ((i >> 1) & 1)];
        g->star_pal[i] = pack(r, gg, b, fmt);
    }

    /* shells white, the missile yellow */
    g->bullet[0] = pack(0xff, 0xff, 0xff, fmt);
    g->bullet[1] = pack(0xff, 0xff, 0x00, fmt);
    g->black = 0;
}

/* galaxian_charlayout / galaxian_spritelayout: 2 planes, the first (the most
 * significant bit) in the first half of the region; MSB first along x. */
static int gfx_pixel(const uint8_t *gfx, int byte, int x)
{
    int mask = 0x80 >> x;
    return ((gfx[byte] & mask) ? 2 : 0) | ((gfx[0x1000 + byte] & mask) ? 1 : 0);
}

static void decode_gfx(mcr_gfx_t *g, const uint8_t *gfx)
{
    for (int code = 0; code < 512; code++)
    {
        for (int i = 0; i < 8; i++)
        {
            uint16_t r = 0, c = 0;
            for (int j = 0; j < 8; j++)
            {
                r |= gfx_pixel(gfx, code * 8 + i, j) << (2 * j); /* row i, along x */
                c |= gfx_pixel(gfx, code * 8 + j, i) << (2 * j); /* column i, along y */
            }
            g->tile_rows[code][i] = r;
            g->tile_cols[code][i] = c;
        }
    }
    for (int code = 0; code < 128; code++)
    {
        for (int i = 0; i < 16; i++)
        {
            uint32_t r = 0, c = 0;
            for (int j = 0; j < 16; j++)
            {
                /* (x, y) lives at byte code*32 + (y&7) + (y&8 ? 16 : 0) + (x&8 ? 8 : 0) */
                int rb = code * 32 + (i & 7) + ((i & 8) ? 16 : 0) + ((j & 8) ? 8 : 0);
                int cb = code * 32 + (j & 7) + ((j & 8) ? 16 : 0) + ((i & 8) ? 8 : 0);
                r |= (uint32_t)gfx_pixel(gfx, rb, j & 7) << (2 * j);
                c |= (uint32_t)gfx_pixel(gfx, cb, i & 7) << (2 * j);
            }
            g->spr_rows[code][i] = r;
            g->spr_cols[code][i] = c;
        }
    }
}

/* The starfield is a 17-bit LFSR clocked twice per pixel; a star shows where
 * its top 8 bits are 1 and its low bit is 0. Only ~256 of the 2^17-1 states
 * qualify, so they are kept as a list instead of MAME's 128 KB table. */
static void init_stars(mcr_gfx_t *g)
{
    uint32_t shiftreg = 0;
    g->nstars = 0;
    for (uint32_t i = 0; i < STAR_RNG_PERIOD; i++)
    {
        if ((shiftreg & 0x1fe01) == 0x1fe00 && g->nstars < MCR_MAX_STARS)
        {
            g->star_ofs[g->nstars] = i;
            g->star_col[g->nstars] = (uint8_t)((~shiftreg & 0x1f8) >> 3);
            g->nstars++;
        }
        shiftreg = (shiftreg >> 1) | ((((shiftreg >> 12) ^ ~shiftreg) & 1) << 16);
    }
}

void mcr_gfx_init(mcr_gfx_t *g, const mcr_roms_t *roms, mcr_pixfmt_t fmt)
{
    memset(g, 0, sizeof(*g));
    build_palette(g, roms->prom, fmt);
    decode_gfx(g, roms->gfx);
    init_stars(g);
}

/* ---------------------------------------------------------------------------
 * Renderer
 *
 * Raw coordinates are MAME's unrotated bitmap: x 0-255, visible lines y
 * 16-239. Each output row is composed completely before the next one, in
 * the order the display scans them, so the single-buffered framebuffer is
 * rewritten ahead of the beam: black, stars, tiles, sprites 7 to 0 (lower
 * numbers win), bullets.
 * ------------------------------------------------------------------------ */

#define CANVAS_W 320
#define CANVAS_H 240
#define Y0 MCR_FIRST_LINE
#define Y1 (MCR_FIRST_LINE + MCR_VISIBLE_LINES) /* exclusive */
#define SPRITE_MIN_X 17                          /* hard clip at the line buffer */

typedef struct
{
    int16_t sx, sy;
    uint16_t code;
    uint8_t color, flipx, flipy;
} sprite_t;

typedef struct
{
    int16_t y, x; /* the bullet covers x .. x+3 on line y */
    uint8_t missile;
} bullet_t;

/* per-frame working set, prepared from the snapshot before drawing */
static struct
{
    sprite_t spr[8];
    bullet_t bul[8];
    int nbul;
    uint8_t sx[MCR_MAX_STARS], sy[MCR_MAX_STARS], sc[MCR_MAX_STARS];
    int16_t first[256], next[MCR_MAX_STARS]; /* stars by row key */
    uint16_t tile_code[0x400];               /* video RAM after the bank remap */
    uint16_t line[MCR_RAW_WIDTH] __attribute__((aligned(4)));
} rs;

static inline void fill(uint16_t *p, int n, uint16_t c)
{
    while (n-- > 0)
        *p++ = c;
}

static void MCR_HOT(prepare)(const mcr_gfx_t *g, const mcr_video_t *v, int star_key_is_x)
{
    const uint8_t *bank = v->gfxbank;

    /* mooncrst_extend_tile_info */
    for (int i = 0; i < 0x400; i++)
    {
        uint16_t code = v->vram[i];
        if (bank[2] && (code & 0xc0) == 0x80)
            code = (uint16_t)((code & 0x3f) | (bank[0] << 6) | (bank[1] << 7) | 0x100);
        rs.tile_code[i] = code & 0x1ff;
    }

    /* sprites (sprites_draw), flip screen not emulated */
    for (int n = 0; n < 8; n++)
    {
        const uint8_t *base = &v->obj[0x40 + n * 4];
        sprite_t *s = &rs.spr[n];
        /* the first three sprites match against y-1 */
        s->sy = (uint8_t)(240 - (base[0] - (n < 3)));
        uint16_t code = base[1] & 0x3f;
        s->flipx = (base[1] & 0x40) != 0;
        s->flipy = (base[1] & 0x80) != 0;
        s->color = base[2] & 7;
        s->sx = (uint8_t)(base[3] + 1); /* sprite vs tile layer */
        /* mooncrst_extend_sprite_info */
        if (bank[2] && (code & 0x30) == 0x20)
            code = (uint16_t)((code & 0x0f) | (bank[0] << 4) | (bank[1] << 5) | 0x40);
        s->code = code & 0x7f;
    }

    /* bullets (bullets_draw): per line one shell, the highest numbered that
     * matches, and the missile. Each entry matches one line at most. */
    rs.nbul = 0;
    const uint8_t *bb = &v->obj[0x60];
    for (int y = Y0; y < Y1; y++)
    {
        int shell = -1, missile = -1;
        for (int w = 0; w < 3; w++)
            if ((uint8_t)(bb[w * 4 + 1] + (uint8_t)(y - 1)) == 0xff)
                shell = w;
        for (int w = 3; w < 8; w++)
            if ((uint8_t)(bb[w * 4 + 1] + (uint8_t)y) == 0xff)
            {
                if (w != 7)
                    shell = w;
                else
                    missile = w;
            }
        if (shell >= 0)
            rs.bul[rs.nbul++] = (bullet_t){(int16_t)y, (int16_t)(255 - bb[shell * 4 + 3] - 4), 0};
        if (missile >= 0 && rs.nbul < 8)
            rs.bul[rs.nbul++] = (bullet_t){(int16_t)y, (int16_t)(255 - bb[missile * 4 + 3] - 4), 1};
        if (rs.nbul == 8)
            break;
    }

    /* stars of this frame, chained by line (tate) or by column (rotated) */
    for (int i = 0; i < 256; i++)
        rs.first[i] = -1;
    if (!v->stars_enabled)
        return;
    const uint32_t base_ofs = (v->star_origin + Y0 * 512) % STAR_RNG_PERIOD;
    int n = 0;
    for (int i = g->nstars - 1; i >= 0; i--)
    {
        int32_t d = (int32_t)g->star_ofs[i] - (int32_t)base_ofs;
        if (d < 0)
            d += STAR_RNG_PERIOD;
        if (d >= MCR_VISIBLE_LINES * 512)
            continue;
        int y = Y0 + d / 512;
        int x = (d % 512) >> 1;
        /* stars are suppressed unless V1 ^ H8 == 1 */
        if (!((y ^ (x >> 3)) & 1))
            continue;
        rs.sx[n] = (uint8_t)x;
        rs.sy[n] = (uint8_t)y;
        rs.sc[n] = g->star_col[i];
        int key = star_key_is_x ? x : y;
        rs.next[n] = rs.first[key];
        rs.first[key] = (int16_t)n;
        n++;
    }
}

/* One raw line y (256 pixels) into rs.line */
static void MCR_HOT(render_raw_line)(const mcr_gfx_t *g, const mcr_video_t *v, int y)
{
    uint16_t *line = rs.line;
    fill(line, MCR_RAW_WIDTH, g->black);

    for (int s = rs.first[y]; s >= 0; s = rs.next[s])
        line[rs.sx[s]] = g->star_pal[rs.sc[s]];

    /* tiles: every column scrolls on its own, pen 0 transparent */
    for (int col = 0; col < 32; col++)
    {
        const int ty = (y + v->obj[col * 2]) & 0xff;
        const uint16_t code = rs.tile_code[(ty >> 3) * 32 + col];
        uint16_t w = g->tile_rows[code][ty & 7];
        if (!w)
            continue;
        const uint16_t *pal = &g->pal[(v->obj[col * 2 + 1] & 7) << 2];
        uint16_t *dst = &line[col * 8];
        for (int px = 0; px < 8; px++, w >>= 2)
            if (w & 3)
                dst[px] = pal[w & 3];
    }

    for (int n = 7; n >= 0; n--)
    {
        const sprite_t *s = &rs.spr[n];
        int py = y - s->sy;
        if ((unsigned)py >= 16)
            continue;
        if (s->flipy)
            py = 15 - py;
        uint32_t w = g->spr_rows[s->code][py];
        if (!w)
            continue;
        const uint16_t *pal = &g->pal[s->color << 2];
        for (int cx = 0; cx < 16; cx++)
        {
            int x = s->sx + cx;
            if (x < SPRITE_MIN_X || x > 255)
                continue;
            int pix = (w >> (2 * (s->flipx ? 15 - cx : cx))) & 3;
            if (pix)
                line[x] = pal[pix];
        }
    }

    for (int b = 0; b < rs.nbul; b++)
    {
        if (rs.bul[b].y != y)
            continue;
        const uint16_t c = g->bullet[rs.bul[b].missile];
        for (int x = rs.bul[b].x; x < rs.bul[b].x + 4; x++)
            if (x >= 0 && x <= 255)
                line[x] = c;
    }
}

/* One raw column x, from line 239 up to line 16, into dst (224 pixels): the
 * upright picture's row */
static void MCR_HOT(render_raw_column)(const mcr_gfx_t *g, const mcr_video_t *v, int x, uint16_t *dst)
{
    fill(dst, MCR_RAW_HEIGHT, g->black);

    /* output column i shows line Y1 - 1 - i */
    for (int s = rs.first[x]; s >= 0; s = rs.next[s])
        dst[Y1 - 1 - rs.sy[s]] = g->star_pal[rs.sc[s]];

    {
        const int col = x >> 3, px = x & 7;
        const int scroll = v->obj[col * 2];
        const uint16_t *pal = &g->pal[(v->obj[col * 2 + 1] & 7) << 2];
        int i = 0;
        while (i < MCR_RAW_HEIGHT)
        {
            const int y = Y1 - 1 - i;
            const int ty = (y + scroll) & 0xff;
            const uint16_t code = rs.tile_code[(ty >> 3) * 32 + col];
            const uint16_t w = g->tile_cols[code][px];
            /* this tile covers rows ty&7 down to 0 */
            int py = ty & 7;
            if (!w)
            {
                i += py + 1;
                continue;
            }
            for (; py >= 0 && i < MCR_RAW_HEIGHT; py--, i++)
            {
                int pix = (w >> (2 * py)) & 3;
                if (pix)
                    dst[i] = pal[pix];
            }
        }
    }

    if (x >= SPRITE_MIN_X)
    {
        for (int n = 7; n >= 0; n--)
        {
            const sprite_t *s = &rs.spr[n];
            int cx = x - s->sx;
            if ((unsigned)cx >= 16)
                continue;
            if (s->flipx)
                cx = 15 - cx;
            uint32_t w = g->spr_cols[s->code][cx];
            if (!w)
                continue;
            const uint16_t *pal = &g->pal[s->color << 2];
            for (int py = 0; py < 16; py++)
            {
                int y = s->sy + py;
                if (y < Y0 || y >= Y1)
                    continue;
                int pix = (w >> (2 * (s->flipy ? 15 - py : py))) & 3;
                if (pix)
                    dst[Y1 - 1 - y] = pal[pix];
            }
        }
    }

    for (int b = 0; b < rs.nbul; b++)
    {
        if (x >= rs.bul[b].x && x < rs.bul[b].x + 4)
            dst[Y1 - 1 - rs.bul[b].y] = g->bullet[rs.bul[b].missile];
    }
}

/* Game area of each orientation in the 320x240 canvas */
static void game_area(mcr_orient_t orient, int *x0, int *y0, int *w, int *h)
{
    if (orient == MCR_ORIENT_ROTATED)
    {
        *w = MCR_RAW_HEIGHT; /* 224 */
        *h = CANVAS_H;       /* 240 */
    }
    else
    {
        *w = MCR_RAW_WIDTH;  /* 256 */
        *h = MCR_RAW_HEIGHT; /* 224 */
    }
    *x0 = (CANVAS_W - *w) / 2;
    *y0 = (CANVAS_H - *h) / 2;
}

void mcr_render_borders(const mcr_gfx_t *g, mcr_orient_t orient, uint16_t *fb, int stride)
{
    int x0, y0, w, h;
    game_area(orient, &x0, &y0, &w, &h);
    for (int r = 0; r < CANVAS_H; r++)
    {
        uint16_t *row = fb + r * stride;
        if (r < y0 || r >= y0 + h)
        {
            fill(row, CANVAS_W, g->black);
        }
        else
        {
            fill(row, x0, g->black);
            fill(row + x0 + w, CANVAS_W - x0 - w, g->black);
        }
    }
}

void MCR_HOT(mcr_render)(const mcr_gfx_t *g, const mcr_video_t *v, mcr_orient_t orient, uint16_t *fb, int stride)
{
    int x0, y0, w, h;
    game_area(orient, &x0, &y0, &w, &h);
    prepare(g, v, orient == MCR_ORIENT_ROTATED);

    if (orient == MCR_ORIENT_ROTATED)
    {
        /* Upright 224 x 256 picture: output row r shows raw column
         * x = r*16/15, so every 16th column (x = 15, 31, ...) is dropped. */
        for (int r = 0; r < CANVAS_H; r++)
            render_raw_column(g, v, (r * 16) / 15, fb + r * stride + x0);
        return;
    }

    for (int ry = 0; ry < MCR_RAW_HEIGHT; ry++)
    {
        if (orient == MCR_ORIENT_TATE_CW)
        {
            render_raw_line(g, v, Y0 + ry);
            copy_words(fb + (y0 + ry) * stride + x0, rs.line, sizeof(rs.line));
        }
        else
        {
            /* rotated by 180: the bottom line first, in scan order */
            render_raw_line(g, v, Y1 - 1 - ry);
            uint16_t *row = fb + (y0 + ry) * stride + x0;
            for (int x = 0; x < MCR_RAW_WIDTH; x++)
                row[x] = rs.line[MCR_RAW_WIDTH - 1 - x];
        }
    }
}
