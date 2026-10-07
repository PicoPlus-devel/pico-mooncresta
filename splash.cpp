#include "menu.h"
#include "FrensHelpers.h"
#include <cstring>

// Called by menu.cpp. picoMoonCresta has no ROM browser, so this is only reached
// from the in-game settings menu path, but the symbol must exist either way.
//
// Note: putText collapses runs of whitespace into one cell, and renders '_' as
// a literal space - hence the underscores in the credit lines.
static int fgcolorSplash = DEFAULT_FGCOLOR;
static int bgcolorSplash = DEFAULT_BGCOLOR;

void splash()
{
    char s[SCREEN_COLS + 1];
    ClearScreen(bgcolorSplash);

    strcpy(s, "Pico-MoonCresta");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 2, s, fgcolorSplash, bgcolorSplash);

    strcpy(s, "Moon Cresta arcade emulator");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 4, s, fgcolorSplash, bgcolorSplash);

    strcpy(s, "for RP2350");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 5, s, fgcolorSplash, bgcolorSplash);

    strcpy(s, "Based on the MAME Galaxian driver");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 7, s, fgcolorSplash, bgcolorSplash);
    strcpy(s, "A. Giles, Couriersud, J. Buchmueller");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 8, s, CBLUE, bgcolorSplash);

    strcpy(s, "Pico Port");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 10, s, fgcolorSplash, bgcolorSplash);
    strcpy(s, "@frenskefrens");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 11, s, CBLUE, bgcolorSplash);

#if !HSTX
    strcpy(s, "DVI Support");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 14, s, fgcolorSplash, bgcolorSplash);
    strcpy(s, "@shuichi_takano");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 15, s, CBLUE, bgcolorSplash);
#else
    strcpy(s, "HSTX video driver _ I2S audio___");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 14, s, fgcolorSplash, bgcolorSplash);
    strcpy(s, "__@fliperama86____@frenskefrens__");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 15, s, CBLUE, bgcolorSplash);
#endif

    strcpy(s, "(S)NES/WII controller support");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 18, s, fgcolorSplash, bgcolorSplash);
    strcpy(s, "@PaintYourDragon @adafruit");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 19, s, CBLUE, bgcolorSplash);

    strcpy(s, "Moon Cresta (c) 1980 Nichibutsu.");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 22, s, fgcolorSplash, bgcolorSplash);
    strcpy(s, "This project is not affiliated with it.");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 23, s, fgcolorSplash, bgcolorSplash);

    strcpy(s, "https://github.com/");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 26, s, CBLUE, bgcolorSplash);
    strcpy(s, "PicoPlus-devel/pico-mooncresta");
    putText(SCREEN_COLS / 2 - strlen(s) / 2, 27, s, CBLUE, bgcolorSplash);
}
