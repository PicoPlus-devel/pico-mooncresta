/*
 * Runs a CP/M .COM CPU test (ZEXDOC, ZEXALL) on the Z80 core, to check the
 * port before anything is built on top of it.
 *
 *   ./hosttest/zex_host <file.com>...
 *
 * ZEXDOC checks the documented flags, ZEXALL all of them (including the
 * undocumented X and Y bits). The test binaries are not part of this
 * repository.
 *
 * BDOS is emulated with two stubs: 0x0000 holds OUT (0),A (exit) and 0x0005
 * holds OUT (1),A / RET (print). Functions 2 (char) and 9 (string) only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z80.h"

static uint8_t ram[0x10000];
static int done;

static uint8_t mem_read(z80_t *c, uint16_t a)
{
    (void)c;
    return ram[a];
}

static void mem_write(z80_t *c, uint16_t a, uint8_t v)
{
    (void)c;
    ram[a] = v;
}

static uint8_t io_in(z80_t *c, uint16_t port)
{
    (void)c;
    (void)port;
    return 0xff;
}

static void io_out(z80_t *c, uint16_t port, uint8_t v)
{
    (void)v;
    if (done)
        return; /* the rest of the slice runs on into the stubs */
    if ((port & 0xff) == 0)
    {
        done = 1;
        return;
    }
    if ((port & 0xff) == 1)
    {
        if (c->bc.b.l == 2)
            putchar(c->de.b.l);
        else if (c->bc.b.l == 9)
            for (uint16_t a = c->de.w; ram[a] != '$'; a++)
                putchar(ram[a]);
        fflush(stdout);
    }
}

static int run_test(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    memset(ram, 0, sizeof(ram));
    size_t n = fread(ram + 0x100, 1, sizeof(ram) - 0x100, f);
    fclose(f);

    ram[0x0000] = 0xd3; /* OUT (0),A */
    ram[0x0001] = 0x00;
    ram[0x0005] = 0xd3; /* OUT (1),A */
    ram[0x0006] = 0x01;
    ram[0x0007] = 0xc9; /* RET */

    static z80_t cpu;
    z80_init(&cpu);
    for (int p = 0; p < 256; p++)
    {
        cpu.rd_page[p] = &ram[p << 8];
        cpu.wr_page[p] = &ram[p << 8];
    }
    cpu.read = mem_read;
    cpu.write = mem_write;
    cpu.in = io_in;
    cpu.out = io_out;
    z80_reset(&cpu);
    cpu.pc.w = 0x100;
    cpu.sp.w = 0xf000;

    printf("---- %s (%zu bytes) ----\n", path, n);
    done = 0;
    unsigned long long cycles = 0;
    while (!done)
    {
        int before = cpu.icount;
        z80_run(&cpu, 100000);
        cycles += (unsigned long long)(before + 100000 - cpu.icount);
    }
    printf("\n---- done, %llu cycles ----\n\n", cycles);
    return 0;
}

int main(int argc, char **argv)
{
    int rc = 0;
    for (int i = 1; i < argc; i++)
        rc |= run_test(argv[i]);
    return rc;
}
