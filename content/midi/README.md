# Loop Master MIDI patterns: what is here and how to write more

This folder holds the 62 grooves that ship with Loop Master and Loop Draft, exported as Standard MIDI Files, and
the rules a new file has to follow so that Loop Master's pattern browser can use it. It is written for someone
(or an AI agent) who will draft more patterns.

The files hold **patterns only**: which drum hits and which notes. The sounds (drum kit, synth presets, pedals) are
not in a MIDI file. Nothing here has been checked by ear in its MIDI form; the descriptions say what each style is
meant to sound like.

## How a file is laid out

- Standard MIDI File format 1, 480 ticks per quarter note, 4/4. One sixteenth note = 120 ticks.
- Track 1: tempo and time signature. Track 2: drums. Tracks 3 to 6: Bass, Lead, Synth 3, Synth 4.
- **Drums are on MIDI channel 10** with these General MIDI notes. Loop Master has eight drum pads:

| Pad | Note | | Pad | Note |
|---|---|---|---|---|
| Kick | 36 (C1) | | Open hat | 46 |
| Snare | 38 | | Rim | 37 |
| Clap | 39 | | Tom | 45 |
| Closed hat | 42 | | Perc | 64 |

  Other General MIDI drum notes are folded onto these pads when a file is read (35 to kick, 40 to snare, 44 to closed
  hat, 49/51/52/55/57/59 to open hat, 41/43/47/48/50 to tom, anything else to perc).
- **Bass is on channel 1, Lead on channel 2, Synth 3 on channel 3, Synth 4 on channel 4.**
- Velocity: **100 or more is an accent** (written as 127), anything below is a normal hit (written as 96). There are
  no other dynamics.
- A slide (glide into a note) is written as an overlap: the note before ends 12 ticks after the slide note starts.
- Drum rolls inside one sixteenth are written as 2, 3 or 4 evenly spaced hits. Reading a file, Loop Master keeps
  only one hit per sixteenth, so a new file should not rely on rolls.

## What Loop Master does with a file

Put the files in any folder, press MIDI FOLDER in the pattern list of the DRUMS or SYNTH page and choose it.

- Everything is moved to the nearest **sixteenth note**. Triplets and finer timing are lost.
- A pattern is **1 to 16 bars** long (the last note decides, rounded up to whole bars). Longer files are cut.
- On the DRUMS page the drum notes are used: channel 10 only if the file has a channel 10, otherwise every note.
- On the SYNTH page the notes of the open track's channel are used (channel 1 for Bass, 2 for Lead, 3 and 4 for the
  other two). If the file has nothing on that channel, all notes that are not on channel 10 are used. So a file
  with one melodic part works on any synth track.
- Melodic notes are **moved to the nearest note of the project's scale**. Write in the key and scale given below (or
  in any key, knowing it will be bent to the project's). Chords of up to three notes survive; denser ones collide.
- At most 256 notes per track. Note lengths are in sixteenths.
- The tempo in the file is not used; the project or the DAW sets the tempo. The tempo below says what each
  pattern was written for.

## Rules of thumb for new patterns

1. One idea per file. 1, 2 or 4 bars are the most useful lengths; 8 for a melody.
2. Name the file `<pack or style> - <what it is>.mid`, for example `jungle - amen roll fill.mid`. The file name is all
   the user sees in the list.
3. Give drums-only files and melody-only files; a file may also hold a whole groove (drums, bass and lead on their
   channels) like the ones here.
4. Make variations in sets of four that belong together: a main pattern, a busier one, a breakdown, a fill. Users
   put them into grooves A to H and chain them into a song.
5. Bass lines should work on the root of the scale; leads should stay within about two octaves.
6. Use accents (velocity 100+) with intent: on the kick of beat 1, on back-beat snares, on the notes of a bass line
   that should open the filter.

A second way to add material, with sounds included, is the JSON groove format in `docs/CONTENT_FORMAT.md` of the
repository; the MIDI route is for patterns only.

## The styles

### Acid techno / acid house

*130-140 BPM, minor*

Four-on-the-floor kick, off-beat open hat, straight closed hats, clap on 2 and 4. The star is ONE bass line of sixteenth notes that mostly repeats the root, jumps an octave now and then, and uses accents and slides (overlapping notes) so a resonant filter can squelch. Few different pitches, lots of rhythm. No chords.

**Acid Tunnel 138** (written at 138 BPM in D minor)

| File | Bars | What it is |
|---|---|---|
| `acid-tunnel-138-acid-build-rolls.mid` | 1 | Acid Build Rolls (drums, bass, lead, syn3, syn4) |
| `acid-tunnel-138-acid-main-4x4.mid` | 1 | Acid Main 4x4 (drums, bass, lead, syn3, syn4) |
| `acid-tunnel-138-acid-syncopated-hat.mid` | 1 | Acid Syncopated Hat (drums, bass, lead, syn3, syn4) |
| `acid-tunnel-138-acid-tension-break.mid` | 1 | Acid Tension Break (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A A B B C C D C (A to D in the order of the table).

### Jungle / oldschool drum and bass

*160-172 BPM, minor*

Chopped breakbeats: kick on 1 and the 'and' of 3 (steps 0 and 10), snare on 2 and 4 (steps 4 and 12) with ghost snares and shuffled hats in between; bars 2 and 4 rearrange the hits. Bass is a long, low sub note per bar or half bar (reese), sometimes sliding. Pads or stabs are sparse, one chord every one or two bars.

**Amen Horizon 168** (written at 168 BPM in D minor)

| File | Bars | What it is |
|---|---|---|
| `amen-horizon-168-amen-chopped-syncopated.mid` | 1 | Amen Chopped Syncopated (drums, bass, lead, syn3, syn4) |
| `amen-horizon-168-amen-classic-twostep.mid` | 1 | Amen Classic TwoStep (drums, bass, lead, syn3, syn4) |
| `amen-horizon-168-atmospheric-jungle-filter-brea.mid` | 1 | Atmospheric Jungle Filter Brea (drums, bass, lead, syn3, syn4) |
| `amen-horizon-168-jungle-snare-rush-turnaround.mid` | 1 | Jungle Snare Rush Turnaround (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

**Amen Mirage 168** (written at 168 BPM in D minor)

| File | Bars | What it is |
|---|---|---|
| `amen-mirage-168-jungle-amen-classic-chop.mid` | 1 | Jungle Amen Classic Chop (drums, bass, lead, syn3, syn4) |
| `amen-mirage-168-jungle-atmospheric-half.mid` | 1 | Jungle Atmospheric Half (drums, bass, lead, syn3, syn4) |
| `amen-mirage-168-jungle-snare-rush-fill.mid` | 1 | Jungle Snare Rush Fill (drums, bass, lead, syn3, syn4) |
| `amen-mirage-168-jungle-syncopated-reverse.mid` | 1 | Jungle Syncopated Reverse (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### EBM / dark electro

*118-128 BPM, minor or phrygian*

Stiff, machine-like. Kick on every beat or a 'boom . boom boom' body-music pattern, snare/clap on 2 and 4, sparse metallic percussion. The bass is a sequenced sixteenth or eighth-note line on the root and octave with a minor second or fifth for menace; it carries the track. Leads are short, cold stabs.

**Body Voltage 124** (written at 124 BPM in E minor)

| File | Bars | What it is |
|---|---|---|
| `body-voltage-124-ebm-broken-glitch-break.mid` | 1 | EBM Broken Glitch Break (drums, bass, lead, syn3, syn4) |
| `body-voltage-124-ebm-stompy-machine.mid` | 1 | EBM Stompy Machine (drums, bass, lead, syn3, syn4) |
| `body-voltage-124-ebm-syncopated-industrial.mid` | 1 | EBM Syncopated Industrial (drums, bass, lead, syn3, syn4) |
| `body-voltage-124-ebm-tribal-tom-drive.mid` | 1 | EBM Tribal Tom Drive (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A A B B C C D C (A to D in the order of the table).

**Brutalist Body 124** (written at 124 BPM in E minor)

| File | Bars | What it is |
|---|---|---|
| `brutalist-body-124-dark-electro-glitch-break.mid` | 1 | Dark Electro Glitch Break (drums, bass, lead, syn3, syn4) |
| `brutalist-body-124-ebm-stomp-machine.mid` | 1 | EBM Stomp Machine (drums, bass, lead, syn3, syn4) |
| `brutalist-body-124-ebm-syncopated-industrial.mid` | 1 | EBM Syncopated Industrial (drums, bass, lead, syn3, syn4) |
| `brutalist-body-124-ebm-tribal-tom-drive.mid` | 1 | EBM Tribal Tom Drive (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A A B B C C D C (A to D in the order of the table).

### Breakcore / IDM glitch

*170-190 BPM, minor or chromatic feel*

Deliberately broken: snare rolls (32nd repeats), kicks in unexpected places, bars that differ from each other, sudden gaps. Drums should never loop identically for more than a bar. Bass and melody are fragments: short blips, micro-arpeggios, long notes cut off.

**Drill Matrix 182** (written at 182 BPM in C minor)

| File | Bars | What it is |
|---|---|---|
| `drill-matrix-182-afx-micro-click-groove.mid` | 1 | AFX Micro Click Groove (drums, bass, lead, syn3, syn4) |
| `drill-matrix-182-ambient-idm-metronome-break.mid` | 1 | Ambient IDM Metronome Break (drums, bass, lead, syn3, syn4) |
| `drill-matrix-182-breakcore-snare-drill.mid` | 1 | Breakcore Snare Drill (drums, bass, lead, syn3, syn4) |
| `drill-matrix-182-idm-choke-contrast.mid` | 1 | IDM Choke Contrast (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

**Glitch Cortex 180** (written at 180 BPM in C minor)

| File | Bars | What it is |
|---|---|---|
| `glitch-cortex-180-breakcore-4x-snare-chaos.mid` | 1 | Breakcore 4x Snare Chaos (drums, bass, lead, syn3, syn4) |
| `glitch-cortex-180-breakcore-hyper-chop.mid` | 1 | Breakcore Hyper Chop (drums, bass, lead, syn3, syn4) |
| `glitch-cortex-180-glitch-micro-break.mid` | 1 | Glitch Micro Break (drums, bass, lead, syn3, syn4) |
| `glitch-cortex-180-idm-aphex-syncopation.mid` | 1 | IDM Aphex Syncopation (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### Lo-fi house

*118-124 BPM, minor or dorian*

Soft four-on-the-floor kick, loose shuffled hats (use swing), clap or rim on 2 and 4. Warm, simple chord stabs or a two-note bass that repeats. Nothing is busy; the charm is in the swing and the repetition.

**(single grooves)** (written at 122 BPM in A dorian)

| File | Bars | What it is |
|---|---|---|
| `dusty-house-122.mid` | 1 | Dusty House 122 (drums, bass, lead) |
| `rolling-acid-136.mid` | 2 | Rolling Acid 136 (drums, bass) |

### Lo-fi hip-hop / boom-bap

*80-95 BPM, minor or dorian, swing 20-40 %*

Lazy, behind-the-beat. Kick on 1 and a syncopated second kick, snare on 2 and 4, eighth-note hats with a few missing. Bass plays long round notes following the kick. Keys play two- to four-note jazzy fragments (thirds and sevenths of the scale) with rests.

**Dusty Vinyl 84** (written at 84 BPM in F minor)

| File | Bars | What it is |
|---|---|---|
| `dusty-vinyl-84-boombap-dilla-swing.mid` | 1 | BoomBap Dilla Swing (drums, bass, lead, syn3, syn4) |
| `dusty-vinyl-84-boombap-ghost-snare-fill.mid` | 1 | BoomBap Ghost Snare Fill (drums, bass, lead, syn3, syn4) |
| `dusty-vinyl-84-lofi-chill-breakdown.mid` | 1 | LoFi Chill Breakdown (drums, bass, lead, syn3, syn4) |
| `dusty-vinyl-84-lofi-minimal-dusty-verse.mid` | 1 | LoFi Minimal Dusty Verse (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### Hard techno / schranz / gabber

*145-190 BPM, minor*

Relentless. Kick on every beat, loud (accent every hit); often an extra kick on the last sixteenth of a bar. Off-beat open hat or ride, clap on 2 and 4 or every beat. Bass, if any, sits between the kicks (off-beat eighths, 'rumble'). Leads are one- or two-note hoover/screech stabs, very repetitive.

**Iron Foundry 152** (written at 152 BPM in F minor)

| File | Bars | What it is |
|---|---|---|
| `iron-foundry-152-gabber-kick-rolls.mid` | 1 | Gabber Kick Rolls (drums, bass, lead, syn3, syn4) |
| `iron-foundry-152-industrial-break-snare-rush.mid` | 1 | Industrial Break Snare Rush (drums, bass, lead, syn3, syn4) |
| `iron-foundry-152-schranz-16th-perc-shuffle.mid` | 1 | Schranz 16th Perc Shuffle (drums, bass, lead, syn3, syn4) |
| `iron-foundry-152-schranz-main-drive.mid` | 1 | Schranz Main Drive (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### Synthwave / retrowave

*90-118 BPM, minor or major, straight*

Eighties drum machine: kick on 1 and 3 (or four-on-the-floor), big snare on 2 and 4, straight eighth or sixteenth hats, tom fills at the end of a 4-bar phrase. Bass is a steady eighth-note or sixteenth-note pulse on the root that follows a 4-chord progression (one chord per bar). Lead plays a singable melody of long notes; arpeggios run in sixteenths.

**Neon Horizon 108** (written at 108 BPM in A minor)

| File | Bars | What it is |
|---|---|---|
| `neon-horizon-108-neon-atmospheric-break.mid` | 1 | Neon Atmospheric Break (drums, bass, lead, syn3, syn4) |
| `neon-horizon-108-outrun-80s-main.mid` | 1 | Outrun 80s Main (drums, bass, lead, syn3, syn4) |
| `neon-horizon-108-retrowave-80s-tom-fill.mid` | 1 | Retrowave 80s Tom Fill (drums, bass, lead, syn3, syn4) |
| `neon-horizon-108-synthwave-driving-half.mid` | 1 | Synthwave Driving Half (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A A B B C C D C (A to D in the order of the table).

**Neon Skyline 106** (written at 106 BPM in A minor)

| File | Bars | What it is |
|---|---|---|
| `neon-skyline-106-retrowave-halftime-intro.mid` | 1 | Retrowave HalfTime Intro (drums, bass, lead, syn3, syn4) |
| `neon-skyline-106-synthwave-80s-gated-4x4.mid` | 1 | Synthwave 80s Gated 4x4 (drums, bass, lead, syn3, syn4) |
| `neon-skyline-106-synthwave-80s-rototom-fill.mid` | 1 | Synthwave 80s Rototom Fill (drums, bass, lead, syn3, syn4) |
| `neon-skyline-106-synthwave-driving-16th-hat.mid` | 1 | Synthwave Driving 16th Hat (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### Neurofunk / techstep drum and bass

*170-176 BPM, minor or phrygian*

Two-step drums: kick on 1 and the 'and' of 3 (steps 0 and 10), hard snare on 2 and 4, tight sixteenth hats and ghost notes. The bass is the lead: syncopated mid-range growl phrases that answer the snare, with slides and rests; a sub note underneath. Dark, mechanical, precise.

**Neuro Spine 174** (written at 174 BPM in F minor)

| File | Bars | What it is |
|---|---|---|
| `neuro-spine-174-neuro-ghost-shuffle.mid` | 1 | Neuro Ghost Shuffle (drums, bass, lead, syn3, syn4) |
| `neuro-spine-174-neuro-turnaround-fill.mid` | 1 | Neuro Turnaround Fill (drums, bass, lead, syn3, syn4) |
| `neuro-spine-174-neuro-twostep-main.mid` | 1 | Neuro TwoStep Main (drums, bass, lead, syn3, syn4) |
| `neuro-spine-174-techstep-minimal-roll.mid` | 1 | Techstep Minimal Roll (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

**Neuro Vortex 174** (written at 174 BPM in F minor)

| File | Bars | What it is |
|---|---|---|
| `neuro-vortex-174-neuro-ghost-shuffle.mid` | 1 | Neuro Ghost Shuffle (drums, bass, lead, syn3, syn4) |
| `neuro-vortex-174-neuro-sinister-breakdown.mid` | 1 | Neuro Sinister Breakdown (drums, bass, lead, syn3, syn4) |
| `neuro-vortex-174-neuro-twostep-crack.mid` | 1 | Neuro TwoStep Crack (drums, bass, lead, syn3, syn4) |
| `neuro-vortex-174-techstep-call-response-fill.mid` | 1 | Techstep Call Response Fill (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### Ambient / Berlin school

*90-125 BPM, minor, dorian or major*

Little or no drums (a soft kick or a hat pulse at most). A sequencer line of eighth or sixteenth notes that repeats and slowly changes one note at a time; long pad notes of 4 to 16 steps underneath; much space.

**Potsdam Sequencer 95** (written at 95 BPM in D minor)

| File | Bars | What it is |
|---|---|---|
| `potsdam-sequencer-95-ambient-sparse-texture.mid` | 1 | Ambient Sparse Texture (drums, bass, lead, syn3, syn4) |
| `potsdam-sequencer-95-berlin-industrial-clock.mid` | 1 | Berlin Industrial Clock (drums, bass, lead, syn3, syn4) |
| `potsdam-sequencer-95-berlin-quiet-heartbeat.mid` | 1 | Berlin Quiet Heartbeat (drums, bass, lead, syn3, syn4) |
| `potsdam-sequencer-95-berlin-subtle-pulse.mid` | 1 | Berlin Subtle Pulse (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

### Deep / classic house

*118-126 BPM, minor or dorian, a little swing*

Four-on-the-floor kick, off-beat open hat, clap on 2 and 4, shuffled closed hats. Bass is a short, bouncy off-beat figure of 2 to 4 notes per bar. Chord stabs (organ or piano style) on the off-beats; one chord for one or two bars.

**Velvet Garage 124** (written at 124 BPM in A dorian)

| File | Bars | What it is |
|---|---|---|
| `velvet-garage-124-classic-house-skippy-909.mid` | 1 | Classic House Skippy 909 (drums, bass, lead, syn3, syn4) |
| `velvet-garage-124-deep-house-shaker-swing.mid` | 1 | Deep House Shaker Swing (drums, bass, lead, syn3, syn4) |
| `velvet-garage-124-house-filter-break.mid` | 1 | House Filter Break (drums, bass, lead, syn3, syn4) |
| `velvet-garage-124-house-jackin-fill.mid` | 1 | House Jackin Fill (drums, bass, lead, syn3, syn4) |

Suggested order of the four in a song: A B C C D C (A to D in the order of the table).

