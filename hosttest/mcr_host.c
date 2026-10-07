/*
 * Host harness for the Moon Cresta core: loads the ROM set through the same
 * loader as the firmware, runs the machine, dumps frames as PPM and the audio
 * as WAV.
 *
 *   ./hosttest/mcr_host <romdir> <frames> <dump-every-N> [outdir] [options]
 *
 * Options:
 *   --tate 0|1|2   Tate mode: 0 Off (default), 1 Bottom left, 2 Bottom right
 *   --wav FILE     write the audio (mono, 16-bit, 44.1 kHz)
 *   --coin F       insert a coin at frame F (held for 4 frames)
 *   --start F      press 1P start at frame F (held for 4 frames)
 *   --play F       from frame F: fire every 16 frames, sway left/right
 *
 * Every dumped frame is outdir/frame_NNNNN.ppm (320x240, the canvas the board
 * shows). A summary line per second reports the time spent per frame.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "mooncresta.h"
#include "romload.h"

#define SAMPLERATE 44100
#define SAMPLES_PER_FRAME 735

/* --- stdio implementation of mcr_io_t ----------------------------------- */

static int host_list_dir(void *ctx, const char *dir, mcr_dir_cb cb, void *arg)
{
    (void)ctx;
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_name[0] == '.')
            continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0)
            continue;
        cb(arg, e->d_name, (uint32_t)st.st_size, S_ISDIR(st.st_mode));
    }
    closedir(d);
    return 1;
}

static void *host_open(void *ctx, const char *path)
{
    (void)ctx;
    return fopen(path, "rb");
}

static uint32_t host_size(void *ctx, void *f)
{
    (void)ctx;
    long cur = ftell((FILE *)f);
    fseek((FILE *)f, 0, SEEK_END);
    long n = ftell((FILE *)f);
    fseek((FILE *)f, cur, SEEK_SET);
    return (uint32_t)n;
}

static int host_read_at(void *ctx, void *f, uint32_t ofs, void *buf, uint32_t len)
{
    (void)ctx;
    if (fseek((FILE *)f, ofs, SEEK_SET) != 0)
        return -1;
    return (int)fread(buf, 1, len, (FILE *)f);
}

static void host_close(void *ctx, void *f)
{
    (void)ctx;
    fclose((FILE *)f);
}

/* --- output helpers ------------------------------------------------------ */

static void write_ppm(const char *path, const uint16_t *fb)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n320 240\n255\n");
    for (int i = 0; i < 320 * 240; i++)
    {
        uint16_t p = fb[i];
        uint8_t rgb[3] = {(uint8_t)(((p >> 11) & 0x1f) * 255 / 31), (uint8_t)(((p >> 5) & 0x3f) * 255 / 63),
                          (uint8_t)((p & 0x1f) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void put16(FILE *f, uint16_t v)
{
    fputc(v & 0xff, f);
    fputc(v >> 8, f);
}

static void put32(FILE *f, uint32_t v)
{
    put16(f, v & 0xffff);
    put16(f, v >> 16);
}

static void wav_header(FILE *f, uint32_t samples)
{
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + samples * 2);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 1);
    put32(f, SAMPLERATE);
    put32(f, SAMPLERATE * 2);
    put16(f, 2);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, samples * 2);
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* --- main ---------------------------------------------------------------- */

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <romdir> <frames> <dump-every-N> [outdir] [options]\n", argv[0]);
        return 1;
    }
    const char *romdir = argv[1];
    int frames = atoi(argv[2]);
    int every = atoi(argv[3]);
    const char *outdir = "hosttest/out";
    int tate = 0, coin_at = -1, start_at = -1, play_at = -1;
    const char *wav_path = NULL;
    int a = 4;
    if (argc > 4 && argv[4][0] != '-')
        outdir = argv[a++];
    for (; a < argc; a++)
    {
        if (!strcmp(argv[a], "--tate") && a + 1 < argc)
            tate = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--wav") && a + 1 < argc)
            wav_path = argv[++a];
        else if (!strcmp(argv[a], "--coin") && a + 1 < argc)
            coin_at = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--start") && a + 1 < argc)
            start_at = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--play") && a + 1 < argc)
            play_at = atoi(argv[++a]);
        else
        {
            fprintf(stderr, "unknown option %s\n", argv[a]);
            return 1;
        }
    }

    static mcr_roms_t roms;
    mcr_io_t io = {NULL, host_list_dir, host_open, host_size, host_read_at, host_close};
    uint32_t found = mcr_romload(&roms, &io, romdir, 0);
    if (found != MCR_ROMS_ALL)
    {
        char sub[1024];
        snprintf(sub, sizeof(sub), "%s/mooncrst", romdir);
        found = mcr_romload(&roms, &io, sub, found);
    }
    int n_found = 0;
    for (int i = 0; i < MCR_ROMFILE_COUNT; i++)
        n_found += (found >> i) & 1;
    printf("ROM set: %d of %d files found\n", n_found, MCR_ROMFILE_COUNT);
    if (found != MCR_ROMS_ALL)
    {
        for (int i = 0; i < MCR_ROMFILE_COUNT; i++)
            if (!(found & (1u << i)))
                printf("  missing: %s (crc %08x)\n", mcr_romfiles[i].name, mcr_romfiles[i].crc);
        return 2;
    }

    mkdir(outdir, 0755);

    static mcr_t m;
    static mcr_gfx_t gfx;
    mcr_init(&m, &roms, SAMPLERATE, SAMPLES_PER_FRAME);
    mcr_gfx_init(&gfx, &roms, MCR_FMT_RGB565);

    FILE *wav = NULL;
    if (wav_path)
    {
        wav = fopen(wav_path, "wb");
        if (wav)
            wav_header(wav, 0);
    }

    static uint16_t fb[320 * 240];
    int16_t audio[SAMPLES_PER_FRAME];
    uint32_t total_samples = 0;
    double t_emu = 0, t_render = 0;
    int peak = 0;

    for (int f = 0; f < frames; f++)
    {
        uint8_t in = 0;
        if (coin_at >= 0 && f >= coin_at && f < coin_at + 4)
            in |= MCR_IN_COIN;
        if (start_at >= 0 && f >= start_at && f < start_at + 4)
            in |= MCR_IN_START1;
        if (play_at >= 0 && f >= play_at)
        {
            int p = f - play_at;
            if ((p & 15) < 4)
                in |= MCR_IN_FIRE;
            if ((p / 90) & 1)
                in |= MCR_IN_LEFT;
            else
                in |= MCR_IN_RIGHT;
        }
        m.inputs = in;

        double t0 = now_s();
        mcr_run_frame(&m, audio);
        double t1 = now_s();
        if (f == 0)
            mcr_render_borders(&gfx, (mcr_orient_t)tate, fb, 320);
        mcr_render(&gfx, &m.video, (mcr_orient_t)tate, fb, 320);
        double t2 = now_s();
        t_emu += t1 - t0;
        t_render += t2 - t1;

        for (int i = 0; i < SAMPLES_PER_FRAME; i++)
        {
            int v = audio[i] < 0 ? -audio[i] : audio[i];
            if (v > peak)
                peak = v;
        }
        if (wav)
        {
            fwrite(audio, sizeof(int16_t), SAMPLES_PER_FRAME, wav);
            total_samples += SAMPLES_PER_FRAME;
        }

        if (every > 0 && (f % every) == 0)
        {
            char path[1024];
            snprintf(path, sizeof(path), "%s/frame_%05d.ppm", outdir, f);
            write_ppm(path, fb);
        }
        if ((f + 1) % 60 == 0)
        {
            printf("frame %5d  pc=%04x  emu %.3f ms/frame  render %.3f ms/frame  audio peak %d\n", f + 1,
                   m.cpu.pc.w, t_emu * 1000 / 60, t_render * 1000 / 60, peak);
            t_emu = t_render = 0;
            peak = 0;
        }
    }

    if (wav)
    {
        wav_header(wav, total_samples);
        fclose(wav);
    }
    return 0;
}
