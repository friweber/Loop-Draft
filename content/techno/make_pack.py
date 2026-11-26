#!/usr/bin/env python3
"""Packs the techno database for the plugin: every .mid file end to end in techno.pack, and techno.json saying
where each one starts, with what the browser shows about it. Run from the repository root after changing the database."""
import json, os
here = os.path.dirname(os.path.abspath(__file__))
cat = json.load(open(os.path.join(here, "catalog.json")))
blob = bytearray()
def add(rel):
    data = open(os.path.join(here, rel), "rb").read()
    off = len(blob); blob.extend(data)
    return off, len(data)
keys = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
scales = {"minor": 0, "major": 1, "dorian": 2, "pentatonic": 3}
gi = {g["slug"]: i for i, g in enumerate(cat["genres"])}
out = {"genres": [], "patterns": [], "songs": []}
for g in cat["genres"]:
    out["genres"].append({"name": g["name"], "bpm": g["bpm"], "root": keys.index(g["key"]), "scale": scales[g["scale"]],
                          "vibe": g["vibe"], "sounds": g["preset_tips"], "pedals": g["pedal_tips"]})
sections = [s["slug"] for s in cat["drum_sections"]]
roles = [r["slug"] for r in cat["synth_roles"]]
for p in cat["patterns"]:
    off, size = add(p["path"])
    drum = p["type"] == "drum"
    out["patterns"].append({"g": gi[p["genre"]], "drum": drum, "cat": (sections if drum else roles).index(p["category"]),
                            "title": p["title"], "bars": p["bars"], "energy": p.get("energy", 0), "about": p["description"],
                            "off": off, "size": size})
for s in cat["starter_songs"]:
    scenes = []
    for sc in s["scenes"]:
        off, size = add(sc["path"])
        scenes.append({"title": sc["title"], "off": off, "size": size})
    out["songs"].append({"title": s["title"], "bpm": s["bpm"], "root": keys.index(s["key"]), "scale": scales[s["scale"]],
                         "chain": s["suggested_chain"].replace(" ", ""), "genres": [gi[x] for x in s["genres_blended"]], "scenes": scenes})
os.makedirs(os.path.join(here, "pack"), exist_ok=True)
open(os.path.join(here, "pack", "techno.pack"), "wb").write(blob)
json.dump(out, open(os.path.join(here, "pack", "techno.json"), "w"), separators=(",", ":"))
print(len(out["patterns"]), "patterns,", len(out["songs"]), "songs,", len(blob), "bytes")
