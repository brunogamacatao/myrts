#!/usr/bin/env python3
"""Builds assets/data/tilesets.json: for each of the four terrain eras, where every map-tile code lives in the
tileset image.

A Warcraft II map stores one 16-bit code per 32x32 tile (the MTXM section of a .pud). The tileset image holds
the megatiles in a 19-column grid (33 px pitch: 32 px plus a 1 px gap); this table says which one a code is.

The table comes from the data.js of jcfieldsdev/warcraft2-map-editor (MIT licence, see NOTICE.md):
    scripts/extract_tilesets.py path/to/warcraft2-map-editor/www/scripts/data.js
"""
import json
import pathlib
import re
import sys

OUT = pathlib.Path(__file__).resolve().parent.parent / "assets" / "data" / "tilesets.json"
ERAS = ["forest", "winter", "wasteland", "swamp"]  # the values of the ERA section of a .pud


def main(data_js: str) -> None:
    text = pathlib.Path(data_js).read_text()
    start = text.index("data.tiles = [")
    end = text.index("data.races")
    block = text[start:end]

    # One `{ // era ... }` object per era, in ERA order.
    parts = re.split(r"\{\s*//\s*(\w+)", block)[1:]  # [name, body, name, body, ...]
    eras = []
    for name, body in zip(parts[0::2], parts[1::2]):
        tiles = [[int(c, 16), int(x), int(y)] for c, x, y in re.findall(r"0x([0-9a-fA-F]+):\s*\{x:\s*(\d+),\s*y:\s*(\d+)\}", body)]
        eras.append({"name": name, "image": f"tilesets/{name}.png", "tiles": tiles})
    names = [e["name"] for e in eras]
    assert names == ERAS, names

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps({"tile_size": 32, "eras": eras}, separators=(",", ":")))
    print("wrote", OUT, [len(e["tiles"]) for e in eras], "tiles per era")


if __name__ == "__main__":
    main(sys.argv[1])
