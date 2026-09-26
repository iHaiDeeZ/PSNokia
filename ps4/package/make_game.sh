#!/bin/bash
# Builds a PS4 package for one MIDP game; each game is its own app, so they
# install side by side. Build PSNokia first (ps4/build_all.sh).
#
# Usage: ps4/package/make_game.sh <jar> <title> <title-id> [WxH] [content-label]
#   title-id:      4 letters + 5 digits, unique per game, e.g. PSNK00004
#   WxH:           the phone screen the game was made for, e.g. 128x128
#                  (default: portrait 240x320); "-" for the default
#   content-label: 16 characters A-Z/0-9 (default: made from the title)
#
# The package lands in $PSNOKIA_OUT/games/<title-id>/.
set -e
. "$(dirname "$0")/../env.sh"
JAR=$1 TITLE=$2 TID=$3 SCREEN=$4 LABEL=$5
if [ -z "$JAR" ] || [ -z "$TITLE" ] || [ -z "$TID" ]; then
    sed -n '2,13p' "$0"
    exit 1
fi
if [ -z "$LABEL" ]; then
    LABEL=$(printf 'PSNOKIA%s000000000000000' "$(printf '%s' "$TITLE" | tr -cd 'A-Za-z0-9' | tr 'a-z' 'A-Z')" | cut -c1-16)
fi
D="$PSNOKIA_OUT/games/$TID"
rm -rf "$D"
mkdir -p "$D/sce_sys"
cp "$PSNOKIA_PS4/package/Makefile" "$D/"
cp "$JAR" "$D/game.jar"
if [ -n "$SCREEN" ] && [ "$SCREEN" != "-" ]; then
    printf '%s' "$SCREEN" > "$D/screen.txt"
fi
# Icon: the MIDlet's own icon on a 512x512 tile (needs Python with Pillow;
# set PSNOKIA_PYTHON to pick the interpreter),
# else the default PSNokia icon
"${PSNOKIA_PYTHON:-python3}" "$(cygpath -m "$PSNOKIA_PS4/package/make_icon.py")" "$(cygpath -m "$D/game.jar")" \
    "$(cygpath -m "$D/sce_sys/icon0.png")" "$TITLE" 2>/dev/null ||
    cp "$PSNOKIA_PS4/package/icon0.png" "$D/sce_sys/icon0.png"
# The same icon for the PSNokia menu, as raw pixels
mkdir -p "$D/assets"
"${PSNOKIA_PYTHON:-python3}" "$(cygpath -m "$PSNOKIA_PS4/package/make_raw_icon.py")" \
    "$(cygpath -m "$D/sce_sys/icon0.png")" "$(cygpath -m "$D/assets/icon.raw")" 2>/dev/null ||
    rm -f "$D/assets/icon.raw"
cd "$D"
make TITLE="$TITLE" TITLE_ID="$TID" CONTENT_ID="IV0000-${TID}_00-$LABEL" | tail -1
ls -l "$D"/*.pkg
