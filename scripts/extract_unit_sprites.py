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
# "fall" is the row with one falling sprite per direction and "corpse" the row with the body left behind;
# together they make the one-shot death animation.
UNITS = {
    "orcs_peon": {
        "sheet": "orcs_peon.png",
        "in_assets": True,  # the sheet already lives in assets/sprites
        "walk": range(0, 5),
        "chop": range(5, 10),
        "fall": 10,
        "corpse": 11,
        "walk_gold": range(12, 17),
        "walk_wood": range(17, 22),
    },
    "orcs_grunt": {
        "sheet": "wc2_orc_grunt.png",
        "walk": range(0, 5),
        "attack": range(5, 9),
        "fall": 9,
        "corpse": 10,
    },
    "human_footman": {
        "sheet": "wc2_human_footman.png",
        "walk": range(0, 5),
        "attack": range(5, 9),
        "fall": 9,
        "corpse": 10,
    },
    "human_peasant": {
        "sheet": "wc2_human_peasant.png",
        "walk": range(0, 5),
        "chop": range(5, 10),
        "fall": 10,
        "corpse": 11,
        "walk_gold": range(12, 17),
        "walk_wood": range(17, 22),
    },
}
# New sheets. "death_sequence" is a row whose sprites are the frames of one death animation shared by every
# direction (mages, death knights, dragons, gryphons). Vehicles and flyers only have movement frames.
UNITS.update({
    "orcs_troll_axethrower": {"sheet": "wc2_orc_troll_axethrower.png", "walk": range(0, 5), "attack": range(5, 9), "fall": 9, "corpse": 10},
    "orcs_ogre": {"sheet": "wc2_orc_ogre.png", "walk": range(0, 5), "attack": range(5, 9), "fall": 10, "corpse": 11},
    "orcs_death_knight": {"sheet": "wc2_orc_death_knight.png", "walk": range(0, 5), "attack": range(5, 9), "death_sequence": 9},
    "orcs_dragon": {"sheet": "wc2_orc_dragon.png", "walk": range(0, 4), "attack": range(4, 5), "death_sequence": 5},
    "orcs_zeppelin": {"sheet": "wc2_orc_goblin_zeppelin.png", "walk": range(0, 2)},
    "orcs_catapult": {"sheet": "wc2_orc_catapult.png", "walk": range(0, 4)},
    "human_archer": {"sheet": "wc2_human_elven_archer.png", "walk": range(0, 5), "attack": range(5, 7), "fall": 7, "corpse": 8},
    "human_knight": {"sheet": "wc2_human_knight.png", "walk": range(0, 5), "attack": range(5, 9), "fall": 9, "corpse": 10},
    "human_mage": {"sheet": "wc2_human_mage.png", "walk": range(0, 5), "attack": range(5, 9), "death_sequence": 9},
    "human_gryphon": {"sheet": "wc2_human_gryphon_rider.png", "walk": range(0, 4), "attack": range(4, 7), "death_sequence": 7},
    "human_ballista": {"sheet": "wc2_human_ballista.png", "walk": range(0, 4)},
})
# Ships: one frame per direction (they slide over the water). "walk_oil" is the loaded tanker; "death_sequence" may name
# several rows whose sprites follow each other (the ripper's credit stamp, a wide short label, is left out).
UNITS.update({
    "orcs_destroyer": {"sheet": "wc2_orc_troll_destroyer.png", "walk": range(0, 1), "death_sequence": [1]},
    "orcs_juggernaut": {"sheet": "wc2_orc_ogre_juggernaut.png", "walk": range(0, 1), "death_sequence": [1]},
    "orcs_tanker": {"sheet": "wc2_orc_oil_tanker.png", "walk": range(0, 1), "walk_oil": range(2, 3), "death_sequence": [1]},
    "human_destroyer": {"sheet": "wc2_human_elven_destroyer.png", "walk": range(0, 1)},  # (its other rows are per-direction swirls: no death animation)
    # The battleship sheet has only four directions in its first row; the fifth (down) is the first sprite of the second row.
    "human_battleship": {"sheet": "wc2_human_battleship.png", "walk": range(0, 1), "fifth_from": (1, 0),
                         "death_sequence": [1, 2, 3], "skip_in_sequence": (1, 0)},
    "human_tanker": {"sheet": "wc2_human_oil_tanker.png", "walk": range(0, 1)},
})
FPS = {"walk": 5, "walk_oil": 5, "attack": 8, "chop": 6, "walk_gold": 5, "walk_wood": 5, "death": 4}


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
    sheet = (OUT if spec.get("in_assets") else directory) / spec["sheet"]
    im = Image.open(sheet).convert("RGBA")
    width, height = im.size
    alpha = im.split()[3].load()
    rows = bands([any(alpha[x, y] > 0 for x in range(width)) for y in range(height)])
    animated = {k: v for k, v in spec.items() if isinstance(v, range)}
    row_ids = {i for v in animated.values() for i in v} | ({spec["fall"]} if "fall" in spec else set())
    columns = {r: row_columns(alpha, width, rows[r]) for r in row_ids}
    extra = None  # the fifth direction taken from another row (see "fifth_from")
    if "fifth_from" in spec:
        er, ec = spec["fifth_from"]
        extra = frame_box(alpha, bands([any(alpha[x, y] > 0 for y in range(*rows[er])) for x in range(width)], min_width=4)[ec], rows[er])
    corpse = None
    if "corpse" in spec:  # the body sprite is the first thing in the "corpse" row (one pose for every direction)
        corpse_cols = bands([any(alpha[x, y] > 0 for y in range(*rows[spec["corpse"]])) for x in range(width)], min_width=4)
        corpse = frame_box(alpha, corpse_cols[0], rows[spec["corpse"]])
    sequence = None
    if "death_sequence" in spec:  # every sprite of the row(s), in order
        sequence = []
        for index in (spec["death_sequence"] if isinstance(spec["death_sequence"], list) else [spec["death_sequence"]]):
            r = rows[index]
            seq_cols = bands([any(alpha[x, y] > 0 for y in range(*r)) for x in range(width)], min_width=4)
            for k, c in enumerate(seq_cols):
                if spec.get("skip_in_sequence") == (index, k):
                    continue
                box = frame_box(alpha, c, r)
                if not (box["h"] <= 32 and box["w"] >= 70):  # the ripper's credit stamp is a wide, short label
                    sequence.append(box)

    def frames(kind, d):
        if extra is not None and d == 4 and kind == "walk":
            return [extra]
        return [frame_box(alpha, columns[r][d], rows[r]) for r in spec[kind]]

    animations = []

    def add_death(d):
        if sequence is not None:
            death_frames = sequence
        elif corpse is not None:
            death_frames = [frame_box(alpha, columns[spec["fall"]][d], rows[spec["fall"]]), corpse]
        else:
            return
        variants = [(DIRS[d], False)]
        if "right" in DIRS[d]:
            variants.append((DIRS[d].replace("right", "left"), True))
        for direction, flip in variants:
            a = {"name": f"death_{direction}", "fps": FPS["death"], "loop": False, "frames": death_frames}
            if flip:
                a["flip_horizontal"] = True
            animations.append(a)

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
        add_death(d)
        add("idle", "walk", d, only_first=True)
        add("walk", "walk", d)
        if "attack" in spec:
            add("attack", "attack", d)
        if "chop" in spec:
            add("chop", "chop", d)
        if "walk_gold" in spec:
            add("walk_gold", "walk_gold", d)
            add("idle_gold", "walk_gold", d, only_first=True)
        if "walk_oil" in spec:
            add("walk_oil", "walk_oil", d)
            add("idle_oil", "walk_oil", d, only_first=True)
        if "walk_wood" in spec:
            add("walk_wood", "walk_wood", d)
            add("idle_wood", "walk_wood", d, only_first=True)

    OUT.mkdir(parents=True, exist_ok=True)
    png = f"{name}.png"
    if sheet.resolve() != (OUT / png).resolve():
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
