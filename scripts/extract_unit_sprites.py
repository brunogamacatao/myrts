#!/usr/bin/env python3
"""Builds sprite definitions (png + json) for units from the Warcraft II sheets of The Spriters Resource.

All unit sheets share one layout: every row is one animation frame and the 5 sprites of a row are
the directions up, up-right, right, down-right, down. The left-facing directions are mirrored at
draw time (flip_horizontal). Rows are found automatically from the transparent gaps.

Usage: scripts/extract_unit_sprites.py --dir ~/Downloads
Reads wc2_orc_grunt.png, wc2_human_footman.png, wc2_human_peasant.png; writes assets/sprites/.
"""
import argparse
import json
import pathlib
import shutil

from PIL import Image

OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "sprites"
DIRS = ["up", "up_right", "right", "down_right", "down"]

# Row indices (0-based, top to bottom, gaps between rows detect them) of each animation.
UNITS = {
    "orcs_grunt": {
        "sheet": "wc2_orc_grunt.png",
        "walk": range(0, 5),
        "attack": range(5, 9),
    },
    "human_footman": {
        "sheet": "wc2_human_footman.png",
        "walk": range(0, 5),
        "attack": range(5, 9),
    },
    "human_peasant": {
        "sheet": "wc2_human_peasant.png",
        "walk": range(0, 5),
        "chop": range(5, 10),
        "walk_gold": range(12, 17),
        "walk_wood": range(17, 22),
    },
}
FPS = {"walk": 5, "attack": 8, "chop": 6, "walk_gold": 5, "walk_wood": 5}


def bands(flags, min_width=1):
    out, start = [], None
    for i, on in enumerate(flags):
        if on and start is None:
            start = i
        elif not on and start is not None:
            out.append((start, i))
            start = None
    if start is not None:
        out.append((start, len(flags)))
    return [b for b in out if b[1] - b[0] >= min_width]


def row_columns(alpha, width, row):
    """The 5 horizontal extents of a row: tiny slivers are dropped, then the closest clusters merge."""
    cols = bands([any(alpha[x, y] > 0 for y in range(*row)) for x in range(width)], min_width=4)
    while len(cols) > 5:
        gaps = [cols[i + 1][0] - cols[i][1] for i in range(len(cols) - 1)]
        i = gaps.index(min(gaps))
        cols[i:i + 2] = [(cols[i][0], cols[i + 1][1])]
    return cols


def frame_box(alpha, col, row):
    pts = [(x, y) for x in range(*col) for y in range(*row) if alpha[x, y] > 0]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    return {"x": min(xs), "y": min(ys), "w": max(xs) - min(xs) + 1, "h": max(ys) - min(ys) + 1}


def build(name, spec, directory):
    sheet = directory / spec["sheet"]
    im = Image.open(sheet).convert("RGBA")
    width, height = im.size
    alpha = im.split()[3].load()
    rows = bands([any(alpha[x, y] > 0 for x in range(width)) for y in range(height)])
    columns = {r: row_columns(alpha, width, rows[r]) for r in {i for k, v in spec.items() if k != "sheet" for i in v}}

    def frames(kind, d):
        return [frame_box(alpha, columns[r][d], rows[r]) for r in spec[kind]]

    animations = []

    def add(label, kind, d, only_first=False):
        fr = frames(kind, d)
        variants = [(DIRS[d], False)]
        if "right" in DIRS[d]:
            variants.append((DIRS[d].replace("right", "left"), True))
        for direction, flip in variants:
            a = {"name": f"{label}_{direction}", "fps": FPS[kind], "frames": fr[:1] if only_first else fr}
            if flip:
                a["flip_horizontal"] = True
            animations.append(a)

    for d in range(5):
        add("idle", "walk", d, only_first=True)
        add("walk", "walk", d)
        if "attack" in spec:
            add("attack", "attack", d)
        if "chop" in spec:
            add("chop", "chop", d)
        if "walk_gold" in spec:
            add("walk_gold", "walk_gold", d)
            add("idle_gold", "walk_gold", d, only_first=True)
        if "walk_wood" in spec:
            add("walk_wood", "walk_wood", d)
            add("idle_wood", "walk_wood", d, only_first=True)

    OUT.mkdir(parents=True, exist_ok=True)
    png = f"{name}.png"
    shutil.copyfile(sheet, OUT / png)
    (OUT / f"{name}.json").write_text(
        json.dumps({"spritesheet": f"sprites/{png}", "default_animation": "idle_up", "animations": animations}, indent=2)
    )
    print("wrote", OUT / f"{name}.json", f"({len(animations)} animations)")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=str(pathlib.Path.home() / "Downloads"))
    args = ap.parse_args()
    for unit_name, unit_spec in UNITS.items():
        build(unit_name, unit_spec, pathlib.Path(args.dir))
