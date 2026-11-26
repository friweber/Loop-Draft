# Loop Master Techno Starter Database (10 Genres · 132 MIDI Patterns)

A curated, musically deep starter library built specifically for **Loop Master** — 100% compliant with Loop Master's 16th-note grid, 8-pad GM drum mapping, accent velocity (`96` / `127`), `+12 tick` slide overlap, max 3-note chords, and channel routing (`Ch 10` Drums, `Ch 1` Bass, `Ch 2` Lead/Arp, `Ch 3` Chords/Stabs, `Ch 4` Pads).

---

## Folder & Taxonomy Architecture (Future Drill-Down Browser Ready)

```text
LoopMaster-Techno-Database/
├── catalog.json                         # Machine-readable metadata index for future drill-down UI
├── README_DATABASE.md                   # This producer & developer guide
├── drums/
│   └── <genre>/
│       ├── 01_intro/                    # Kick-less / atmospheric openers (2 bars)
│       ├── 02_verse_groove/             # Core locked club pockets (2 bars)
│       ├── 03_peak_drop/                # Full-energy peak drops with bar-4 turnarounds (4 bars)
│       ├── 04_break_bridge/             # Breakdown & bridge percussion (2 bars)
│       ├── 05_build_fill/               # Tension risers & 16th-grid turnaround fills (2 bars)
│       └── 06_outro/                    # Clean DJ mix-out grooves (2 bars)
├── synths/
│   └── <genre>/
│       ├── bass/                        # Ch 1 — 2 distinct basslines per genre (Main + Alt)
│       ├── lead_hook/                   # Ch 2 — Melodic hooks, sonar calls, rave screeches
│       ├── arp_seq/                     # Ch 2 — Polymetric sequences & 16th arpeggios
│       ├── chords_stabs/                # Ch 3 — Max-3-note triads, dub chords, rave stabs
│       └── pad_atmosphere/              # Ch 4 — 4-bar sustained progressions & breakdown drones
└── starter_songs/
    ├── song_01_neon_berghain/           # 4 full multi-channel scenes (Hypnotic × Acid 303)
    ├── song_02_midnight_detroiter/      # 4 full multi-channel scenes (Detroit Electro × Melodic)
    └── song_03_iron_cathedral/          # 4 full multi-channel scenes (EBM × Psy-Rolling Techno)
```

### Why Synth Patterns Are Organized by Musical Role
While drums map cleanly to **song structure** (`Intro → Verse → Peak → Break → Build → Outro`), synths work best when filtered first by **Genre** and second by **Musical Role**:
1. **Bass (`Ch 1`)** — Foundation & groove pocket (includes 2 contrasting variations per genre)
2. **Lead & Hook (`Ch 2`)** — Memorable focal riffs and melodies
3. **Arp & Sequence (`Ch 2`)** — Hypnotic polymetric cycles (3-over-4, 5-over-16, 7-step) and 16th arps
4. **Chords & Stabs (`Ch 3`)** — Rhythmic harmonic punches (strictly ≤3 simultaneous voices)
5. **Pad & Atmosphere (`Ch 4`)** — Sustained 4-bar harmonic beds for breakdowns and tension lifts

Because every synth file contains a single melodic track on its natural channel, Loop Master automatically loads it onto its matching synth track **or** lets you audition it on any other synth track.

---

## The 10 Techno Genres Included

| # | Genre | BPM | Key / Scale | Signature Musical DNA |
|---|---|---:|---|---|
| 01 | **Hypnotic Berlin Techno** | 134 | D minor | Dark, rolling, polymetric Berghain / Tresor warehouse techno. |
| 02 | **Acid Techno / 303** | 138 | D minor | Squelching TB-303 basslines, 909 warehouse drums, euphoric rave stabs. |
| 03 | **Industrial / Hard Techno** | 150 | F minor | Relentless 150 BPM Schranz & industrial warehouse devastation, hoover screeches, stomping kicks. |
| 04 | **Dub Techno** | 122 | C minor | Deep, spacious, tape-saturated Basic Channel / Maurizio dub chords and cavernous sub-bass. |
| 05 | **Detroit Techno & Electro** | 130 | A minor | Broken TR-808 electro funk, Drexciya aquatic arps, and soaring soulful Detroit strings. |
| 06 | **Minimal / Micro-Techno** | 126 | G minor | Playful, surgical Perlon / Richie Hawtin micro-grooves, beat-1-skipping bass, 7-step glitch cycles. |
| 07 | **Melodic / Peak-Time Techno** | 128 | A minor | Widescreen Afterlife / Tale Of Us emotional peak-time techno: rolling Am-F-Dm-Em progressions & soaring leads. |
| 08 | **EBM / Dark Body Techno** | 124 | E minor | Cold, militant, sexy DAF / Gesaffelstein / Schwefelgelb dark body techno with 16th octave basslines. |
| 09 | **Tribal / Hardgroove Techno** | 142 | D dorian | Fast, funky, hip-moving Ben Sims / Funk Assault hardgroove with Afro-Latin percussion & Dorian stabs. |
| 10 | **Psy-Techno / Rolling Trance-Tech** | 140 | E minor | Surgical 140 BPM rolling K-b-b-b psy-basslines, spiraling trance-tech arps, and alien laser leads. |

## Genre Sound Design & Pedal Recipes

### Hypnotic Berlin Techno (`01_hypnotic_berlin` — 134 BPM, D minor)
- **Vibe:** Dark, rolling, polymetric Berghain / Tresor warehouse techno.
- **Suggested Presets:** Kick: 909 Punch + Sub Rumble; Bass: Sub/LPF with Drive; Arp: Short FM/Pluck + Dub Delay; Pad: Vector dark drone.
- **Suggested Loop Master Pedals:** Keep-Sub Overdrive on bass, Filtered Dub Delay on Arp/Stabs, Gated/Dark Hall Reverb on Rim & Chords.

### Acid Techno / 303 (`02_acid_techno` — 138 BPM, D minor)
- **Vibe:** Squelching TB-303 basslines, 909 warehouse drums, euphoric rave stabs.
- **Suggested Presets:** Bass: Saw/Square Acid Mono with high resonance & fast env; Drums: Classic 909; Chords: Rave Organ/Stab.
- **Suggested Loop Master Pedals:** Acid Distortion / Fuzz (50% Mix + Keep Sub) on 303, Tempo Delay on Counter-Acid, Gated Reverb on Snare/Clap.

### Industrial / Hard Techno (`03_industrial_hard` — 150 BPM, F minor)
- **Vibe:** Relentless 150 BPM Schranz & industrial warehouse devastation, hoover screeches, stomping kicks.
- **Suggested Presets:** Kick: Hard Distorted 909; Lead: Detuned Saw/FM Hoover; Bass: Saturated Rumble Sub; Chords: Metallic FM Stab.
- **Suggested Loop Master Pedals:** Hard Clip / Fuzz Distortion + Drum Bus Crusher on Kicks & Hats, Frequency Shifter on Rim/Perc, Gated Reverb on Snare.

### Dub Techno (`04_dub_techno` — 122 BPM, C minor)
- **Vibe:** Deep, spacious, tape-saturated Basic Channel / Maurizio dub chords and cavernous sub-bass.
- **Suggested Presets:** Chords: Warm Saw/FM with low-pass filter envelope; Bass: Pure Sine/Tri Sub; Drums: Warm 808/909 low-passed.
- **Suggested Loop Master Pedals:** Filtered Dub Tape Delay (dotted 8th!) + Shimmer/Hall Reverb on Chords & Sonar Lead, Tape Lo-Fi Warble on master/pads.

### Detroit Techno & Electro (`05_detroit_electro` — 130 BPM, A minor)
- **Vibe:** Broken TR-808 electro funk, Drexciya aquatic arps, and soaring soulful Detroit strings.
- **Suggested Presets:** Drums: Punchy TR-808; Bass: Snappy Analog Saw/Sub; Arp: Bright FM/Pluck; Pad: Lush Analog Strings.
- **Suggested Loop Master Pedals:** Stereo Chorus / Ensemble on Strings & Chords, Crunch Overdrive on 808 Snare, Ping-Pong Delay on Aquatic Arp.

### Minimal / Micro-Techno (`06_minimal_micro` — 126 BPM, G minor)
- **Vibe:** Playful, surgical Perlon / Richie Hawtin micro-grooves, beat-1-skipping bass, 7-step glitch cycles.
- **Suggested Presets:** Drums: Short dry Click/Micro kit; Bass: Snappy rubbery FM/Sine; Arp: Ultra-short Percussive Pluck.
- **Suggested Loop Master Pedals:** Transient Puncher (shorten sustain!) on Drums, Tempo Slicer / Micro-Delay on Bleeps, Light Chorus on Glass Pad.

### Melodic / Peak-Time Techno (`07_melodic_peak` — 128 BPM, A minor)
- **Vibe:** Widescreen Afterlife / Tale Of Us emotional peak-time techno: rolling Am-F-Dm-Em progressions & soaring leads.
- **Suggested Presets:** Bass: Tight Moog/Sub Pluck; Arp: Bright Saw Pluck with LPF sweep; Lead: Lush Detuned Supersaw/Mono Glide; Pad: Wide Strings.
- **Suggested Loop Master Pedals:** Shimmer Reverb + Stereo Ping-Pong Delay on Lead & Arp, Sidechain/Ducking Comp on Pad & Bass, Warm Tape Drive on Master.

### EBM / Dark Body Techno (`08_ebm_dark` — 124 BPM, E minor)
- **Vibe:** Cold, militant, sexy DAF / Gesaffelstein / Schwefelgelb dark body techno with 16th octave basslines.
- **Suggested Presets:** Bass: Aggressive Saw/Square Analog with snappy filter decay; Drums: Gated Industrial 707/Linn/909; Lead: Cold Metallic FM.
- **Suggested Loop Master Pedals:** Gated Reverb on Snare, Analog Overdrive / Fuzz on Octave Bass (Keep Sub ON!), Chorus/Flanger on Coldwave Stabs.

### Tribal / Hardgroove Techno (`09_tribal_hardgroove` — 142 BPM, D dorian)
- **Vibe:** Fast, funky, hip-moving Ben Sims / Funk Assault hardgroove with Afro-Latin percussion & Dorian stabs.
- **Suggested Presets:** Drums: Punchy 909 + Conga/Bongo Perc & Tuned Toms; Bass: Funky FM/Analog Pluck; Chords: Filtered Organ/House Stab.
- **Suggested Loop Master Pedals:** Parallel Drum Bus Compressor / Saturation on Percussion, Auto-Filter / Phaser on Chords, Short Slapback Delay on Perc.

### Psy-Techno / Rolling Trance-Tech (`10_psy_rolling_techno` — 140 BPM, E minor)
- **Vibe:** Surgical 140 BPM rolling K-b-b-b psy-basslines, spiraling trance-tech arps, and alien laser leads.
- **Suggested Presets:** Bass: Tight Saw Pluck with snappy LPF env (zero release!); Lead: Resonant FM/Acid Squelch; Arp: Bright Saw Pluck.
- **Suggested Loop Master Pedals:** Stereo Ping-Pong Delay + Phaser/Flanger on Alien Lead, Granular Cloud / Shimmer on Pads, Tight Limiter/Transient Shaper on Drums.

---

## 3 Complete Unique Starter Songs (Ready-to-Load Scenes)

### Song 1: Neon Berghain (Hypnotic Berlin × Acid 303)
- **Tempo / Key:** 136 BPM, D minor
- **Blends:** `01_hypnotic_berlin, 02_acid_techno`
- **Suggested Loop Master Song Chain (Grooves A–D):** `A A B B C D D B C D D A`
- **Scenes:**
  - `song_01_neon_berghain - A_intro.mid` — Scene A — Warehouse Tension Intro (4 bars, 134 notes)
  - `song_01_neon_berghain - B_groove.mid` — Scene B — Hypnotic 303 Lock-In (4 bars, 186 notes)
  - `song_01_neon_berghain - C_break.mid` — Scene C — Acid Filter Scream Break (4 bars, 108 notes)
  - `song_01_neon_berghain - D_peak.mid` — Scene D — Full Berghain Acid Peak (4 bars, 311 notes)
- **Preset & Pedalboard Recipe:**
  - **Drums:** 909 Warehouse Kit + Tape Saturation on Bus
  - **Bass (Ch 1):** Acid 303 Saw/Square Mono with high resonance + Keep-Sub Fuzz pedal (45% Mix)
  - **Lead (Ch 2):** Short Metallic FM Pluck (Modular Bleeps) + Filtered Dub Delay (dotted 8th)
  - **Synth 3 (Ch 3):** Dark Minor Stab / Rave Organ + Cavernous Hall Reverb
  - **Synth 4 (Ch 4):** Brooding Analog Sub-Drone Pad + Slow LPF Sweep

### Song 2: Midnight Detroiter (Detroit Electro × Melodic Peak)
- **Tempo / Key:** 128 BPM, A minor
- **Blends:** `05_detroit_electro, 07_melodic_peak`
- **Suggested Loop Master Song Chain (Grooves A–D):** `A B B C D D B C D D A`
- **Scenes:**
  - `song_02_midnight_detroiter - A_intro.mid` — Scene A — 808 Clock & Strings Opener (4 bars, 142 notes)
  - `song_02_midnight_detroiter - B_groove.mid` — Scene B — Drexciya Electro Funk Groove (4 bars, 210 notes)
  - `song_02_midnight_detroiter - C_break.mid` — Scene C — Widescreen Arena Strings Break (4 bars, 51 notes)
  - `song_02_midnight_detroiter - D_peak.mid` — Scene D — Motor City Euphoric Climax (4 bars, 252 notes)
- **Preset & Pedalboard Recipe:**
  - **Drums:** Punchy TR-808 / Hybrid Club Kit + Light Gated Reverb on Snare
  - **Bass (Ch 1):** Snappy SH-101 / Moog Analog Bass + Subtle Chorus on high notes
  - **Lead (Ch 2):** Lush Detuned Supersaw / Aquatic FM Pluck + Stereo Ping-Pong Delay
  - **Synth 3 (Ch 3):** Warm Motor City Brass/Organ Stabs + Tape Lo-Fi Warble
  - **Synth 4 (Ch 4):** Strings of Life Wide Analog Strings + Shimmer Reverb

### Song 3: Iron Cathedral (EBM Dark Body × Psy-Rolling Techno)
- **Tempo / Key:** 140 BPM, E minor
- **Blends:** `08_ebm_dark, 10_psy_rolling_techno`
- **Suggested Loop Master Song Chain (Grooves A–D):** `A A B B C D D B C D D A`
- **Scenes:**
  - `song_03_iron_cathedral - A_intro.mid` — Scene A — Cold Industrial March Intro (4 bars, 148 notes)
  - `song_03_iron_cathedral - B_groove.mid` — Scene B — EBM Stomp & Rolling Psy Bass (4 bars, 204 notes)
  - `song_03_iron_cathedral - C_break.mid` — Scene C — Wormhole Gothic Tension Break (4 bars, 106 notes)
  - `song_03_iron_cathedral - D_peak.mid` — Scene D — Iron Cathedral Psy-Body Drop (4 bars, 296 notes)
- **Preset & Pedalboard Recipe:**
  - **Drums:** Hard Gated Industrial 909/Linn Kit + Drum Bus Crusher (Parallel 40%)
  - **Bass (Ch 1):** Tight Saw Pluck (Psy Gallop / EBM Octave) + Keep-Sub Overdrive
  - **Lead (Ch 2):** Cold Metallic FM / Acid Laser Lead + Phaser + Ping-Pong Delay
  - **Synth 3 (Ch 3):** Brutalist Coldwave Brass / Trance-Gated Chord Stab + Flanger
  - **Synth 4 (Ch 4):** Dark Gothic Bunker Drone + Granular Cloud Reverb
