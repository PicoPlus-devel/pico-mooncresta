/*
 * picoMoonCresta - Moon Cresta (Nichibutsu, 1980) arcade emulator for RP2350
 * boards.
 *
 * Boots the pico_shared framework, loads the ROM set from the SD card and runs
 * the emulated board (mooncresta/) one frame per display frame. The core is
 * plain C shared with the host harness in hosttest/; this file wires it to the
 * display, audio and controller drivers and to the settings menu.
 *
 * Like picoPhoenix and picoOutRun, and unlike the console emulators, there is
 * no ROM browser:
 *   - the board boots straight into the game; menu() is never called and only
 *     the in-game settings menu (SELECT + START) is used.
 *   - the ROM set is read from ROMDIR: MAME's mooncrst.zip as is, or its files
 *     unzipped. When it is missing or incomplete, an error screen names the
 *     missing files.
 *
 * Moon Cresta is a vertical game. The Tate mode setting chooses between the
 * picture turned upright for a normal monitor (default) and the unrotated
 * raster for a monitor turned on its side; see mooncresta/mooncresta.h.
 */

#include <cstdio>
#include <cstring>

#include "pico/stdlib.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"

#include "ff.h"
#include "tusb.h"

#include "FrensHelpers.h"
#include "FrensFonts.h"
#include "gamepad.h"
#include "menu.h"
#include "menu_settings.h"
#include "nespad.h"
#include "settings.h"
#include "vumeter.h"
#include "wiipad.h"

#include "mooncresta.h"
#include "romload.h"

// The emulated machine is light: a Z80 at 3.07 MHz, a tilemap, eight sprites
// and a few sound circuits. The standard 252 MHz clock (10x the 640x480 pixel
// clock) leaves most of core0 idle.
#define MOONCRESTA_CLOCKFREQ_KHZ 252000

// Must be a power of two (util::RingBuffer asserts it). It holds the audio
// cushion (see pushAudio()) plus a whole frame of 735 samples. The I2S ring of
// pico_shared is given the same size in CMakeLists.txt.
#define AUDIOBUFFERSIZE 2048

#define SAMPLERATE 44100
#define SAMPLES_PER_FRAME (SAMPLERATE / 60) // 735

#define ROMDIR "/roms/arcade/MOONCRESTA"

// Output gains in Q8 (256 = unity). The game's sound peaks at about 2/3 of
// full scale, which is too loud next to the other emulators of this family, so
// HDMI audio (HSTX and PicoDVI) gets half the level. The I2S DAC drives the
// amplifier or headphones directly, with no volume stage after it, and was still
// too loud at half: it gets a quarter (-12 dB), as in pico-galagino.
#ifndef DVI_AUDIO_GAIN_Q8
#define DVI_AUDIO_GAIN_Q8 128
#endif
#ifndef EXT_AUDIO_GAIN_Q8
#define EXT_AUDIO_GAIN_Q8 64
#endif

static uint32_t CPUFreqKHz = MOONCRESTA_CLOCKFREQ_KHZ;

static mcr_roms_t roms;
static mcr_t machine;
static mcr_gfx_t gfx;
static int16_t audioBuf[SAMPLES_PER_FRAME];

static bool haveRoms = false;
static uint32_t romsFound = 0;
static bool showSettings = false;
static uint32_t fps = 0;
static uint64_t lastFrameUs = 0;

// ---------------------------------------------------------------------------
// Settings menu wiring.
//
// Positional, indexed by MenuSettingsIndex - append only, never reorder.
// 1 = shown, 0 = hidden, -1 = never shown. 0 and -1 differ only for Exit, Save/
// Restore state and Reset, which the in-game menu shows unless they are -1.
// C++ designated initializers may not skip members, so every entry up to the
// last one used is listed.
// ---------------------------------------------------------------------------
int8_t g_settings_visibility_mooncresta[MOPT_COUNT] = {
    [MOPT_EXIT_GAME] = -1,                         // nowhere to exit to: there is no ROM browser
    [MOPT_RESET_GAME] = 1,
    [MOPT_REBOOT_TO_LOADER] = BOOTLOADER_BUILD,    // return to the bootloader's picker
    [MOPT_SAVE_RESTORE_STATE] = -1,                // no save states
    [MOPT_SCREENMODE] = 1,
    [MOPT_SCANLINES] = 0,                          // covered by the screen modes
    [MOPT_SCANLINE_TYPE] = HSTX,
    [MOPT_FPS_OVERLAY] = 1,
    [MOPT_AUDIO_ENABLE] = 1,
    [MOPT_FRAMESKIP] = 0,
    [MOPT_DISPLAY_MODE] = HSTX && ENABLEDVI,
    [MOPT_EXTERNAL_AUDIO] = EXT_AUDIO_IS_ENABLED,
    [MOPT_FONT_COLOR] = 0,
    [MOPT_FONT_BACK_COLOR] = 0,
    [MOPT_FRUITJAM_VUMETER] = ENABLE_VU_METER,
    [MOPT_FRUITJAM_VOLUME_CONTROL] = (HW_CONFIG == 8),
    [MOPT_DMG_PALETTE] = 0,                        // Game Boy
    [MOPT_BORDER_MODE] = 0,                        // Game Boy
    [MOPT_RAPID_FIRE_ON_A] = 1,                    // A fires
    [MOPT_RAPID_FIRE_ON_B] = 1,                    // B fires as well
    [MOPT_AUTO_INSERT_FDS_DISK_A] = 0,             // Famicom Disk System
    [MOPT_AUTO_SWAP_FDS_DISK] = 0,                 // Famicom Disk System
    [MOPT_FDS_DISK_SWAP] = 0,                      // Famicom Disk System
    [MOPT_OVERCLOCK] = 0,                          // ROM browser only, and not needed
    [MOPT_FM_AUDIO] = 0,                           // Master System YM2413
    [MOPT_ENTER_BOOTSEL_MODE] = 1,
    [MOPT_CONTROLLER_TEST] = 1,
    [MOPT_RECENT_GAMES] = 0,                       // ROM browser only
    [MOPT_USB_DRIVE_MODE] = 0,                     // shown regardless: FRENS_FORCE_USB_MSC_IN_SETTINGS
    [MOPT_CASSETTE] = 0,                           // TI-99/4A
    [MOPT_DISK] = 0,                               // TI-99/4A
    [MOPT_SERIAL_KEYBOARD] = 0,                    // TI-99/4A
    [MOPT_SPRITE_LIMIT] = 0,                       // NES
    [MOPT_MENU_OVERSCAN] = 0,                      // shown regardless: listed below the menu colors
    [MOPT_GENESIS_PAD] = 0,                        // Genesis
    [MOPT_NES_PALETTE] = 0,                        // NES
    [MOPT_HSTX_CLOCK_FIX] = 0,                     // only matters at 378 MHz and up
    [MOPT_BUTTON_LAYOUT] = 0,                      // NES
    [MOPT_TATE_MODE] = 1,                          // Moon Cresta is a vertical game
};

// The 8:7 modes stretch NES pixels; Moon Cresta has square-ish pixels, so only
// the 1:1 modes are offered.
const uint8_t g_available_screen_modes_mooncresta[] = {
    0, // SCANLINE_8_7
    0, // NOSCANLINE_8_7
    1, // SCANLINE_1_1
    1, // NOSCANLINE_1_1
};

// ---------------------------------------------------------------------------
// Framebuffer access: the one place the two video back-ends differ.
// Both are a contiguous 320x240 16-bit buffer, RGB555 on HSTX and RGB444 on
// PicoDVI.
// ---------------------------------------------------------------------------
static inline uint16_t *fbLine(int line)
{
#if HSTX
    return hstx_getlineFromFramebuffer(line);
#else
    return &Frens::framebuffer[line * SCREENWIDTH];
#endif
}

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
#if HSTX
    return (uint16_t)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
#else
    return (uint16_t)(((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
#endif
}

// ---------------------------------------------------------------------------
// Text on the framebuffer, for the error screen and the FPS overlay. pico_shared's
// putText() only works inside its own menus, but the 8x8 font is exported.
// ---------------------------------------------------------------------------
static void screenClear(uint16_t bg)
{
    for (int y = 0; y < SCREENHEIGHT; y++)
    {
        uint16_t *dst = fbLine(y);
        for (int x = 0; x < SCREENWIDTH; x++)
            dst[x] = bg;
    }
}

// col/row in 8x8 cells of the 40x30 screen; col < 0 centres the text.
static void screenText(int col, int row, const char *s, uint16_t fg, uint16_t bg)
{
    const int cols = SCREENWIDTH / FONT_CHAR_WIDTH;
    const int len = (int)strlen(s);
    if (col < 0)
        col = (len < cols) ? (cols - len) / 2 : 0;
    for (int i = 0; i < len && col + i < cols; i++)
    {
        char c = s[i];
        if (c < FONT_FIRST_ASCII || c >= FONT_FIRST_ASCII + FONT_N_CHARS)
            c = ' ';
        for (int line = 0; line < FONT_CHAR_HEIGHT; line++)
        {
            uint16_t *dst = fbLine(row * FONT_CHAR_HEIGHT + line) + (col + i) * FONT_CHAR_WIDTH;
            // leftmost pixel in the low bit
            char slice = getcharslicefrom8x8font(c, line);
            for (int bit = 0; bit < FONT_CHAR_WIDTH; bit++, slice >>= 1)
                *dst++ = (slice & 1) ? fg : bg;
        }
    }
}

#define SCR_BG rgb(0, 0, 0)
#define SCR_FG rgb(200, 200, 200)
#define SCR_HI rgb(255, 64, 64)
#define SCR_FILE rgb(255, 255, 255)

static void drawErrorScreen(bool sdOk)
{
    char buf[48];
    int row = 2;

    screenClear(SCR_BG);
    screenText(-1, row++, "picoMoonCresta", SCR_HI, SCR_BG);
    row++;
    if (!sdOk)
    {
        screenText(1, row++, "No SD card found.", SCR_HI, SCR_BG);
        row++;
    }
    else
    {
        int missing = 0;
        for (int i = 0; i < MCR_ROMFILE_COUNT; i++)
            missing += !(romsFound & (1u << i));
        if (missing == MCR_ROMFILE_COUNT)
        {
            screenText(1, row++, "No Moon Cresta ROMs found in", SCR_HI, SCR_BG);
            snprintf(buf, sizeof(buf), "  %s", ROMDIR);
            screenText(1, row++, buf, SCR_FILE, SCR_BG);
        }
        else
        {
            snprintf(buf, sizeof(buf), "Incomplete ROM set: %d of %d missing:", missing, MCR_ROMFILE_COUNT);
            screenText(1, row++, buf, SCR_HI, SCR_BG);
            int shown = 0;
            for (int i = 0; i < MCR_ROMFILE_COUNT && shown < 8; i++)
            {
                if (!(romsFound & (1u << i)))
                {
                    snprintf(buf, sizeof(buf), "  %s", mcr_romfiles[i].name);
                    screenText(1, row++, buf, SCR_FILE, SCR_BG);
                    shown++;
                }
            }
            if (missing > shown)
            {
                snprintf(buf, sizeof(buf), "  ...and %d more", missing - shown);
                screenText(1, row++, buf, SCR_FG, SCR_BG);
            }
        }
        row++;
    }

    row = 17;
    screenText(1, row++, "Copy MAME's mooncrst.zip (Nichibutsu", SCR_FG, SCR_BG);
    screenText(1, row++, "parent set) unchanged, or its files", SCR_FG, SCR_BG);
    screenText(1, row++, "unzipped, to the SD card folder", SCR_FG, SCR_BG);
    snprintf(buf, sizeof(buf), "  %s", ROMDIR);
    screenText(1, row++, buf, SCR_FILE, SCR_BG);

    screenText(1, 26, "SELECT+START opens settings, which", SCR_HI, SCR_BG);
    screenText(1, 27, "can enter USB drive mode.", SCR_HI, SCR_BG);
}

// ---------------------------------------------------------------------------
// ROM loading through FatFs
// ---------------------------------------------------------------------------
static int ffListDir(void *ctx, const char *dir, mcr_dir_cb cb, void *arg)
{
    (void)ctx;
    DIR *d = (DIR *)Frens::f_malloc(sizeof(DIR));
    FILINFO *fno = (FILINFO *)Frens::f_malloc(sizeof(FILINFO));
    int ok = 0;
    if (d && fno && f_opendir(d, dir) == FR_OK)
    {
        ok = 1;
        while (f_readdir(d, fno) == FR_OK && fno->fname[0])
            cb(arg, fno->fname, (uint32_t)fno->fsize, (fno->fattrib & AM_DIR) ? 1 : 0);
        f_closedir(d);
    }
    Frens::f_free(fno);
    Frens::f_free(d);
    return ok;
}

static void *ffOpen(void *ctx, const char *path)
{
    (void)ctx;
    FIL *fil = (FIL *)Frens::f_malloc(sizeof(FIL));
    if (fil && f_open(fil, path, FA_READ) == FR_OK)
        return fil;
    Frens::f_free(fil);
    return nullptr;
}

static uint32_t ffSize(void *ctx, void *file)
{
    (void)ctx;
    return (uint32_t)f_size((FIL *)file);
}

static int ffReadAt(void *ctx, void *file, uint32_t ofs, void *buf, uint32_t len)
{
    (void)ctx;
    UINT br = 0;
    if (f_lseek((FIL *)file, ofs) != FR_OK || f_read((FIL *)file, buf, len, &br) != FR_OK)
        return -1;
    return (int)br;
}

static void ffClose(void *ctx, void *file)
{
    (void)ctx;
    f_close((FIL *)file);
    Frens::f_free(file);
}

static bool loadRoms()
{
    const mcr_io_t io = {nullptr, ffListDir, ffOpen, ffSize, ffReadAt, ffClose};
    romsFound = mcr_romload(&roms, &io, ROMDIR, 0);
    if (romsFound != MCR_ROMS_ALL)
        romsFound = mcr_romload(&roms, &io, ROMDIR "/mooncrst", romsFound);
    printf("ROM set: mask %04lx (%s)\n", (unsigned long)romsFound,
           romsFound == MCR_ROMS_ALL ? "complete" : "incomplete");
    return romsFound == MCR_ROMS_ALL;
}

// ---------------------------------------------------------------------------
// Audio: one frame of mono samples to whichever sink is active.
//
// The machine renders a frame's 735 samples in one go, after the frame has
// run. Pushed as a burst into a sink with nothing queued in front of it, the
// sink runs dry just before the next burst arrives and plays a short gap on
// every frame (PicoDVI fills it with silence, the I2S DMA stops): a buzz at
// the frame rate. So the PicoDVI and I2S rings keep a cushion of
// AUDIO_CUSHION samples queued in front of each frame's burst:
//   - when the sink has drained (at start-up, after the settings menu, after a
//     slow frame), the cushion is refilled with silence, once;
//   - when the level has drifted more than AUDIO_TRIM_BAND away from the
//     cushion, the frame pushes one sample less or one more. That matches the
//     sink's clock to the emulated one: one sample per frame is 1360 ppm, far
//     more than the two clocks differ.
// The HSTX data-island queue manages its own level and is fed as it is.
//
// Runs from SRAM (noinline, or it would be folded into the flash-resident
// caller).
// ---------------------------------------------------------------------------
#define AUDIO_CUSHION 512   // ~12 ms
#define AUDIO_TRIM_BAND 192 // drift tolerated before a frame is trimmed

static inline int16_t applyGain(int x, int gainQ8)
{
    int32_t v = (x * gainQ8) >> 8;
    if (v > 32767)
        v = 32767;
    else if (v < -32768)
        v = -32768;
    return (int16_t)v;
}

// The two ring sinks: the I2S DAC (ext) or, on PicoDVI, the HDMI audio ring.
static inline int ringQueued(bool ext)
{
#if EXT_AUDIO_IS_ENABLED
    if (ext)
        return I2S_AUDIO_RING_SIZE - 1 - EXT_AUDIO_GET_FREE();
#endif
#if !HSTX
    return (int)dvi_->getAudioRingBuffer().getFullReadableSize();
#else
    return 0;
#endif
}

static inline void ringPut(bool ext, int16_t s)
{
#if EXT_AUDIO_IS_ENABLED
    if (ext)
    {
        EXT_AUDIO_ENQUEUE_SAMPLE(s, s);
        return;
    }
#endif
#if !HSTX
    auto &ring = dvi_->getAudioRingBuffer();
    if (ring.getWritableSize() == 0)
        return; // full: dropped
    *ring.getWritePointer() = {s, s};
    ring.advanceWritePointer(1);
#endif
}

static void __noinline __not_in_flash_func(pushAudio)(const int16_t *buf, int n)
{
    const bool mute = !settings.flags.audioEnabled;

#if EXT_AUDIO_IS_ENABLED
#if HSTX
    const bool ext = settings.flags.useExtAudio || Frens::isHeadPhoneJackConnected();
#else
    const bool ext = settings.flags.useExtAudio;
#endif
#else
    const bool ext = false;
#endif

#if HSTX
    if (!ext)
    {
        for (int i = 0; i < n; i++)
        {
            int16_t s = mute ? 0 : buf[i];
#if ENABLE_VU_METER
            if (settings.flags.enableVUMeter)
                addSampleToVUMeter(s);
#endif
            int16_t g = applyGain(s, DVI_AUDIO_GAIN_Q8);
            hstx_push_audio_sample(g, g);
        }
        return;
    }
#endif

    // the cushion and the drift trim, see above
    const int queued = ringQueued(ext);
    int repeat = 0;
    if (queued < AUDIO_CUSHION / 4)
    {
        for (int i = queued; i < AUDIO_CUSHION; i++)
            ringPut(ext, 0);
    }
    else if (queued > AUDIO_CUSHION + AUDIO_TRIM_BAND)
    {
        n--;
    }
    else if (queued < AUDIO_CUSHION - AUDIO_TRIM_BAND)
    {
        repeat = 1;
    }

    int16_t s = 0;
    for (int i = 0; i < n; i++)
    {
        s = mute ? 0 : buf[i];
#if ENABLE_VU_METER
        if (settings.flags.enableVUMeter)
            addSampleToVUMeter(s);
#endif
        ringPut(ext, applyGain(s, ext ? EXT_AUDIO_GAIN_Q8 : DVI_AUDIO_GAIN_Q8));
    }
    if (repeat)
        ringPut(ext, applyGain(s, ext ? EXT_AUDIO_GAIN_Q8 : DVI_AUDIO_GAIN_Q8));
}

// ---------------------------------------------------------------------------
// Video: draw the frame the machine finished last, plus the FPS digits. Called
// right after the frame pace returns, i.e. at the start of output VBLANK, so
// the single-buffered framebuffer is rewritten ahead of scan-out.
//
// The borders are painted only when they need it: at the first frame, when the
// orientation changes, after the settings menu has drawn over the screen, and
// when the FPS counter is switched off. The counter lives in the border, so it
// is overwritten in place every frame instead of being cleared and redrawn -
// clearing it with the whole canvas, as a full repaint did, erased it just
// before the display scanned the top rows, and it was rarely seen.
// ---------------------------------------------------------------------------
static bool bordersValid = false;
static int bordersOrient = -1;
static bool fpsShown = false;

static void __noinline __not_in_flash_func(presentFrame)()
{
    const mcr_orient_t orient = (mcr_orient_t)settings.flags.tateMode;
    const bool showFps = settings.flags.displayFrameRate;

    if (!bordersValid || orient != bordersOrient || (fpsShown && !showFps))
    {
        mcr_render_borders(&gfx, orient, fbLine(0), SCREENWIDTH);
        bordersValid = true;
        bordersOrient = orient;
    }
    fpsShown = showFps;

    mcr_render(&gfx, &machine.video, orient, fbLine(0), SCREENWIDTH);

    if (showFps)
    {
        char s[3] = {(char)('0' + (fps / 10) % 10), (char)('0' + fps % 10), 0};
        // Cell (1, 1): in every orientation that is border, not game, and one
        // cell in from the corner a TV that overscans cuts off.
        screenText(1, 1, s, rgb(255, 255, 255), rgb(0, 0, 0));
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// One GPIO port's pad in io::GamePadState button bits; see pico-outrun's
// nespadGameBits() for why the pad type is read as "SNES or not".
static uint32_t nespadGameBits(uint16_t n, uint8_t type)
{
    typedef io::GamePadState::Button B;
    uint32_t b = 0;
    if (n & (1u << 2)) b |= B::SELECT;
    if (n & (1u << 3)) b |= B::START;
    if (n & (1u << 4)) b |= B::UP;
    if (n & (1u << 5)) b |= B::DOWN;
    if (n & (1u << 6)) b |= B::LEFT;
    if (n & (1u << 7)) b |= B::RIGHT;
    if (type != NESPAD_TYPE_SNES)
    {
        if (n & (1u << 0)) b |= B::A;
        if (n & (1u << 1)) b |= B::B;
    }
    else
    {
        if (n & (1u << 0)) b |= B::B;
        if (n & (1u << 1)) b |= B::Y;
        if (n & (1u << 8)) b |= B::A;
        if (n & (1u << 9)) b |= B::X;
    }
    return b;
}

// Every pad source merged into one: Moon Cresta is played by one player at a time
// (two players take turns on the same controls).
static uint32_t readPads(uint16_t wii, uint32_t *pad2)
{
    typedef io::GamePadState::Button B;
    uint32_t b = 0;
    auto &gp = io::getCurrentGamePadState(0);
    if (gp.connected)
        b |= gp.buttons;
    auto &gp2 = io::getCurrentGamePadState(1);
    *pad2 = gp2.connected ? gp2.buttons : 0;
#if NES_PIN_CLK != -1
    b |= nespadGameBits(nespad_states_ext[0], nespad_padtype[0]);
#endif
#if NES_PIN_CLK_1 != -1
    b |= nespadGameBits(nespad_states_ext[1], nespad_padtype[1]);
#endif
    if (wii & (1u << 0)) b |= B::A;
    if (wii & (1u << 1)) b |= B::B;
    if (wii & (1u << 2)) b |= B::SELECT;
    if (wii & (1u << 3)) b |= B::START;
    if (wii & (1u << 4)) b |= B::UP;
    if (wii & (1u << 5)) b |= B::DOWN;
    if (wii & (1u << 6)) b |= B::LEFT;
    if (wii & (1u << 7)) b |= B::RIGHT;
    if (wii & (1u << 8)) b |= B::X;
    if (wii & (1u << 9)) b |= B::Y;
    return b;
}

// Maps the pad onto the cabinet:
//   left/right  move            A (or X)  fire       B (or Y)  fire
//   SELECT      coin            START     1 player   UP        2 players
//   START on a second USB pad also starts a 2 player game.
//
// The cabinet has one fire button. A and B both press it, each with its own
// rapid fire setting, so one of them can fire single shots and the other
// automatically.
//
// SELECT doubles as the modifier of SELECT+START (settings menu), so the coin
// is inserted when SELECT is released, and only if no other button was pressed
// while it was held. A coin switch is a pulse; it is held for a few frames.
//
// START + A toggles the frame rate display, as in the sibling emulators. While
// START is held, A therefore does not fire.
static bool selectUsedAsModifier = false;
static uint32_t prevButtons = 0;
static int coinFrames = 0;

// Forget the buttons seen before the settings menu opened: its SELECT+START
// never reaches mapInputs(), so the SELECT release afterwards would otherwise
// insert a coin.
static void resetInputs()
{
    selectUsedAsModifier = false;
    prevButtons = 0;
    coinFrames = 0;
}

static uint8_t mapInputs(uint32_t buttons, uint32_t pad2)
{
    typedef io::GamePadState::Button B;
    static uint32_t frame = 0;
    frame++;
    const uint32_t pushed = buttons & ~prevButtons;

    if ((buttons & B::START) && (pushed & B::A))
        settings.flags.displayFrameRate = !settings.flags.displayFrameRate;

    if (buttons & B::SELECT)
    {
        if (buttons & ~(uint32_t)B::SELECT)
            selectUsedAsModifier = true;
    }
    else if (prevButtons & B::SELECT)
    {
        if (!selectUsedAsModifier)
            coinFrames = 6;
        selectUsedAsModifier = false;
    }
    prevButtons = buttons;

    uint8_t in = 0;
    if (coinFrames > 0)
    {
        coinFrames--;
        in |= MCR_IN_COIN;
    }
    if (buttons & B::SELECT)
        return in; // a modifier combination, not game input

    if (buttons & B::START)
        in |= MCR_IN_START1;
    if ((buttons & B::UP) || (pad2 & B::START))
        in |= MCR_IN_START2;
    if (buttons & B::LEFT)
        in |= MCR_IN_LEFT;
    if (buttons & B::RIGHT)
        in |= MCR_IN_RIGHT;
    // Rapid fire: 4 frames pressed, 4 released.
    const bool rapidPhase = (frame & 4) != 0;
    if ((buttons & B::X) || ((buttons & B::A) && !(buttons & B::START)))
    {
        if (!settings.flags.rapidFireOnA || rapidPhase)
            in |= MCR_IN_FIRE;
    }
    if (buttons & (B::B | B::Y))
    {
        if (!settings.flags.rapidFireOnB || rapidPhase)
            in |= MCR_IN_FIRE;
    }
    return in;
}

// ---------------------------------------------------------------------------
// Wii Classic controller (I2C).
//
// initAll() tries it once at boot, except on WIIPAD_DELAYED_START boards
// without a TLV320 DAC (Murmulator M2), where pico_shared leaves it to its
// menus, and this game never shows the ROM browser. A pad plugged in after
// boot is not picked up on any board either. So it is started here before the
// game, and retried about once a second while no pad answers. A retry without
// a pad costs about 0.3 ms; finding one stalls once for 200 ms (the pad's own
// init delays).
// ---------------------------------------------------------------------------
#if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
static void wiipadPoll()
{
    static int frames = 0;
    if (wiipad_is_connected() || ++frames < 60)
        return;
    frames = 0;
    wiipad_begin();
}
#endif

// ---------------------------------------------------------------------------
// Frame pacing.
//
// On PicoDVI with a framebuffer, pico_shared's PaceFrames60fps() busy-waits
// for core1's vsync flag. Core1 sets it after converting the last line of a
// frame and clears it again a few instructions later, at the top of its loop,
// so core0 misses it about half the time and then waits a whole extra frame:
// the game ran at about 40 fps and the audio starved. The DVI frame counter
// advances at the start of the vertical sync and cannot be missed: a frame
// that ran long simply finds it already advanced. HSTX keeps PaceFrames60fps().
// ---------------------------------------------------------------------------
static void paceFrame(bool init)
{
#if HSTX
    Frens::PaceFrames60fps(init);
#else
    static uint32_t last = 0;
    uint32_t now = dvi_->getFrameCounter();
    if (!init)
    {
        while (now == last)
        {
            __compiler_memory_barrier(); // the counter is advanced by core1
            now = dvi_->getFrameCounter();
        }
    }
    last = now;
#endif
}

// ---------------------------------------------------------------------------
// Once per frame
// ---------------------------------------------------------------------------
static void processPerFrame()
{
    paceFrame(false);

    if (haveRoms)
        presentFrame();

    Frens::pollHeadPhoneJack();

#if ENABLE_VU_METER
    // Fruit Jam Button 2 toggles the VU meter, as in the sibling emulators.
    if (isVUMeterToggleButtonPressed())
    {
        settings.flags.enableVUMeter = !settings.flags.enableVUMeter;
        FrensSettings::savesettings();
        turnOffAllLeds();
    }
#endif

    nespad_read_start();
#if HSTX
    uint32_t count = hstx_getframecounter();
#else
    uint32_t count = dvi_->getFrameCounter();
#endif
    Frens::blinkLed((count >> 5) & 1);
    nespad_read_finish();

    tuh_task();
    uint16_t wii = 0;
#if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
    wiipadPoll();
    wii = wiipad_read(); // boards without the Wii port do not link wiipad at all
#endif

    uint32_t pad2 = 0;
    uint32_t buttons = readPads(wii, &pad2);

    if ((buttons & io::GamePadState::Button::SELECT) && (buttons & io::GamePadState::Button::START))
        showSettings = true;

    if (showSettings)
    {
        showSettings = false;
        machine.inputs = 0;
        resetInputs();
        int rval = showSettingsMenu(true);

        // menu.cpp saves only down the SAVE path; `settings` only ever holds
        // committed values, so writing again can never persist a cancelled edit.
        FrensSettings::savesettings();
        scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
        EXT_AUDIO_SETVOLUME(settings.fruitjamVolumeLevel);

        // Without the ROM set there is nothing to return to. Reboot, so a set
        // just copied onto the card in USB drive mode is picked up.
        if (!haveRoms)
        {
            printf("Rebooting to look for the ROM set again...\n");
            watchdog_reboot(0, 0, 0);
            while (true)
                tight_loop_contents();
        }
        if (rval == 5) // Reset Game
            mcr_reset(&machine);
        bordersValid = false; // the menu drew over the whole screen
        paceFrame(true);
        lastFrameUs = Frens::time_us();
        return;
    }

    if (!haveRoms)
        return;

    machine.inputs = mapInputs(buttons, pad2);
    mcr_run_frame(&machine, audioBuf);
    pushAudio(audioBuf, SAMPLES_PER_FRAME);

    if (settings.flags.displayFrameRate)
    {
        uint64_t now = Frens::time_us();
        uint64_t dt = now - lastFrameUs;
        lastFrameUs = now;
        if (dt > 0)
            fps = (uint32_t)((1000000 + dt / 2) / dt);
    }
}

int main()
{
    Frens::setClocksAndStartStdio(CPUFreqKHz, VREG_VOLTAGE_1_20);

    printf("==========================================================================================\n");
    printf("picoMoonCresta %s\n", SWVERSION);
    printf("Build date: %s %s\n", __DATE__, __TIME__);
    printf("HW_CONFIG=%d  HSTX=%d  CPU freq: %lu kHz\n", HW_CONFIG, HSTX, (unsigned long)(clock_get_hz(clk_sys) / 1000));
    printf("==========================================================================================\n");

    FrensSettings::initSettings(FrensSettings::ARCADE);

    // No ROM is ever selected through the browser; the set is read below.
    char dummyRom[FF_MAX_LFN];
    dummyRom[0] = 0;
    bool sdOk = Frens::initAll(dummyRom, CPUFreqKHz, 0, 0, AUDIOBUFFERSIZE, false, true);

    if (sdOk)
    {
        // loadsettings() inside initAll resets every setting when
        // settings.currentDir does not exist. There is no ROM browser to
        // create it, so make it (FR_EXIST later on is fine) and load again.
        // ROMDIR is made too, so the user sees where the ROM set goes.
        f_mkdir("/roms");
        f_mkdir("/roms/arcade");
        f_mkdir(ROMDIR);
        FrensSettings::loadsettings();
    }
    // All arcade games share /settings_ARC.dat and with it currentDir. Keep it
    // at /roms/arcade, which every arcade game creates: a game's own folder
    // would make the next game reset its settings when that folder is missing.
    strcpy(settings.currentDir, "/roms/arcade");
    g_settings_visibility = g_settings_visibility_mooncresta;
    g_available_screen_modes = g_available_screen_modes_mooncresta;
    if (!g_available_screen_modes[static_cast<int>(settings.screenMode)])
        settings.screenMode = ScreenMode::NOSCANLINE_1_1;
    scaleMode8_7_ = Frens::applyScreenMode(settings.screenMode);
    // Apply the saved DAC volume now; otherwise it only takes effect after the
    // settings menu has been opened and closed. No-op without a TLV320.
    EXT_AUDIO_SETVOLUME(settings.fruitjamVolumeLevel);

#if WII_PIN_SDA >= 0 and WII_PIN_SCL >= 0
    // after initAll(), so after the DAC: see wiipadPoll()
    if (!wiipad_is_connected())
        wiipad_begin();
#endif

    haveRoms = sdOk && loadRoms();
    if (haveRoms)
    {
        mcr_gfx_init(&gfx, &roms, HSTX ? MCR_FMT_RGB555 : MCR_FMT_RGB444);
        mcr_init(&machine, &roms, SAMPLERATE, SAMPLES_PER_FRAME);
    }
    else
    {
        drawErrorScreen(sdOk);
    }

    paceFrame(true);
    lastFrameUs = Frens::time_us();
    while (true)
        processPerFrame();
}
