#!/usr/bin/env python3
"""Converts the first-generation "v0.7" content files into the format the app loads (docs/CONTENT_FORMAT.md).

Those files were written before the format was fixed. Three shapes are understood:
  - a style pack      {"kind": "style_pack", "preset": {...}, "drum_grooves": {...}, "synth_grooves": {...}}
  - an instrument     {"kind": "instrument", "synth": {...}, "fx": {...}}
  - a drum kit        {"kind": "drum_kit", "pads": [...], "fx": {...}}
A file may also hold a list of these, or an object with a list under "presets".

usage: convert_v07.py <output library folder> <input.json>...
Every written file still has to pass `ld_content check`.
"""
import json
import re
import sys
from pathlib import Path

PADS = ["kick", "snare", "clap", "ch", "oh", "rim", "tom", "perc"]
TRACKS = ["bass", "lead", "syn3", "syn4"]
ENGINES = {"subtractive": "subtractive", "fm2op": "fm", "fm": "fm", "pluck": "pluck", "vector": "vector"}


def norm(s):
    return re.sub(r"[^a-z0-9#]", "", str(s).lower())


def slug(s):
    return re.sub(r"-+", "-", re.sub(r"[^a-z0-9]+", "-", str(s).lower())).strip("-")


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def put(out, key, src, name, lo, hi, scale=1.0):
    """Copies a number if it is there, scaled and held inside the range the app accepts."""
    v = src.get(name)
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        out[key] = round(clamp(v * scale, lo, hi), 5)


def slots(lst):
    res = []
    for s in (lst or [])[:3]:
        t = norm(s.get("type", "off"))
        if t in ("", "off"):
            continue
        e = {"type": t}
        put(e, "amount", s, "amount", 0, 1)
        put(e, "rate", s, "rate", 0, 1)
        if s.get("bypass") is True:
            e["bypass"] = True
        res.append(e)
    return res


def track_strip(src, slot_key):
    """level, drive, EQ, sends and effect slots, whatever the old file called them."""
    t = {}
    put(t, "level", src, "level", 0, 1)
    put(t, "drive", src, "drive", 0, 1)
    for old in ("eq_low_db", "eq_low"):
        put(t, "eqLow", src, old, -12, 12)
    for old in ("eq_high_db", "eq_high"):
        put(t, "eqHigh", src, old, -12, 12)
    put(t, "delay", src, "delay_send", 0, 1)
    put(t, "reverb", src, "reverb_send", 0, 1)
    if slot_key in src:
        t["fx"] = slots(src[slot_key])
    return t


def instrument_body(s, fx, with_level=True):
    """s: the flat synth settings of the old file. fx: its mixer part (may be the same object)."""
    o = {}
    if s.get("preset") and norm(s["preset"]) != "custom":
        o["preset"] = s["preset"]
    if "engine" in s:
        o["engine"] = ENGINES.get(norm(s["engine"]), "subtractive")
    for old, new in (("wave1", "osc1"), ("wave2", "osc2")):
        if old in s:
            o[new] = norm(s[old])
    put(o, "detune", s, "detune", -50, 50)
    put(o, "cutoff", s, "cutoff", 60, 12000)
    put(o, "res", s, "resonance", 0.5, 15)
    put(o, "attack", s, "attack", 0.001, 1)
    put(o, "decay", s, "decay", 0.02, 3)
    put(o, "sustain", s, "sustain", 0, 1)
    put(o, "release", s, "release", 0.01, 3)
    if with_level:
        put(o, "level", s, "level", 0, 1)
    lfo = {}
    if "lfo_target" in s:
        lfo["target"] = {"amp": "volume"}.get(norm(s["lfo_target"]), norm(s["lfo_target"]))
    put(lfo, "rate", s, "lfo_rate", 0.05, 20)
    put(lfo, "depth", s, "lfo_depth", 0, 1)
    if lfo:
        o["lfo"] = lfo
    fm = {}
    put(fm, "ratio", s, "fm_ratio", 0.5, 8)
    put(fm, "index", s, "fm_index", 0, 8)
    put(fm, "decay", s, "fm_mod_decay", 0.01, 3)
    put(fm, "feedback", s, "fm_feedback", 0, 1)
    if fm:
        o["fm"] = fm
    for old in ("pluck_bright", "bright"):
        put(o, "pluckBright", s, old, 0, 1)
    # The old files gave the pluck's ring as 0..1. In the app the ring time is the decay (shown times four).
    for old in ("pluck_ring", "ring"):
        if isinstance(s.get(old), (int, float)) and o.get("engine") == "pluck":
            o["decay"] = round(clamp(0.03 + 0.72 * s[old], 0.03, 3), 5)
    vec = {}
    put(vec, "x", s, "vector_x", 0, 1)
    put(vec, "y", s, "vector_y", 0, 1)
    if vec:
        o["vector"] = vec
    put(o, "fold", s, "fold", 0, 1)
    put(o, "slideTime", s, "slide_time_ms", 0.005, 0.5, 0.001)
    put(o, "warmth", s, "warmth", 0, 1)
    put(o, "motion", s, "motion", 0, 1)
    if isinstance(s.get("octave_shift"), int):
        o["octave"] = clamp(s["octave_shift"], -2, 2)
    if isinstance(s.get("chord"), bool):
        o["chord"] = s["chord"]
    strip = track_strip(fx, "slots" if "slots" in fx else "fx_slots")
    if strip:
        o["track"] = strip
    return o


def head(kind, name, style, comment):
    o = {"kind": kind, "format": 1, "name": str(name)[:48]}
    if style:
        o["style"] = style
    if comment:
        o["comment"] = comment
    return o


def conv_instrument(d):
    style = re.sub(r"^\d+_", "", d.get("category", "")).replace("_", " ").strip()
    o = head("instrument", d["name"], style.lower(), d.get("notes", ""))
    s = dict(d.get("synth", {}))
    s.setdefault("engine", d.get("engine", "Subtractive"))
    o.update(instrument_body(s, d.get("fx", {})))
    return "instruments", o


def drumkit_body(d):
    o = {"kit": d.get("kit", "808 Classic")}
    jungle = norm(o["kit"]) == "junglebreak"
    js = d.get("jungle_slicer")
    if jungle and isinstance(js, dict):
        sl = {}
        put(sl, "pitch", js, "repitch", -12, 12)
        put(sl, "tight", js, "tightness", 0.08, 1.5)
        put(sl, "crunch", js, "crunch", 0, 1)
        if sl:
            o["slicer"] = sl
    if not jungle:
        put(o, "hardness", d, "hardness", 0, 1)
    fx = d.get("fx", {})
    strip = track_strip(fx, "slots" if "slots" in fx else "fx_slots")
    if strip:
        o["track"] = strip
    pads = {}
    for p in d.get("pads", []):
        i = p.get("pad")
        if not isinstance(i, int) or not 0 <= i < 8:
            continue
        e = {}
        put(e, "vol", p, "vol", 0, 1)
        put(e, "pan", p, "pan", -1, 1)
        put(e, "pitch", p, "pitch", -24, 24)
        put(e, "decay", p, "decay", 0.02, 2)
        if p.get("choke") in (0, 1, 2):
            e["choke"] = p["choke"]
        if jungle:
            a, b = p.get("slice_start"), p.get("slice_end")
            if isinstance(a, (int, float)) and isinstance(b, (int, float)) and b > a:
                e["sliceStart"] = round(clamp(a, 0, 0.99), 5)
                e["sliceEnd"] = round(clamp(b, 0.01, 1), 5)
            if isinstance(p.get("reverse"), bool):
                e["reverse"] = p["reverse"]
        ds = p.get("drum_synth")
        if not jungle and isinstance(ds, dict) and ds.get("enabled", True):
            y = {}
            put(y, "pitch", ds, "pitch_hz", 25, 1200)
            put(y, "drop", ds, "drop_hz", 0, 600)
            put(y, "dropTime", ds, "drop_time_ms", 0.005, 0.3, 0.001)
            put(y, "length", ds, "length_s", 0.05, 1.5)
            put(y, "tone", ds, "tone_mix", 0, 1)
            put(y, "noise", ds, "noise_mix", 0, 1)
            put(y, "noiseFreq", ds, "noise_freq_hz", 300, 10000)
            put(y, "fm", ds, "fm_amount", 0, 8)
            put(y, "fmRatio", ds, "fm_ratio", 0.5, 8)
            put(y, "drive", ds, "drive", 0, 1)
            put(y, "click", ds, "click", 0, 1)
            e["synth"] = y
        if e:
            pads[PADS[i]] = e
    if pads:
        o["pads"] = pads
    return o


def conv_drumkit(d):
    o = head("drumkit", d["name"], d.get("style", "").lower(), d.get("notes", ""))
    o.update(drumkit_body(d))
    return "drumkits", o


def conv_pack(d):
    """One style pack becomes one groove per drum groove. Each carries the pack's sounds and all its note patterns,
    so the variants can go on scenes A to D."""
    pr = d["preset"]
    style = d.get("style", "")
    dr = pr.get("drums", {})
    kit = {"kit": dr.get("kit", "808 Classic")}
    if norm(kit["kit"]) != "junglebreak":
        put(kit, "hardness", dr, "hardness", 0, 1)
    strip = track_strip(dr, "fx_slots")
    if strip:
        kit["track"] = strip
    chokes = {k: {"choke": v} for k, v in (dr.get("choke_groups") or {}).items() if norm(k) in PADS and v in (0, 1, 2)}
    if chokes:
        kit["pads"] = chokes
    synths = pr.get("synths", [])
    sgrooves = list((d.get("synth_grooves") or {}).values())
    by_name = {norm(g.get("name", "")): g for g in sgrooves}
    entries = []
    for i, s in enumerate(synths):
        g = by_name.get(norm(s.get("name", ""))) or (sgrooves[i] if i < len(sgrooves) else None)
        s = dict(s)
        if g:                                           # the pattern says in which octave and whether as chords it is meant
            for k in ("octave_shift", "chord"):
                if k in g:
                    s[k] = g[k]
        e = {"track": s.get("track", TRACKS[i] if i < 4 else "syn4"), "instrument": instrument_body(s, s, with_level=False)}
        if g:
            notes = []
            for n in g.get("notes", []):
                x = {"step": int(n["step"]), "degree": int(n["degree"]), "len": max(1, int(n.get("len", 1)))}
                if n.get("accent"):
                    x["accent"] = True
                if n.get("slide"):
                    x["slide"] = True
                notes.append(x)
            e["notes"] = notes
        entries.append(e)
    chain = " ".join(pr.get("arrangement_chain", []))
    moves = "; ".join("slot %d %s %s %s to %s" % (a.get("slot", 0) + 1, a.get("track"), a.get("control"), a.get("start"), a.get("end"))
                      for a in pr.get("song_automation", []))
    out = []
    for gname, rows in (d.get("drum_grooves") or {}).items():
        title = re.sub(r"^\d+_", "", gname).replace("_", " ")
        comment = "From the style pack \"%s\"." % pr["name"]
        if chain:
            comment += " Suggested chain: %s." % chain
        if moves:
            comment += " Suggested song moves: %s." % moves
        o = head("groove", "%s: %s" % (pr["name"], title), style.lower(), comment)
        o.update({"bpm": pr.get("bpm", 120), "swing": clamp(pr.get("swing", 0), 0, 0.5), "root": pr.get("root", "C"),
                  "scale": norm(pr.get("scale", "minor")), "octave": clamp(int(pr.get("octave", 3)), 1, 5)})
        steps = max([len(re.sub(r"[ |]", "", r)) for r in rows.values()] + [16])
        for e in entries:
            for n in e.get("notes", []):
                steps = max(steps, n["step"] + 1)
        bars = clamp((steps + 15) // 16, 1, 8)
        o["bars"] = bars
        for e in entries:                               # notes may not run past the end
            for n in e.get("notes", []):
                n["len"] = max(1, min(n["len"], bars * 16 - n["step"]))
        o["drums"] = {"drumkit": kit, "steps": {norm(k): v for k, v in rows.items() if norm(k) in PADS}}
        o["synths"] = entries
        out.append(("grooves", o))
    return out


def convert(d):
    if isinstance(d, list):
        return [x for e in d for x in convert(e)]
    if not isinstance(d, dict):
        return []
    kind = norm(d.get("kind", ""))
    if kind == "stylepack" or ("preset" in d and "drum_grooves" in d):
        return conv_pack(d)
    if kind == "instrument" and "synth" in d:
        return [conv_instrument(d)]
    if kind in ("drumkit", "drum_kit") and isinstance(d.get("pads"), list):
        return [conv_drumkit(d)]
    for key in ("presets", "instruments", "kits", "drum_kits", "items"):
        if isinstance(d.get(key), list):
            return convert(d[key])
    return []


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    lib = Path(sys.argv[1])
    seen, n = set(), 0
    for arg in sys.argv[2:]:
        try:
            data = json.loads(Path(arg).read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            print("SKIP %s: %s" % (arg, e))
            continue
        items = convert(data)
        if not items:
            print("SKIP %s: not a v0.7 pack, instrument or drum kit" % arg)
        for folder, obj in items:
            name = slug(obj["name"])
            if (folder, name) in seen:                  # the same preset shipped in two archives
                continue
            seen.add((folder, name))
            (lib / folder).mkdir(parents=True, exist_ok=True)
            (lib / folder / (name + ".json")).write_text(json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            n += 1
    print("wrote %d files" % n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
