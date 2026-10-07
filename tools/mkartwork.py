#!/usr/bin/env python3
"""
mkartwork.py - build the artwork of pico-mooncresta: the screensaver image
built into the firmware and the pico-bootLoader menu tiles.

    tools/mkartwork.py                      DefaultSS444.c and DefaultSS555.c
    tools/mkartwork.py --bootloader DIR     also DIR/themes/0/mooncresta.png
                                            and DIR/themes/1/mooncresta.png
    tools/mkartwork.py --footage DIR ...    use screenshots of the game (below)
    tools/mkartwork.py --preview FILE.png   also the screensaver image as PNG

Screenshots of the game. The ROM set is not part of this project, so neither
are pictures of the game: they are taken from your own ROM set by the host
harness, then laid out by this script.

    hosttest/build.sh
    tools/capture_footage.sh ~/roms/arcade/MOONCRESTA /tmp/mcr_footage
    tools/mkartwork.py --footage /tmp/mcr_footage \
                       --bootloader ../pico-bootLoader/emu/assets

capture_footage.sh plays the game in the harness and keeps a frame every
second in DIR/frames; it copies five of them to the names below. Copy any other
frame over one of these names to change what a part of the artwork shows.
They are 320x240 PPM files with Tate mode Off (the game area is x 48-271).

    badge.ppm        theme 0: the box art in the badge (the whole game area)
    left.ppm         theme 1: left panel (left part of the game area, 1:1)
    middle.ppm       theme 1: middle panel (centre of the game area, 1:1)
    right.ppm        theme 1: right panel (right part of the game area, 1:1)
    screensaver.ppm  DefaultSS*.c (the game area, scaled to 160 rows)

A part without its file is drawn from simple shapes and pixel sprites made for
this project instead.

DefaultSS*.c is the built-in image the pico_shared menu screensaver bounces
around when no artwork is on the SD card: uint16 width, uint16 height
(little-endian), then width*height little-endian pixels, 0000RRRRGGGGBBBB
(.444) or 0RRRRRGGGGGBBBBB (.555), as an unsigned char array.

The menu tiles follow the two themes of pico-bootLoader, with the geometry of
its tools/make_category_art.py:

    theme 0   640x480: flat saturated ground, a huge ghosted silhouette (a
              three-stage rocket), a boxed badge on the left, the wordmark
              and a rule
    theme 1   320x240: three slanted panels of game footage, white wordmark
              across the middle

Run the bootloader's tools/png2raw.py on the two PNGs afterwards to build the
.444/.555 caches. Requires numpy and ffmpeg (with librsvg).
"""

import argparse
import base64
import os
import subprocess
import sys
import tempfile

import numpy as np

SS = 4  # supersampling factor for the shapes

# 5x7 block letters
FONT = {
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    " ": ["00000"] * 7,
}


def tri_mask(X, Y, p0, p1, p2):
    def edge(a, b):
        return (X - a[0]) * (b[1] - a[1]) - (Y - a[1]) * (b[0] - a[0])

    e0, e1, e2 = edge(p0, p1), edge(p1, p2), edge(p2, p0)
    return ((e0 >= 0) & (e1 >= 0) & (e2 >= 0)) | ((e0 <= 0) & (e1 <= 0) & (e2 <= 0))


def ellipse_mask(X, Y, cx, cy, rx, ry):
    return ((X - cx) / rx) ** 2 + ((Y - cy) / ry) ** 2 <= 1.0


def paint(img, mask, color):
    img[mask] = color


def draw_rocket(img, X, Y, cx, top, height):
    """An upright rocket: body, nose cone, fins, porthole and flame."""
    h = height
    w = h * 0.26                      # body width
    body_top, body_bot = top + h * 0.22, top + h * 0.78
    # flame first, so the body covers its top
    paint(img, tri_mask(X, Y, (cx - w * 0.38, body_bot), (cx + w * 0.38, body_bot), (cx, top + h * 1.08)),
          (255, 120, 20))
    paint(img, tri_mask(X, Y, (cx - w * 0.2, body_bot), (cx + w * 0.2, body_bot), (cx, top + h * 0.96)),
          (255, 230, 90))
    # fins
    paint(img, tri_mask(X, Y, (cx - w * 0.5, body_bot - h * 0.2), (cx - w * 0.5, body_bot + h * 0.04),
                        (cx - w * 1.05, body_bot + h * 0.06)), (210, 40, 50))
    paint(img, tri_mask(X, Y, (cx + w * 0.5, body_bot - h * 0.2), (cx + w * 0.5, body_bot + h * 0.04),
                        (cx + w * 1.05, body_bot + h * 0.06)), (210, 40, 50))
    # body: a rounded capsule, shaded from left to right
    body = (np.abs(X - cx) <= w / 2) & (Y >= body_top) & (Y <= body_bot)
    body |= ellipse_mask(X, Y, cx, body_top, w / 2, h * 0.08)
    shade = np.clip(1.0 - (X - (cx - w / 2)) / w * 0.45, 0.55, 1.0)
    for c, v in enumerate((235, 238, 245)):
        img[..., c] = np.where(body, v * shade, img[..., c])
    # nose cone
    paint(img, tri_mask(X, Y, (cx - w / 2, body_top), (cx + w / 2, body_top), (cx, top)), (210, 40, 50))
    paint(img, ellipse_mask(X, Y, cx, body_top, w / 2, h * 0.025) & (Y >= body_top - h * 0.025), (210, 40, 50))
    # porthole
    py = top + h * 0.42
    paint(img, ellipse_mask(X, Y, cx, py, w * 0.3, w * 0.3), (60, 70, 90))
    paint(img, ellipse_mask(X, Y, cx, py, w * 0.22, w * 0.22), (90, 170, 240))
    paint(img, ellipse_mask(X, Y, cx - w * 0.07, py - w * 0.07, w * 0.07, w * 0.07), (220, 240, 255))
    # a stripe
    paint(img, (np.abs(X - cx) <= w / 2) & (np.abs(Y - (top + h * 0.64)) <= h * 0.018), (210, 40, 50))


def draw_moon(img, X, Y, cx, cy, r):
    disc = (X - cx) ** 2 + (Y - cy) ** 2 <= r * r
    bite = (X - (cx + 0.42 * r)) ** 2 + (Y - (cy - 0.22 * r)) ** 2 <= (0.86 * r) ** 2
    crescent = disc & ~bite
    # lit from the left, a few darker craters
    light = np.clip(1.05 - ((X - (cx - r)) / (2 * r)) * 0.5, 0.6, 1.0)
    crater = (ellipse_mask(X, Y, cx - 0.55 * r, cy + 0.15 * r, 0.12 * r, 0.12 * r)
              | ellipse_mask(X, Y, cx - 0.3 * r, cy + 0.6 * r, 0.09 * r, 0.09 * r)
              | ellipse_mask(X, Y, cx - 0.62 * r, cy - 0.35 * r, 0.07 * r, 0.07 * r))
    for c, v in enumerate((250, 232, 160)):
        val = v * light * np.where(crater, 0.8, 1.0)
        img[..., c] = np.where(crescent, val, img[..., c])


def draw_text(img, text, x0, y0, scale):
    """Block letters at output resolution, yellow to orange, with a shadow."""
    rows = 7
    for pass_ in (0, 1):
        off = max(1, scale // 2) if pass_ == 0 else 0
        x = x0
        for ch in text:
            glyph = FONT[ch]
            for gy in range(rows):
                t = gy / (rows - 1)
                color = (40, 10, 30) if pass_ == 0 else (255, 235 - 115 * t, 90 - 60 * t)
                for gx in range(5):
                    if glyph[gy][gx] == "1":
                        ys, xs = y0 + gy * scale + off, x + gx * scale + off
                        img[ys:ys + scale, xs:xs + scale] = color
            x += 6 * scale


def text_width(text, scale):
    return (6 * len(text) - 1) * scale


def render(w, h, layout, seed):
    W, H = w * SS, h * SS
    Y, X = np.mgrid[0:H, 0:W].astype(np.float32)
    X = (X + 0.5) / SS
    Y = (Y + 0.5) / SS
    img = np.zeros((H, W, 3), np.float32)

    # background: deep blue at the top to black
    t = (Y / h)[..., None]
    img[:] = np.array((14, 18, 52), np.float32) * (1 - t) + np.array((0, 0, 6), np.float32) * t

    # stars: single output pixels, a few brighter
    rng = np.random.default_rng(seed)
    n = int(w * h / 260)
    palette = [(255, 255, 255), (255, 240, 180), (170, 200, 255), (255, 170, 170), (190, 255, 210)]
    for _ in range(n):
        sx, sy = rng.integers(0, w), rng.integers(0, h)
        c = np.array(palette[rng.integers(0, len(palette))], np.float32) * rng.uniform(0.35, 1.0)
        size = 2 if rng.uniform() < 0.06 and w >= 320 else 1
        img[sy * SS:(sy + size) * SS, sx * SS:(sx + size) * SS] = c

    draw_moon(img, X, Y, *layout["moon"])
    draw_rocket(img, X, Y, *layout["rocket"])

    # box-filter down to the output size
    out = img.reshape(h, SS, w, SS, 3).mean(axis=(1, 3))

    for text, ty, scale in layout["text"]:
        draw_text(out, text, (w - text_width(text, scale)) // 2, ty, scale)
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)


PORTRAIT = {  # the screensaver image
    "moon": (90, 26, 17),
    "rocket": (42, 22, 74),
    "text": [("MOON", 108, 3), ("CRESTA", 134, 3)],
}


def write_c(path, name, img, pack):
    h, w, _ = img.shape
    data = [w & 0xFF, w >> 8, h & 0xFF, h >> 8]
    for row in img:
        for r, g, b in row:
            v = pack(int(r), int(g), int(b))
            data += [v & 0xFF, v >> 8]
    with open(path, "w") as f:
        f.write("/* Generated by tools/mkartwork.py - do not edit. */\n")
        f.write("const unsigned char %s[] = {\n" % name)
        for i in range(0, len(data), 12):
            f.write("  " + ", ".join("0x%02x" % b for b in data[i:i + 12]) + ",\n")
        f.write("};\n")
        f.write("const unsigned int %s_len = %d;\n" % (name, len(data)))


def write_png(path, img):
    h, w, _ = img.shape
    with tempfile.NamedTemporaryFile(suffix=".ppm", delete=False) as f:
        f.write(b"P6\n%d %d\n255\n" % (w, h))
        f.write(img.tobytes())
        ppm = f.name
    try:
        subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", ppm, path], check=True)
    finally:
        os.unlink(ppm)


# ---------------------------------------------------------------------------
# pico-bootLoader menu tiles
# ---------------------------------------------------------------------------

# Pixel sprites. Each character is one pixel; '.' is transparent.
SPRITE_COLORS = {
    "R": (214, 48, 49), "W": (236, 238, 242), "w": (178, 184, 196), "B": (52, 120, 210),
    "C": (190, 230, 255), "G": (110, 116, 128), "O": (255, 128, 24), "Y": (255, 226, 90),
    "S": (200, 206, 214), "D": (140, 150, 166), "L": (255, 214, 64), "K": (60, 64, 74),
}

ROCKET = [
    ".....R.....",
    "....RRR....",
    "....RRR....",
    "...RRRRR...",
    "...WWWWW...",
    "..WWWWWWw..",
    "..WWBBBWw..",
    "..WBBCBBw..",
    "..WWBBBWw..",
    "..WWWWWWw..",
    "..WWWWWWw..",
    "..RRRRRRR..",
    "..WWWWWWw..",
    "..WWWWWWw..",
    ".RWWWWWWwR.",
    "RRWWWWWWwRR",
    "RR.WWWWw.RR",
    "R..GGGGG..R",
    "....OYO....",
    "....OYO....",
    ".....O.....",
]

# The two halves of the docking scene: an upper stage coming down onto a
# lower one, a little apart.
UPPER_STAGE = [
    "...RRR...",
    "..RRRRR..",
    ".RRRRRRR.",
    ".WWWWWWw.",
    ".WWBBWWw.",
    ".WBCBBWw.",
    ".WWBBWWw.",
    "RWWWWWWwR",
    "RRGGGGGRR",
]
LOWER_STAGE = [
    "...SSSSS...",
    "..WWWWWWw..",
    "..WWWWWWw..",
    "..RRRRRRR..",
    "..WWWWWWw..",
    "..WWWWWWw..",
    ".RWWWWWWwR.",
    "RRWWWWWWwRR",
    "RRRWWWWwRRR",
    "R..GGGGG..R",
    "...OOYOO...",
    "....OYO....",
    ".....O.....",
]

SAUCER = [
    "....CCCCC....",
    "...CCCCCCC...",
    ".DDDDDDDDDDD.",
    "DDLDDLDDLDDLD",
    ".KKKKKKKKKKK.",
    "...K.....K...",
]


def blit(img, sprite, x0, y0, scale=1):
    for y, row in enumerate(sprite):
        for x, ch in enumerate(row):
            if ch != ".":
                ys, xs = y0 + y * scale, x0 + x * scale
                img[max(ys, 0):max(ys + scale, 0), max(xs, 0):max(xs + scale, 0)] = SPRITE_COLORS[ch]


def starfield(img, density, seed, dim=1.0):
    rng = np.random.default_rng(seed)
    h, w, _ = img.shape
    palette = [(255, 255, 255), (255, 240, 180), (170, 200, 255), (255, 170, 170), (190, 255, 210)]
    for _ in range(int(w * h * density)):
        x, y = rng.integers(0, w), rng.integers(0, h)
        c = np.array(palette[rng.integers(0, len(palette))], np.float32) * rng.uniform(0.35, 1.0) * dim
        img[y, x] = c


def pixel_moon(img, cx, cy, r, color=(250, 232, 160)):
    h, w, _ = img.shape
    yy, xx = np.mgrid[0:h, 0:w]
    disc = (xx - cx) ** 2 + (yy - cy) ** 2 <= r * r
    bite = (xx - (cx + 0.45 * r)) ** 2 + (yy - (cy - 0.2 * r)) ** 2 <= (0.85 * r) ** 2
    m = disc & ~bite
    shade = np.clip(1.05 - (xx - (cx - r)) / (2 * r) * 0.45, 0.6, 1.0)
    for c in range(3):
        img[..., c] = np.where(m, color[c] * shade, img[..., c])


def lunar_surface(img, top, seed):
    h, w, _ = img.shape
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:h, 0:w]
    ridge = top + 4 * np.sin(xx / 9.0) + 3 * np.sin(xx / 4.3 + 1.0)
    ground = yy >= ridge
    base = np.array((120, 124, 134), np.float32)
    for c in range(3):
        img[..., c] = np.where(ground, base[c] * (1.0 - (yy - top) / (h - top + 1) * 0.35), img[..., c])
    for _ in range(14):
        cx, cy = rng.integers(0, w), rng.integers(top + 8, h)
        rx, ry = rng.integers(4, 11), rng.integers(2, 4)
        crater = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2 <= 1
        rim = ((xx - cx) / (rx + 1)) ** 2 + ((yy - cy - 1) / (ry + 1)) ** 2 <= 1
        img[rim & ground & ~crater] = (160, 164, 174)
        img[crater & ground] = (84, 88, 98)


def png_data_uri(img):
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "a.png")
        write_png(path, img)
        with open(path, "rb") as f:
            return "data:image/png;base64," + base64.b64encode(f.read()).decode("ascii")


def rasterise(svg, out_png):
    with tempfile.TemporaryDirectory() as d:
        path = os.path.join(d, "tile.svg")
        with open(path, "w") as f:
            f.write(svg)
        subprocess.run(["ffmpeg", "-y", "-v", "error", "-i", path, "-frames:v", "1", "-update", "1",
                        "-pix_fmt", "rgb24", "-compression_level", "9", out_png], check=True)


def decode_png(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                         check=True, capture_output=True).stdout
    probe = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                            "stream=width,height", "-of", "csv=p=0", path],
                           check=True, capture_output=True, text=True).stdout.strip().split(",")
    w, h = int(probe[0]), int(probe[1])
    return np.frombuffer(raw, np.uint8).reshape(h, w, 3)


# Screenshots from the host harness: 320x240, Tate mode Off
GAME_X, GAME_W, GAME_H = 48, 224, 240


def load_footage(footage, name):
    """The 320x240 frame footage/name.ppm, or None without --footage or file."""
    if not footage:
        return None
    path = os.path.join(footage, name + ".ppm")
    if not os.path.exists(path):
        print("  %s.ppm not found, drawing that part instead" % name)
        return None
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    if (w, h) != (320, 240):
        sys.exit("%s: expected a 320x240 frame from hosttest/mcr_host" % path)
    return np.frombuffer(parts[3][:w * h * 3], np.uint8).reshape(h, w, 3)


def box_scale(img, out_w, out_h):
    """Area-average img down to out_w x out_h."""
    h, w, _ = img.shape
    out = np.zeros((out_h, out_w, 3), np.float64)
    src = img.astype(np.float64)
    for oy in range(out_h):
        y0, y1 = oy * h / out_h, (oy + 1) * h / out_h
        for ox in range(out_w):
            x0, x1 = ox * w / out_w, (ox + 1) * w / out_w
            acc, area = np.zeros(3), 0.0
            for sy in range(int(y0), min(int(np.ceil(y1)), h)):
                wy = min(sy + 1, y1) - max(sy, y0)
                for sx in range(int(x0), min(int(np.ceil(x1)), w)):
                    wx = min(sx + 1, x1) - max(sx, x0)
                    acc += src[sy, sx] * wx * wy
                    area += wx * wy
            out[oy, ox] = acc / area
    return np.clip(out + 0.5, 0, 255).astype(np.uint8)


# Theme 0 geometry, from pico-bootLoader's tools/make_category_art.py
T0_W, T0_H = 640, 480
T0_GROUND, T0_TINT = "#d29c1b", "#e9b33a"
BADGE_X, BADGE_Y, BADGE_W, BADGE_H = 2, 186, 90, 108
BADGE_FRAME = 8
BADGE_GROUND = "#1a1b2a"
RULE_Y, RULE_TH = 229, 2
DASH_X0, DASH_X1 = 94, 108
TEXT_X, TEXT_BASELINE, TEXT_SIZE = 114, 246, 45
WORDMARK = "MOON CRESTA"

# The ghosted silhouette: a three-stage rocket in a 100x100 box. `knock` is
# punched back out of it in the ground colour, so the stages stay legible.
ROCKET_HULL = (
    '<path d="M50 2 L60 18 L60 30 L40 30 L40 18 Z"/>'          # capsule and nose
    '<rect x="38" y="32" width="24" height="22"/>'               # second stage
    '<path d="M38 40 L30 54 L38 54 Z M62 40 L70 54 L62 54 Z"/>'  # its fins
    '<rect x="35" y="56" width="30" height="30"/>'               # first stage
    '<path d="M35 62 L20 92 L35 86 Z M65 62 L80 92 L65 86 Z"/>'  # big fins
    '<path d="M42 86 L58 86 L55 93 L45 93 Z"/>'                  # nozzle
)
ROCKET_KNOCK = (
    '<circle cx="50" cy="22" r="3.6"/>'
    '<circle cx="50" cy="42" r="3.2"/>'
    '<circle cx="50" cy="66" r="3.8"/>'
    '<rect x="38" y="30" width="24" height="2"/>'
    '<rect x="35" y="54" width="30" height="2"/>'
    '<rect x="35" y="76" width="30" height="2.5"/>'
)
WATERMARK = (560, 330, 300, -14)  # centre x, centre y, box size, rotation


def badge_art(w, h):
    """The badge's 'box art': a rocket under a crescent moon, in pixel art."""
    art = np.zeros((h, w, 3), np.float32)
    t = np.linspace(0, 1, h)[:, None]
    art[:] = (np.array((20, 26, 70)) * (1 - t) + np.array((4, 4, 14)) * t)[:, :, None].transpose(0, 2, 1)
    starfield(art, 0.03, seed=5)
    pixel_moon(art, w * 0.72, h * 0.2, w * 0.17)
    blit(art, ROCKET, w // 2 - 11, h - 44, scale=2)
    return np.clip(art + 0.5, 0, 255).astype(np.uint8)


def theme0_svg(rule_x0, badge_frame=None):
    cx, cy, size, rot = WATERMARK
    s = size / 100.0
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
        f'width="{T0_W}" height="{T0_H}">',
        f'<rect width="{T0_W}" height="{T0_H}" fill="{T0_GROUND}"/>',
        f'<g transform="rotate({rot} {cx} {cy}) translate({cx - size / 2},{cy - size / 2}) scale({s:.5f})">'
        f'<g fill="{T0_TINT}">{ROCKET_HULL}</g><g fill="{T0_GROUND}">{ROCKET_KNOCK}</g></g>',
    ]
    if rule_x0 is None:
        # measuring pass: the wordmark alone
        parts.append(
            f'<rect width="{T0_W}" height="{T0_H}" fill="#000"/>'
            f'<text x="{TEXT_X}" y="{TEXT_BASELINE}" font-family="Liberation Sans" font-weight="bold" '
            f'font-size="{TEXT_SIZE}" letter-spacing="2" fill="#ffffff">{WORDMARK}</text></svg>')
        return "".join(parts)

    ix, iy = BADGE_X + BADGE_FRAME, BADGE_Y + BADGE_FRAME
    iw, ih = BADGE_W - 2 * BADGE_FRAME, BADGE_H - 2 * BADGE_FRAME
    art_h = ih - 18
    if badge_frame is not None:
        # the whole game area as box art, filling the plate above the footer
        game = badge_frame[:, GAME_X:GAME_X + GAME_W]
        art = (f'<svg x="{ix}" y="{iy}" width="{iw}" height="{art_h}" viewBox="0 0 {GAME_W} {GAME_H}" '
               f'preserveAspectRatio="xMidYMid slice"><image xlink:href="{png_data_uri(game)}" '
               f'width="{GAME_W}" height="{GAME_H}"/></svg>')
    else:
        art = (f'<image xlink:href="{png_data_uri(badge_art(iw, art_h))}" x="{ix}" y="{iy}" width="{iw}" '
               f'height="{art_h}" image-rendering="pixelated"/>')
    parts.append(
        f'<rect x="{BADGE_X}" y="{BADGE_Y}" width="{BADGE_W}" height="{BADGE_H}" fill="#ffffff"/>'
        f'<rect x="{ix}" y="{iy}" width="{iw}" height="{ih}" fill="{BADGE_GROUND}"/>'
        + art +
        f'<rect x="{ix + 3}" y="{iy + ih - 15}" width="{iw - 6}" height="11" fill="{T0_GROUND}"/>')
    parts.append(
        f'<rect x="{DASH_X0}" y="{RULE_Y}" width="{DASH_X1 - DASH_X0}" height="{RULE_TH}" fill="#ffffff"/>'
        f'<text x="{TEXT_X}" y="{TEXT_BASELINE}" font-family="Liberation Sans" font-weight="bold" '
        f'font-size="{TEXT_SIZE}" letter-spacing="2" fill="#ffffff">{WORDMARK}</text>'
        f'<rect x="{rule_x0}" y="{RULE_Y}" width="{637 - rule_x0}" height="{RULE_TH}" fill="#ffffff"/>')
    parts.append("</svg>")
    return "".join(parts)


def theme0_tile(out_png, footage=None):
    # measure the wordmark's ink, then start the rule 12px after it
    with tempfile.TemporaryDirectory() as d:
        probe = os.path.join(d, "m.png")
        rasterise(theme0_svg(None), probe)
        ink = (decode_png(probe) > 128).any(axis=2)
        cols = np.where(ink.any(axis=0))[0]
        rule_x0 = int(cols.max()) + 12
    rasterise(theme0_svg(rule_x0, load_footage(footage, "badge")), out_png)


# Theme 1 geometry: panels 8.5 degrees off vertical, 2px black seams
T1_W, T1_H = 320, 240
PANEL_CLIP = {
    "L": "0,0 120,0 84,240 0,240",
    "M": "120,0 236,0 200,240 84,240",
    "R": "236,0 320,0 320,240 200,240",
}
T1_TEXT_Y, T1_TEXT_SIZE, T1_STROKE = 114, 33, "#101820"


def t1_panels():
    # left: dark space, a rocket climbing
    left = np.zeros((T1_H, T1_W, 3), np.float32)
    starfield(left, 0.02, seed=21, dim=0.8)
    blit(left, ROCKET, 26, 128, scale=3)
    pixel_moon(left, 40, 40, 18, color=(200, 186, 130))

    # middle, the lit one: blue sky, the docking of two stages, the lunar surface
    mid = np.zeros((T1_H, T1_W, 3), np.float32)
    t = np.linspace(0, 1, T1_H)[:, None, None]
    mid[:] = np.array((16, 52, 132)) * (1 - t) + np.array((44, 92, 170)) * t
    starfield(mid, 0.015, seed=22)
    lunar_surface(mid, 196, seed=23)
    # below the wordmark band, as the bootloader's own tiles keep it clear
    blit(mid, UPPER_STAGE, 151, 134, scale=2)
    blit(mid, LOWER_STAGE, 149, 158, scale=2)
    # thruster puffs either side of the gap
    for x, y in ((142, 150), (174, 150)):
        mid[y:y + 2, x:x + 2] = (255, 255, 255)
    pixel_moon(mid, 186, 34, 14, color=(240, 226, 170))

    # right: dark space, flying saucers
    right = np.zeros((T1_H, T1_W, 3), np.float32)
    starfield(right, 0.02, seed=24, dim=0.8)
    blit(right, SAUCER, 236, 30, scale=3)
    blit(right, SAUCER, 268, 150, scale=2)
    blit(right, SAUCER, 222, 196, scale=2)
    return [np.clip(p + 0.5, 0, 255).astype(np.uint8) for p in (left, mid, right)]


# Where each footage frame goes in the panel boxes (L 0-120, M 84-236,
# R 200-320) at 1:1: x offset of the 320x240 frame, so that the left, centre
# and right columns of its game area fill the three panels.
FOOTAGE_X = {"L": -GAME_X, "M": 0, "R": GAME_X}


def theme1_tile(out_png, footage=None):
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
        f'width="{T1_W}" height="{T1_H}"><defs>',
    ]
    for slot, pts in PANEL_CLIP.items():
        parts.append(f'<clipPath id="p{slot}"><polygon points="{pts}"/></clipPath>')
    parts.append(f'</defs><rect width="{T1_W}" height="{T1_H}" fill="#000"/>')
    drawn = t1_panels()
    for slot, name, img in zip("LMR", ("left", "middle", "right"), drawn):
        x = 0
        frame = load_footage(footage, name)
        if frame is not None:
            img, x = frame, FOOTAGE_X[slot]
        parts.append(f'<g clip-path="url(#p{slot})"><image xlink:href="{png_data_uri(img)}" x="{x}" y="0" '
                     f'width="{T1_W}" height="{T1_H}" image-rendering="pixelated"/></g>')
    parts.append('<g stroke="#000" stroke-width="2" fill="none"><path d="M120 0L84 240M236 0L200 240"/></g>')
    parts.append(
        f'<text x="{T1_W // 2}" y="{T1_TEXT_Y}" text-anchor="middle" font-family="Liberation Sans Narrow" '
        f'font-weight="bold" font-style="italic" font-size="{T1_TEXT_SIZE}" letter-spacing="1" '
        f'fill="#ffffff" stroke="{T1_STROKE}" stroke-width="3" paint-order="stroke fill">{WORDMARK}</text>')
    parts.append("</svg>")
    rasterise("".join(parts), out_png)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bootloader", metavar="DIR", help="pico-bootLoader emu/assets directory")
    ap.add_argument("--footage", metavar="DIR", help="screenshots from tools/capture_footage.sh")
    ap.add_argument("--preview", metavar="FILE", help="also write the screensaver image as PNG")
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    frame = load_footage(args.footage, "screensaver")
    if frame is not None:
        out_w = round(GAME_W * 160 / GAME_H)
        ss = box_scale(frame[:, GAME_X:GAME_X + GAME_W], out_w, 160)
    else:
        ss = render(120, 160, PORTRAIT, seed=3)
    write_c(os.path.join(root, "DefaultSS444.c"), "DefaultSS160_444", ss,
            lambda r, g, b: ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4))
    write_c(os.path.join(root, "DefaultSS555.c"), "DefaultSS160_555", ss,
            lambda r, g, b: ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3))
    print("wrote DefaultSS444.c and DefaultSS555.c (%dx%d)" % (ss.shape[1], ss.shape[0]))
    if args.preview:
        write_png(args.preview, ss)
        print("wrote", args.preview)

    if args.bootloader:
        for theme, make in (("0", theme0_tile), ("1", theme1_tile)):
            path = os.path.join(args.bootloader, "themes", theme, "mooncresta.png")
            make(path, args.footage)
            print("wrote", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
