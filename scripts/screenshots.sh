#!/usr/bin/env bash
# Takes the pictures of docs/images/ on a virtual display (Xvfb), never on your own screen.
# Usage: scripts/screenshots.sh      (needs xvfb, xdotool, imagemagick; builds the release preset first)
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
"$ROOT/scripts/build.sh" release >/dev/null
OUT="$ROOT/docs/images"
mkdir -p "$OUT"
export DISPLAY=:97
Xvfb :97 -screen 0 1600x1000x24 >/dev/null 2>&1 &
XVFB=$!
trap 'kill $XVFB 2>/dev/null; pkill -x myrts 2>/dev/null; pkill -x myrts_engine 2>/dev/null' EXIT
sleep 1
WORK="$(mktemp -d)"
cp -r "$ROOT/games/warcraft-like" "$WORK/game"   # the editor works on a copy: nothing in the repository changes
cd "$ROOT/build/release"
export MYRTS_MUTE=1

click() { xdotool mousemove "$1" "$2"; sleep .35; xdotool mousedown 1; sleep .1; xdotool mouseup 1; sleep .6; }
shot() { import -window root "$WORK/full.png"; convert "$WORK/full.png" -crop "$2" +repage "$OUT/$1"; }
fresh() { pkill -x "$1" 2>/dev/null; sleep .5; }

# ---- the game (800x600 window at the top-left)
for level in 1 7; do
  ./myrts --game "$WORK/game" --level "$level" --faction orcs >/dev/null 2>&1 &
  sleep 5
  shot "game-level$level.png" 800x600+0+0
  fresh myrts
done

# ---- the editor (1400x860 window)
rm -f myrts_engine.ini
./myrts_engine "$WORK/game" >/dev/null 2>&1 &
sleep 3
EDITOR=1400x860+0+0
click 80 148;  shot editor-level.png $EDITOR            # a level of the campaign
click 437 56;  click 370 366; shot editor-characters.png $EDITOR
click 586 56;  click 370 366; shot editor-sprites.png $EDITOR
click 379 56;  click 480 80; sleep 1                    # the map of the active level
click 567 130; xdotool mousemove 600 330; sleep .3; xdotool mousedown 1
for x in 630 670 710 750; do xdotool mousemove $x 340; sleep .1; done; xdotool mouseup 1; sleep .5
shot editor-map.png $EDITOR
fresh myrts_engine
rm -f myrts_engine.ini
rm -rf "$WORK"
echo "pictures in $OUT"
