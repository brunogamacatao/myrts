#!/usr/bin/env python3
"""Crops building / gold-mine sprites out of the Warcraft II sheets from The Spriters Resource.

Sheets (download manually from https://www.spriters-resource.com/ms_dos/warcraftii/):
  asset/29484  Orc Buildings (Summer)   -> --buildings <png>
  asset/29540  Human Buildings (Summer) -> --human <png>
  asset/29542  Miscellaneous            -> --misc <png>   (gold mine, optional)

Usage: scripts/extract_sprites.py --buildings orc_buildings.png [--human human_buildings.png] [--misc misc.png]
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
    "barracks_construction": (16, 347, 92, 86),
    "barracks": (108, 335, 94, 103),
    # the hull on its stocks, with the red tusk poles
    "shipyard_construction": (14, 448, 92, 89),
    "shipyard": (109, 441, 96, 96),
    # production buildings of the Horde
    "ogre_mound_construction": (18, 260, 88, 77),
    "ogre_mound": (109, 242, 95, 96),
    "temple_construction": (12, 152, 79, 79),
    "temple": (108, 152, 94, 83),
    "dragon_roost_construction": (212, 160, 93, 74),
    "dragon_roost": (308, 144, 96, 96),
    "alchemist_construction": (414, 170, 88, 70),
    "alchemist": (504, 149, 94, 91),
}
HUMAN_BUILDINGS = {
    "alliance_town_hall_construction": (270, 154, 111, 95),
    "alliance_town_hall": (270, 17, 119, 104),
    "alliance_farm_construction": (398, 73, 63, 59),
    "alliance_farm": (398, 4, 64, 64),
    "alliance_barracks_construction": (107, 370, 93, 82),
    "alliance_barracks": (105, 271, 95, 85),
    # production buildings of the Alliance
    "alliance_stables": (205, 264, 95, 88),
    "alliance_stables_construction": (205, 353, 95, 88),
    "alliance_mage_tower": (308, 264, 94, 96),
    "alliance_mage_tower_construction": (308, 365, 87, 91),
    "alliance_aviary": (408, 264, 94, 94),
    "alliance_aviary_construction": (408, 360, 94, 95),
    "alliance_blacksmith": (4, 264, 96, 96),
    "alliance_blacksmith_construction": (4, 365, 96, 90),
    "alliance_shipyard": (4, 458, 96, 94),
    "alliance_shipyard_construction": (4, 553, 96, 97),
}
MISC = {
    "gold_mine": (13, 12, 96, 89),
    "gold_mine_active": (13, 108, 96, 89),
    "oil_patch": (111, 206, 94, 79),
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
    ap.add_argument("--human")
    ap.add_argument("--misc")
    args = ap.parse_args()
    if args.buildings:
        crop_all(args.buildings, BUILDINGS)
    if args.human:
        crop_all(args.human, HUMAN_BUILDINGS)
    if args.misc:
        crop_all(args.misc, MISC)
