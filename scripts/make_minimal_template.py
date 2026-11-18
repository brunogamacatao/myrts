#!/usr/bin/env python3
"""Builds templates/minimal-game: the smallest game the editor starts a new project from (one faction, a worker and a
soldier, three buildings, one level), reusing the art of games/warcraft-like as placeholders.

  scripts/make_minimal_template.py [path/to/myrts_tool]

The generated folder is committed; run this again only to change what the template contains."""
import json
import shutil
import subprocess
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
src = root / "games" / "warcraft-like"
out = root / "templates" / "minimal-game"
tool = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "build" / "release" / "myrts_tool"

shutil.rmtree(out, ignore_errors=True)
for folder in ("data", "sprites", "buildings", "ui", "cursors", "tilesets", "maps", "strings"):
    (out / folder).mkdir(parents=True)


def load(name):
    return json.loads((src / "data" / name).read_text())


def save(name, doc):
    (out / "data" / name).write_text(json.dumps(doc, indent=2) + "\n")


# ---- art: placeholders taken from the original game
for name in ("food", "frame_bottom", "frame_top", "gold", "icons", "lumber", "minimap_frame", "oil", "panel", "topbar"):
    shutil.copy(src / "ui" / f"{name}.png", out / "ui")
shutil.copytree(src / "cursors", out / "cursors", dirs_exist_ok=True)
shutil.copy(src / "tilesets" / "forest.png", out / "tilesets")
for name in ("alliance_town_hall", "alliance_town_hall_construction", "alliance_farm", "alliance_farm_construction",
             "alliance_barracks", "alliance_barracks_construction", "gold_mine_forest", "gold_mine_active_forest"):
    shutil.copy(src / "buildings" / f"{name}.png", out / "buildings")

sprite_names = {"worker": "human_peasant", "soldier": "human_footman"}
for new, old in sprite_names.items():
    shutil.copy(src / "sprites" / f"{old}.png", out / "sprites" / f"{new}.png")
    doc = json.loads((src / "sprites" / f"{old}.json").read_text())
    doc["spritesheet"] = f"sprites/{new}.png"
    (out / "sprites" / f"{new}.json").write_text(json.dumps(doc, indent=2) + "\n")

# ---- data
tilesets = load("tilesets.json")
tilesets["eras"] = [e for e in tilesets["eras"] if e["name"] == "forest"]
save("tilesets.json", tilesets)

resources = load("resources.json")
resources["resources"] = [r for r in resources["resources"] if r["id"] in ("gold", "lumber")]
for r in resources["resources"]:
    r.pop("pud_source", None)
save("resources.json", resources)

commands = load("commands.json")
save("commands.json", commands)

save("factions.json", {"factions": [{"id": "kingdom", "name": "Kingdom", "worker": "worker", "town_hall": "hall",
                                      "base_buildings": ["barracks"]}]})

units = {u["id"]: u for u in load("units.json")["units"]}
worker = dict(units["alliance_peasant"], id="worker", name="Worker", faction="kingdom", sprite="sprites/worker.json",
              builds=["hall", "farm", "barracks"])
soldier = dict(units["alliance_footman"], id="soldier", name="Soldier", faction="kingdom", sprite="sprites/soldier.json")
save("units.json", {"units": [worker, soldier]})

buildings = {b["id"]: b for b in load("buildings.json")["buildings"]}
hall = dict(buildings["alliance_town_hall"], id="hall", name="Hall", faction="kingdom", trains=["worker"],
            sprite="buildings/alliance_town_hall.png", sprite_construction="buildings/alliance_town_hall_construction.png")
farm = dict(buildings["alliance_farm"], id="farm", name="Farm", faction="kingdom")
barracks = dict(buildings["alliance_barracks"], id="barracks", name="Barracks", faction="kingdom", trains=["soldier"])
save("buildings.json", {"buildings": [hall, farm, barracks]})

save("sounds.json", {"voices": {}, "units": {}, "buildings": {}, "effects": {}})
save("rules.json", {"format_version": 1})
save("ai_profiles.json", {"format_version": 1, "profiles": {"default": {}}})
save("campaign.json", {"levels": [{
    "name": "First Skirmish", "chapter": "Tutorial", "kind": "land", "map": "maps/field.mrmap",
    "briefing": "Gather gold and lumber, build a Barracks and train Soldiers. Destroy the enemy base.",
    "hints": ["Right-click a gold mine or a tree with a Worker selected to gather.",
              "Build Farms for food, and a Barracks to train Soldiers."],
    "player_stock": {"gold": 2000, "lumber": 1500}, "enemy_stock": {"gold": 2000, "lumber": 1500},
    "ai": {"start_delay": 240, "first_wave": 4}}]})

(out / "game.json").write_text(json.dumps({
    "format_version": 1, "id": "minimal-game", "title": "Minimal Game", "version": "0.1.0", "default_language": "en",
    "paths": {"data": "data", "sounds": "sounds", "cursors": "cursors", "campaign": "data/campaign.json",
              "tilesets": "data/tilesets.json", "sound_table": "data/sounds.json", "rules": "data/rules.json",
              "ai_profiles": "data/ai_profiles.json", "strings": "strings", "credits": "credits.txt",
              "fallback_map": "maps/field.mrmap"},
    "music": {}}, indent=2) + "\n")
(out / "credits.txt").write_text("MINIMAL GAME\nMade with MyRTS Engine\n\nArt\nPlaceholders: replace the pictures in sprites/, buildings/, ui/ and tilesets/ with your own.\n")
(out / "strings" / ".gitkeep").write_text("")

subprocess.run([str(tool), "new-map", str(out), "forest", "48", "48", str(out / "maps" / "field.mrmap")], check=True)
print("template written to", out)
