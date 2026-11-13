#!/usr/bin/env python3
"""Collects the Warcraft II interface art used by the HUD: command/unit/building icons, resource icons, panel
textures, mouse cursors and the gold mines.

Needs (see NOTICE.md for where they come from):
  --editor   a checkout of jcfieldsdev/warcraft2-map-editor (the 46x38 icon bank, resource icons, mine sprites)
  --pud      the `pud` tool built from war2/war2tools, and --maindat its data/maindat.war (panel textures, cursors)
  --misc     the "Miscellaneous" sheet of The Spriters Resource (the lit "mine in use" frames)

    scripts/extract_ui_assets.py --editor ~/src/warcraft2-map-editor --pud build/pud/pud \\
        --maindat war2tools/data/maindat.war --misc ~/Downloads/wc2_miscellaneous.png

Writes assets/ui/*, assets/cursors/* and assets/buildings/gold_mine_*.png.
"""
import argparse
import json
import os
import pathlib
import re
import subprocess
import tempfile

from PIL import Image

ASSETS = pathlib.Path(__file__).resolve().parent.parent / "assets"
ICON_COLS = 15
ICON_W, ICON_H = 46, 38

# UI entries of maindat.war (orc versions): top bar, minimap frame, info panel, and the top/bottom window frames.
UI_ENTRIES = {294: "topbar", 296: "minimap_frame", 298: "panel", 288: "frame_top", 292: "frame_bottom"}
# Cursors worth shipping (entry numbers of libwar2).
CURSORS = {302: "pointer", 303: "pointer_no", 306: "target", 308: "target_enemy", 310: "target_ok", 311: "magnifier",
           313: "wait", 315: "scroll_n", 316: "scroll_ne", 317: "scroll_e", 318: "scroll_se", 319: "scroll_s",
           320: "scroll_sw", 321: "scroll_w", 322: "scroll_nw"}
ERAS = ["forest", "winter", "wasteland", "swamp"]
# Where the lit ("a worker is inside") mine of each era sits in the Spriters Resource "Miscellaneous" sheet.
LIT_MINE = {"forest": (13, 108, 96, 89), "winter": (112, 105, 96, 94), "wasteland": (212, 109, 96, 88)}


def icons(editor: pathlib.Path) -> None:
    src = editor / "www" / "icons" / "forest"
    files = sorted(src.glob("*.png"))
    rows = (len(files) + ICON_COLS - 1) // ICON_COLS
    atlas = Image.new("RGBA", (ICON_COLS * ICON_W, rows * ICON_H), (0, 0, 0, 0))
    for i, f in enumerate(files):
        atlas.paste(Image.open(f).convert("RGBA"), ((i % ICON_COLS) * ICON_W, (i // ICON_COLS) * ICON_H))
    out = ASSETS / "ui"
    out.mkdir(parents=True, exist_ok=True)
    atlas.save(out / "icons.png")
    for name in ("gold", "lumber", "oil", "food"):
        Image.open(editor / "www" / "icons" / "resources" / f"{name}.png").convert("RGBA").save(out / f"{name}.png")
    print(f"icons: {len(files)} in a {ICON_COLS}-column atlas")


def war2_assets(pud: pathlib.Path, maindat: pathlib.Path) -> None:
    env = dict(os.environ, LD_LIBRARY_PATH=f"{pud.parent.parent / 'libpud'}:{pud.parent.parent / 'libwar2'}")
    out_ui, out_cur = ASSETS / "ui", ASSETS / "cursors"
    out_ui.mkdir(parents=True, exist_ok=True)
    out_cur.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as tmp:
        for entry, name in UI_ENTRIES.items():
            subprocess.run([str(pud), "--war", str(maindat), "--ui", str(entry), "--png", "-o", f"{tmp}/ui"],
                           check=True, env=env, capture_output=True)
            Image.open(f"{tmp}/ui_{entry}.png").convert("RGBA").save(out_ui / f"{name}.png")

        done = subprocess.run([str(pud), "--war", str(maindat), "--cursor", "--png", "-o", f"{tmp}/cur"], check=True,
                              env=env, capture_output=True, text=True).stdout
        # The tool prints "hotx: X, hoty: Y" then "Saving image at 'cur_NNN.png'" for every cursor.
        hot = {}
        pending = None
        for line in done.splitlines():
            if m := re.match(r"hotx: (-?\d+), hoty: (-?\d+)", line):
                pending = (int(m[1]), int(m[2]))
            elif m := re.search(r"cur_(\d+)\.png", line):
                hot[int(m[1])] = pending
        meta = {}
        for entry, name in CURSORS.items():
            Image.open(f"{tmp}/cur_{entry}.png").convert("RGBA").save(out_cur / f"{name}.png")
            meta[name] = {"hot_x": hot[entry][0], "hot_y": hot[entry][1]}
        (out_cur / "cursors.json").write_text(json.dumps(meta, indent=2))
    print("ui panels and", len(CURSORS), "cursors")


def mines(editor: pathlib.Path, misc: pathlib.Path) -> None:
    sheet = Image.open(misc).convert("RGBA")
    out = ASSETS / "buildings"
    out.mkdir(parents=True, exist_ok=True)
    for era in ERAS:
        idle = Image.open(editor / "www" / "units" / era / "0092.png").convert("RGBA")
        idle.save(out / f"gold_mine_{era}.png")
        active = idle
        if era in LIT_MINE:
            x, y, w, h = LIT_MINE[era]
            lit = sheet.crop((x, y, x + w, y + h))
            left, top, right, bottom = idle.getbbox()
            active = Image.new("RGBA", idle.size, (0, 0, 0, 0))
            active.alpha_composite(lit, (left + (right - left - w) // 2, bottom - h))  # sits where the idle mine sits
        active.save(out / f"gold_mine_active_{era}.png")
    print("gold mines for", ", ".join(ERAS))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--editor", required=True)
    ap.add_argument("--pud")
    ap.add_argument("--maindat")
    ap.add_argument("--misc")
    a = ap.parse_args()
    editor = pathlib.Path(a.editor)
    icons(editor)
    if a.pud and a.maindat:
        war2_assets(pathlib.Path(a.pud), pathlib.Path(a.maindat))
    if a.misc:
        mines(editor, pathlib.Path(a.misc))
