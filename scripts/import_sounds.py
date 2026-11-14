#!/usr/bin/env python3
"""Imports the Warcraft II sounds the game uses and writes assets/data/sounds.json.

Usage: scripts/import_sounds.py "/path/to/WarCraft 2 Sounds"

Blizzard's naming convention: <unit>what = selected, <unit>yess* = acknowledge, <unit>pis*/pissed* = annoyed,
<unit>ready = finished training, <unit>help = under attack, <unit>dead = death. The same events are what Wargus
(scripts/sound.lua, scripts/<race>/units.lua) calls selected, acknowledge, annoyed, ready, help and dead.
The WAV files are copied, lower-cased, to assets/sounds/ (the table refers to them by that name).
"""
import json, re, shutil, sys
from pathlib import Path

if len(sys.argv) != 2:
    sys.exit(__doc__)
src = Path(sys.argv[1])
root = Path(__file__).resolve().parent.parent
out = root / "assets" / "sounds"
out.mkdir(parents=True, exist_ok=True)


def f(folder, *stems):
    """Files of `folder` whose stem starts with any of `stems` (e.g. 'Owhat' matches Owhat1..6)."""
    names = []
    for stem in stems:
        found = sorted(p.name for p in (src / folder).glob("*.wav") if re.fullmatch(stem + r"\d*", p.stem, re.I))
        if not found:
            sys.exit(f"missing {folder}/{stem}")
        names += found
    return [(folder, n) for n in names]


def v(**events):
    return events


# folder-qualified references are flattened to lower-case names at the end
orc = dict(selected=f("Orc", "Owhat"), acknowledge=f("Orc", "Oyessir"), annoyed=f("Orc", "Opissed"),
           help=f("Orc", "Ohelp"), dead=f("Orc", "Odead"), work_done=f("Orc", "Owrkdone"))
human = dict(selected=f("Human", "Hwhat"), acknowledge=f("Human", "Hyessir"), annoyed=f("Human", "Hpissed"),
             help=f("Human", "Hhelp"), dead=f("Human", "Hdead"), work_done=f("Human", "Hwrkdone"))
M = "Misc"
voices = {
    "orc": dict(orc),
    "human": dict(human),
}
units = {
    "orc_peon": dict(voice="orc", ready=f("Peon", "Pnready"), attack=f(M, "Peonatak")),
    "orc_grunt": dict(voice="orc", ready=f("Orc", "Oready"), attack=f(M, "Sword")),
    "orc_troll_axethrower": dict(voice="orc", selected=f("Troll", "Trwhat"), acknowledge=f("Troll", "Tryessr"),
                                 annoyed=f("Troll", "Trpissd"), ready=f("Troll", "Trready"), attack=f(M, "Bowfire")),
    "orc_ogre": dict(voice="orc", selected=f("Ogre", "Ogwhat"), acknowledge=f("Ogre", "Ogyessr"),
                     annoyed=f("Ogre", "Ogpissd"), ready=f("Ogre", "Ogready"), attack=f(M, "Punch")),
    "orc_death_knight": dict(voice="orc", selected=f("DeathKnt", "Dkwhat"), acknowledge=f("DeathKnt", "Dkyessr"),
                             annoyed=f("DeathKnt", "Dkpissd"), ready=f("DeathKnt", "Dkready"), attack=f(M, "Fireball")),
    "orc_dragon": dict(voice="orc", selected=f("Dragon", "Drwhat"), acknowledge=f("Dragon", "Dryessr"),
                       annoyed=f("Dragon", "Dryessr"), ready=f("Dragon", "Drready"), attack=f(M, "Firehit")),
    "orc_zeppelin": dict(voice="orc", selected=f("Zeppelin", "Gbyessr"), acknowledge=f("Zeppelin", "Gbyessr"),
                         annoyed=f("Zeppelin", "Gbpissd"), ready=f("Zeppelin", "Gbready")),
    "orc_catapult": dict(voice="orc", selected=f(M, "Catyessr"), acknowledge=f(M, "Catyessr"),
                         annoyed=f(M, "Catyessr"), attack=f(M, "Catapult")),
    "alliance_peasant": dict(voice="human", selected=f("Peasant", "Pswhat"), acknowledge=f("Peasant", "Psyessr"),
                             annoyed=f("Peasant", "Pspissd"), ready=f("Peasant", "Psready"),
                             work_done=f("Peasant", "Pswrkdon"), attack=f(M, "Sword")),
    "alliance_footman": dict(voice="human", ready=f("Human", "Hready"), attack=f(M, "Sword")),
    "alliance_archer": dict(voice="human", selected=f("Elves", "Ewhat"), acknowledge=f("Elves", "Eyessir"),
                            annoyed=f("Elves", "Epissed"), ready=f("Elves", "Eready"), attack=f(M, "Bowfire")),
    "alliance_knight": dict(voice="human", selected=f("Knight", "Knwhat"), acknowledge=f("Knight", "Knyessr"),
                            annoyed=f("Knight", "Knpissd"), ready=f("Knight", "Knready"), attack=f(M, "Sword")),
    "alliance_mage": dict(voice="human", selected=f("Wizard", "Wzwhat"), acknowledge=f("Wizard", "Wzyessr"),
                          annoyed=f("Wizard", "Wzpissd"), ready=f("Wizard", "Wzready"), attack=f(M, "Firehit")),
    "alliance_gryphon": dict(voice="human", selected=f("Griffon", "Grwhat"), acknowledge=f("Griffon", "Griffon"),
                             annoyed=f("Griffon", "Griffon"), ready=f("Griffon", "Griffon"), attack=f(M, "Punch")),
    "alliance_ballista": dict(voice="human", selected=f(M, "Catyessr"), acknowledge=f(M, "Catyessr"),
                              annoyed=f(M, "Catyessr"), attack=f(M, "Catapult")),
}
def ship(prefix, attack):
    return dict(voice="{}", selected=f("Ships", prefix + "shpwht"), acknowledge=f("Ships", prefix + "shpyes"),
                annoyed=f("Ships", prefix + "shppis"), ready=f("Ships", prefix + "shpredy"), dead=f("Ships", "Shipsink"), attack=attack)


units.update({
    "orc_tanker": {**ship("O", []), "voice": "orc"},
    "orc_destroyer": {**ship("O", f(M, "Catapult")), "voice": "orc"},
    "orc_juggernaut": {**ship("O", f(M, "Catapult")), "voice": "orc"},
    "alliance_tanker": {**ship("H", []), "voice": "human"},
    "alliance_destroyer": {**ship("H", f(M, "Catapult")), "voice": "human"},
    "alliance_battleship": {**ship("H", f(M, "Catapult")), "voice": "human"},
})
B = "Bldg"
buildings = {
    "great_hall": f(B, "Ochant"), "pig_farm": f(B, "Ofarm"), "barracks": f(B, "Ochant"),
    "ogre_mound": f(B, "Ogrecamp"), "temple_of_the_damned": f(B, "Dthtower"), "dragon_roost": f(B, "Dragon"),
    "alchemist": f(B, "Alchemst"),
    "alliance_town_hall": f(B, "Hchant"), "alliance_farm": f(B, "Hfarm"), "alliance_barracks": f(B, "Hchant"),
    "alliance_stables": f(B, "Stables"), "alliance_mage_tower": f(B, "Wzrdtowr"), "alliance_aviary": f(B, "Aviary"),
    "alliance_blacksmith": f(B, "Smith"),
    "orc_shipyard": f(B, "Shipbell"), "alliance_shipyard": f(B, "Shipbell"),
}
effects = {
    "construct": f(M, "Constrct"), "error": f(M, "Error"), "tree": f(M, "Tree"),
    "building_destroyed": f(M, "Bldexpl"),
}


def flat(ref):
    folder, name = ref
    dest = name.lower().replace(" ", "_")
    shutil.copyfile(src / folder / name, out / dest)
    return dest


def flatten(d):
    return {k: ([flat(r) for r in x] if isinstance(x, list) else x) for k, x in d.items()}


doc = {
    "voices": {k: flatten(x) for k, x in voices.items()},
    "units": {k: flatten(x) for k, x in units.items()},
    "buildings": {k: {"selected": [flat(r) for r in x]} for k, x in buildings.items()},
    "effects": {k: [flat(r) for r in x] for k, x in effects.items()},
}
(root / "assets" / "data" / "sounds.json").write_text(json.dumps(doc, indent=1) + "\n")
print(f"{len(list(out.glob('*.wav')))} sounds in {out}")
