# Third-party material

## Warcraft II map format and tile algorithm

The way maps are stored and drawn (tile codes built from four terrain quadrants, boundary tiles between terrains,
the movement rules per tile, the `.pud` file layout) was learned from these open-source projects, all MIT-licensed:

* **warcraft2-map-editor** (jcfieldsdev) - <https://github.com/jcfieldsdev/warcraft2-map-editor>
  The table of tile codes per tileset (`assets/data/tilesets.json`, built by `scripts/extract_tilesets.py`), the four
  tileset images in `assets/tilesets/` and the sample maps in `assets/maps/` come from this repository.
* **war2edit** and **war2tools** (Jean Guyomarc'h, Copyright (c) 2014 - 2017) - <https://github.com/war2/war2edit>,
  <https://github.com/war2/war2tools>. Their tile-composition code and format documentation were the reference for
  `src/core/terrain.cpp`, which is a fresh C++ implementation of the same rules.
* The PUD format documentation by the Warcraft II modding community (pudspec) and
  <https://formats.kaitai.io/warcraft_2_pud/>.

> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
> documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
> rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
> persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and
> this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
> "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
> MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
> BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Interface art

The HUD uses the original Warcraft II interface art, gathered by `scripts/extract_ui_assets.py`:

* the 198 icons (46x38) for commands, units and buildings, the resource icons and the gold-mine sprites of each era come
  from **warcraft2-map-editor** (see above);
* the panel textures and the mouse cursors were decoded from `maindat.war` with **war2tools** (see above);
* the lit "mine in use" frames come from the Miscellaneous sheet of The Spriters Resource.

## Warcraft II art and maps

The tileset images, sprites and the maps in `assets/maps/` are Warcraft II material (Blizzard Entertainment), taken from the
sources above and from The Spriters Resource (sprites ripped by Maxim). They are used here as the project's author asked;
check their terms before redistributing the game.

## Sounds
The WAV files in `assets/sounds/` come from the Warcraft II sound pack and are Blizzard Entertainment material; check the
terms before redistributing them. Which sound plays on which event follows the naming convention of the originals and the
event names used by Wargus (`scripts/sound.lua`, GPLv2), used only as a behaviour reference: none of its code was copied.
`scripts/import_sounds.py` regenerates `assets/sounds/` and `assets/data/sounds.json` from the original folder.

## Music
The tracks in `assets/music/` are the remastered Warcraft II soundtrack (Blizzard Entertainment material), converted to Ogg
Vorbis by `scripts/import_music.py`. Check the terms before redistributing them.
