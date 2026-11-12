#!/usr/bin/env python3
"""Crops building / gold-mine sprites out of the Warcraft II sheets from The Spriters Resource.

Sheets (download manually from https://www.spriters-resource.com/ms_dos/warcraftii/):
  asset/29484  Orc Buildings (Summer)   -> --buildings <png>
  asset/29542  Miscellaneous            -> --misc <png>   (gold mine, optional)

Usage: scripts/extract_sprites.py --buildings orc_buildings.png [--misc misc.png]
Writes PNGs to assets/buildings/.
"""
import argparse
import pathlib

from PIL import Image

OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "buildings"

# (x, y, w, h) of each sprite inside its sheet.
BUILDINGS = {
    "great_hall_construction": (25, 555, 101, 100),
    "great_hall": (144, 543, 119, 116),
    "pig_farm_construction": (272, 603, 63, 64),
    "pig_farm": (337, 603, 64, 64),
    "barracks_construction": (14, 448, 92, 89),
    "barracks": (109, 441, 96, 96),
}
MISC = {
    "gold_mine": (13, 12, 96, 89),
    "gold_mine_active": (13, 108, 96, 89),
}


def crop_all(sheet: str, boxes: dict) -> None:
    im = Image.open(sheet).convert("RGBA")
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (x, y, w, h) in boxes.items():
        im.crop((x, y, x + w, y + h)).save(OUT / f"{name}.png")
        print("wrote", OUT / f"{name}.png")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--buildings")
    ap.add_argument("--misc")
    args = ap.parse_args()
    if args.buildings:
        crop_all(args.buildings, BUILDINGS)
    if args.misc:
        crop_all(args.misc, MISC)
