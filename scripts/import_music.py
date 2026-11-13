#!/usr/bin/env python3
"""Converts the Warcraft II soundtrack (mp3) to Ogg Vorbis in assets/music/ (Allegro cannot read mp3).

Usage: scripts/import_music.py "/path/to/warcraft_songs"   (needs ffmpeg with libvorbis)

The files are recognised by their names ("Intro", "War Room", "Orc 1".."Orc 6", "Orc Victory", "Orc Defeat").
"""
import re, subprocess, sys
from pathlib import Path

if len(sys.argv) != 2:
    sys.exit(__doc__)
src = Path(sys.argv[1])
out = Path(__file__).resolve().parent.parent / "assets" / "music"
out.mkdir(parents=True, exist_ok=True)

names = [(r"intro", "intro"), (r"war room", "war_room"), (r"orc (\d)", None), (r"orc victory", "victory"), (r"orc defeat", "defeat")]
done = 0
for f in sorted(src.glob("*.mp3")):
    low = f.stem.lower()
    target = None
    for pattern, name in names:
        m = re.search(pattern, low)
        if m:
            target = name or f"orc{m.group(1)}"
            break
    if not target:
        print("skipped", f.name)
        continue
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(f), "-vn", "-c:a", "libvorbis", "-q:a", "3",
                    str(out / f"{target}.ogg")], check=True)
    done += 1
print(f"{done} tracks in {out}")
