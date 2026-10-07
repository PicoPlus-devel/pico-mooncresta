#!/usr/bin/env bash
# Build the host harnesses:
#   hosttest/mcr_host  - the Moon Cresta core (machine, video, sound, ROM loader)
#   hosttest/zex_host  - the Z80 core on the ZEXDOC/ZEXALL instruction tests
#
# The core in mooncresta/ has no Pico dependencies; this compiles it for Linux
# with ASan. Drop -fsanitize=address (and use -O2) for timing measurements:
#   OPT="-O2" hosttest/build.sh
set -euo pipefail
cd "$(dirname "$0")/.."

OPT=${OPT:--O1 -g -fsanitize=address -fno-omit-frame-pointer}
CFLAGS=(
  $OPT -std=c11 -Wall -Wextra -Wno-unused-parameter
  -D_DEFAULT_SOURCE
  -I mooncresta -I third_party/miniz
  -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_DEFLATE_APIS
  -DMINIZ_NO_ZLIB_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
)

CORE=(
  mooncresta/z80.c
  mooncresta/mooncresta.c
  mooncresta/mooncresta_sound.c
  mooncresta/romload.c
  third_party/miniz/miniz.c
)

gcc "${CFLAGS[@]}" hosttest/mcr_host.c "${CORE[@]}" -lm -o hosttest/mcr_host
gcc "${CFLAGS[@]}" hosttest/zex_host.c mooncresta/z80.c -o hosttest/zex_host
echo "built hosttest/mcr_host and hosttest/zex_host"
