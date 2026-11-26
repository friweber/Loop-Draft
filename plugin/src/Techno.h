// The techno database (content/techno): ten genres, each with drum patterns for every part of a song and synth
// patterns for every role, and three starter songs. The MIDI files travel inside the plugin as one block
// (content/techno/pack, made by make_pack.py); this file reads them, and sets a project up for a genre:
// tempo, key, a drum kit, four sounds, pedals, and - if wanted - the patterns stacked into a whole song.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <loopdraft/content.h>
#include <loopdraft/session.h>
#include "TechnoData.h"

namespace techno {

struct Genre { juce::String name, vibe, sounds, pedals; float bpm = 130.0f; int root = 0, scale = 0; };
struct Pattern { int genre = 0; bool drum = false; int cat = 0; juce::String title, about; int bars = 1, energy = 0, off = 0, size = 0; };
struct SongScene { juce::String title; int off = 0, size = 0; };
struct Song { juce::String title, chain; float bpm = 130.0f; int root = 0, scale = 0, genre = 0; std::vector<SongScene> scenes; };
struct Pack {
  std::vector<Genre> genres;
  std::vector<Pattern> patterns;
  std::vector<Song> songs;
  const char* blob = nullptr;
  int blobSize = 0;
};

inline const char* const kDrumCats[6] = {"INTRO", "GROOVE", "PEAK", "BREAK", "FILL", "OUTRO"};
inline const char* const kSynthCats[5] = {"BASS", "LEAD", "ARP", "CHORDS", "PAD"};

inline const Pack& pack() {
  static const Pack pk = [] {
    Pack k;
    k.blob = TechnoData::techno_pack;
    k.blobSize = TechnoData::techno_packSize;
    const juce::var v = juce::JSON::parse(juce::String::fromUTF8(TechnoData::techno_json, TechnoData::techno_jsonSize));
    if (const auto* gs = v["genres"].getArray())
      for (const auto& g : *gs) {
        Genre x;
        x.name = g["name"].toString(); x.vibe = g["vibe"].toString(); x.sounds = g["sounds"].toString(); x.pedals = g["pedals"].toString();
        x.bpm = float(double(g["bpm"])); x.root = int(g["root"]); x.scale = int(g["scale"]);
        k.genres.push_back(x);
      }
    const auto inside = [&k](int off, int size) { return off >= 0 && size > 0 && off + size <= k.blobSize; };
    if (const auto* ps = v["patterns"].getArray())
      for (const auto& q : *ps) {
        Pattern x;
        x.genre = int(q["g"]); x.drum = bool(q["drum"]); x.cat = int(q["cat"]); x.title = q["title"].toString(); x.about = q["about"].toString();
        x.bars = int(q["bars"]); x.energy = int(q["energy"]); x.off = int(q["off"]); x.size = int(q["size"]);
        if (inside(x.off, x.size) && x.genre >= 0 && x.genre < int(k.genres.size())) k.patterns.push_back(x);
      }
    if (const auto* ss = v["songs"].getArray())
      for (const auto& s : *ss) {
        Song x;
        x.title = s["title"].toString(); x.chain = s["chain"].toString(); x.bpm = float(double(s["bpm"])); x.root = int(s["root"]); x.scale = int(s["scale"]);
        if (const auto* gg = s["genres"].getArray()) if (!gg->isEmpty()) x.genre = int((*gg)[0]);
        if (const auto* sc = s["scenes"].getArray())
          for (const auto& c : *sc) { SongScene y; y.title = c["title"].toString(); y.off = int(c["off"]); y.size = int(c["size"]); if (inside(y.off, y.size)) x.scenes.push_back(y); }
        k.songs.push_back(x);
      }
    return k;
  }();
  return pk;
}

// ---- a MIDI file as a pattern ----
// part 0: every note becomes a drum hit (General MIDI drum notes, or the pads' own notes).
// part 1..: the notes go to that synth, moved to the nearest notes of the project's scale.
inline bool midiToScene(juce::InputStream& in, const ld::Project& pr, int part, ld::Scene& out) {
  juce::MidiFile mf;
  if (!mf.readFrom(in)) return false;
  const int tpq = mf.getTimeFormat();
  if (tpq <= 0) return false;                                // SMPTE time: no bars to go by
  const double perStep = double(tpq) / 4.0;
  out = ld::Scene();
  int last = -1;
  bool tenOnly = false;
  if (part == 0)                                             // a file with a drum channel: only that channel is drums
    for (int t = 0; t < mf.getNumTracks() && !tenOnly; ++t)
      for (const auto* h : *mf.getTrack(t)) if (h->message.isNoteOn() && h->message.getChannel() == 10) { tenOnly = true; break; }
  bool ownChannel = false;                                   // a file with notes on this track's channel: only those
  if (part > 0)
    for (int t = 0; t < mf.getNumTracks() && !ownChannel; ++t)
      for (const auto* h : *mf.getTrack(t)) if (h->message.isNoteOn() && h->message.getChannel() == part) { ownChannel = true; break; }
  for (int t = 0; t < mf.getNumTracks(); ++t) {
    for (const auto* h : *mf.getTrack(t)) {
      const juce::MidiMessage& m = h->message;
      if (!m.isNoteOn()) continue;
      const int step = int(std::floor(m.getTimeStamp() / perStep + 0.5));
      if (step < 0 || step >= ld::kMaxSteps) continue;
      const int note = m.getNoteNumber();
      const bool strong = m.getVelocity() >= 100;
      if (part == 0) {
        if (tenOnly && m.getChannel() != 10) continue;
        int pad = -1;
        for (int i = 0; i < ld::kNumPads && pad < 0; ++i) if (int(pr.pads[i].gmNote) == note) pad = i;
        if (pad < 0) {
          switch (note) {
            case 35: case 36: pad = 0; break;
            case 38: case 40: pad = 1; break;
            case 39: pad = 2; break;
            case 42: case 44: pad = 3; break;
            case 46: case 49: case 51: case 52: case 55: case 57: case 59: pad = 4; break;
            case 37: pad = 5; break;
            case 41: case 43: case 45: case 47: case 48: case 50: pad = 6; break;
            default: pad = 7; break;
          }
        }
        if (out.drums.steps[pad][step] < 2) out.drums.steps[pad][step] = strong ? 2 : 1;
      } else {
        if (m.getChannel() == 10 || (ownChannel && m.getChannel() != part)) continue;
        ld::SynthPattern& pat = out.synth[part - 1];
        if (pat.count >= ld::kMaxNotes) continue;
        const int deg = ld::midiToDegree(pr, part - 1, note);
        bool twice = false;
        for (int i = 0; i < pat.count && !twice; ++i) twice = pat.notes[i].step == step && pat.notes[i].degree == deg;
        if (twice) continue;
        ld::Note n;
        n.step = uint16_t(step); n.degree = int8_t(juce::jlimit(-21, 35, deg)); n.vel = strong ? 2 : 1;
        const double end = h->noteOffObject ? h->noteOffObject->message.getTimeStamp() : m.getTimeStamp() + perStep;
        n.len = uint8_t(juce::jlimit(1, 255, int(std::floor((end - m.getTimeStamp()) / perStep + 0.5))));
        pat.notes[pat.count++] = n;
      }
      last = juce::jmax(last, step);
    }
  }
  if (last < 0) return false;
  out.bars = juce::jlimit(1, ld::kMaxBars, last / 16 + 1);
  return true;
}

inline bool blockToScene(int off, int size, const ld::Project& pr, int part, ld::Scene& out) {
  const Pack& k = pack();
  if (off < 0 || size <= 0 || off + size > k.blobSize) return false;
  juce::MemoryInputStream in(k.blob + off, size_t(size), false);
  return midiToScene(in, pr, part, out);
}

// A pattern that is shorter than its groove plays again and again until the groove is full.
inline void tileDrums(ld::DrumPattern& d, int from, int to) {
  for (auto& row : d.steps) for (int st = from * 16; st < to * 16 && st < ld::kMaxSteps; ++st) row[st] = row[st % (from * 16)];
}
inline void tileSynth(ld::SynthPattern& s, int from, int to) {
  const int n = s.count;
  for (int rep = 1; rep * from < to; ++rep)
    for (int i = 0; i < n && s.count < ld::kMaxNotes; ++i) {
      ld::Note x = s.notes[i];
      const int st = int(x.step) + rep * from * 16;
      if (st >= to * 16) continue;
      x.step = uint16_t(st);
      s.notes[s.count++] = x;
    }
}

// ---- what a genre sounds like here: a kit, four sounds and pedals from the library and the stompbox collection ----
struct Stomp { int type; float amount, rate, mix; int flags; };            // flags: 1 keeps the lows clean, 2 runs in time
struct Setup {
  const char* kit;
  const char* inst[4];                                                    // bass, lead, chords, pad
  Stomp fx[5][2];                                                         // drums, then the four synths
  const char* why;
};
inline const Setup kSetups[10] = {
  {"DR - Warehouse Rumble", {"BS - Dusty Lo-Fi Sub", "PL - Berlin Seq Pluck", "KY - Dub Techno Chord", "PD - Sci-Fi Neuro Morph"},
   {{{22, 0.5f, 0.4f, 1.0f, 0}, {20, 0.45f, 0.5f, 0.14f, 0}}, {{1, 0.45f, 0.35f, 0.4f, 1}, {0, 0, 0, 0, 0}}, {{29, 0.5f, 0.45f, 0.35f, 2}, {20, 0.5f, 0.55f, 0.28f, 0}},
    {{4, 0.4f, 0.2f, 0.4f, 0}, {20, 0.55f, 0.6f, 0.38f, 0}}, {{27, 0.5f, 0.4f, 0.4f, 0}, {20, 0.6f, 0.7f, 0.5f, 0}}},
   "A rumbling kick, a dull sub, bleeps in a dub delay, everything in a big dark room."},
  {"DR - 909 Acid Warehouse", {"BS - Acid 303 Squelch", "LD - Acid Square Bite", "KY - Rave Organ Stab", "PD - Warm Tape Bed"},
   {{{22, 0.55f, 0.4f, 1.0f, 0}, {23, 0.35f, 0.5f, 0.18f, 0}}, {{7, 0.55f, 0.5f, 0.5f, 1}, {18, 0.4f, 0.45f, 0.2f, 2}}, {{4, 0.5f, 0.3f, 0.45f, 0}, {19, 0.45f, 0.45f, 0.3f, 2}},
    {{20, 0.5f, 0.5f, 0.35f, 0}, {0, 0, 0, 0, 0}}, {{3, 0.5f, 0.3f, 0.5f, 0}, {20, 0.6f, 0.65f, 0.5f, 0}}},
   "A 909, a squelching 303 through distortion with its lows left alone, rave stabs."},
  {"DR - Gabber Assault", {"BS - FM Neuro Growl", "LD - Unison Hoover Stack", "KY - Rave Organ Stab", "PD - Industrial Clang FX"},
   {{{7, 0.55f, 0.45f, 0.5f, 1}, {25, 0.5f, 0.4f, 0.3f, 0}}, {{8, 0.55f, 0.5f, 0.55f, 1}, {0, 0, 0, 0, 0}}, {{17, 0.4f, 0.3f, 0.3f, 0}, {20, 0.5f, 0.5f, 0.3f, 0}},
    {{23, 0.5f, 0.5f, 0.4f, 0}, {25, 0.5f, 0.45f, 0.3f, 0}}, {{30, 0.4f, 0.2f, 0.4f, 0}, {20, 0.6f, 0.7f, 0.5f, 0}}},
   "A distorted kick with a gated room, a growling bass in fuzz, metal and noise."},
  {"DR - Synth Dub Echo", {"BS - Drawbar Sub Organ", "PL - Berlin Seq Pluck", "KY - Dub Techno Chord", "PD - FM Submarine Sonar"},
   {{{28, 0.45f, 0.4f, 0.4f, 1}, {29, 0.45f, 0.45f, 0.18f, 2}}, {{6, 0.5f, 0.4f, 1.0f, 0}, {0, 0, 0, 0, 0}}, {{29, 0.55f, 0.45f, 0.5f, 2}, {20, 0.6f, 0.7f, 0.45f, 0}},
    {{29, 0.6f, 0.45f, 0.55f, 2}, {32, 0.45f, 0.5f, 0.3f, 0}}, {{28, 0.5f, 0.35f, 0.5f, 0}, {20, 0.65f, 0.8f, 0.6f, 0}}},
   "Soft drums on tape, a pure sub, chords that disappear into a dub delay and a long reverb."},
  {"DR - 808 Miami Electro", {"BS - DX7 Lately Bass", "PL - Crystalline FM Harp", "KY - Juno Poly Brass", "PD - Solina Horizon"},
   {{{22, 0.5f, 0.4f, 1.0f, 0}, {2, 0.3f, 0.5f, 0.2f, 0}}, {{1, 0.4f, 0.5f, 0.3f, 1}, {0, 0, 0, 0, 0}}, {{3, 0.5f, 0.35f, 0.45f, 0}, {18, 0.45f, 0.45f, 0.3f, 2}},
    {{4, 0.45f, 0.25f, 0.4f, 0}, {20, 0.5f, 0.5f, 0.3f, 0}}, {{3, 0.55f, 0.3f, 0.5f, 0}, {20, 0.6f, 0.7f, 0.5f, 0}}},
   "An 808, an FM bass, glassy bells, strings from a string machine."},
  {"DR - Synth Minimal Micro", {"BS - FM Donk Bounce", "PL - IDM Micro Glitch", "KY - Phase Square Clav", "PD - Choir Glass Vector"},
   {{{22, 0.4f, 0.4f, 1.0f, 0}, {0, 0, 0, 0, 0}}, {{6, 0.5f, 0.4f, 1.0f, 0}, {0, 0, 0, 0, 0}}, {{15, 0.5f, 0.4f, 0.4f, 0}, {18, 0.4f, 0.45f, 0.25f, 2}},
    {{10, 0.5f, 0.5f, 0.5f, 2}, {20, 0.4f, 0.4f, 0.22f, 0}}, {{27, 0.45f, 0.4f, 0.35f, 0}, {0, 0, 0, 0, 0}}},
   "Small dry drums, a bouncing bass, clicks and short chords with a lot of air between them."},
  {"DR - Deep House 909", {"BS - Phase Resonant Digital", "LD - Unison Supersaw Anthem", "KY - Juno Poly Brass", "PD - Unison String Ensemble"},
   {{{22, 0.5f, 0.4f, 1.0f, 0}, {20, 0.45f, 0.5f, 0.14f, 0}}, {{1, 0.4f, 0.5f, 0.3f, 1}, {0, 0, 0, 0, 0}}, {{18, 0.5f, 0.45f, 0.35f, 2}, {32, 0.5f, 0.5f, 0.35f, 0}},
    {{3, 0.5f, 0.3f, 0.5f, 0}, {20, 0.6f, 0.65f, 0.45f, 0}}, {{32, 0.5f, 0.5f, 0.5f, 0}, {20, 0.65f, 0.8f, 0.5f, 0}}},
   "A round 909, a supersaw lead in delay and shimmer, wide strings for the big moment."},
  {"DR - EBM Body Machine", {"BS - Buchla Wavefold", "LD - Dark Electro Cut", "KY - FM Clavinet Funk", "PD - Warm Tape Bed"},
   {{{7, 0.4f, 0.45f, 0.35f, 1}, {25, 0.5f, 0.4f, 0.35f, 0}}, {{7, 0.5f, 0.5f, 0.5f, 1}, {3, 0.45f, 0.3f, 0.3f, 0}}, {{12, 0.5f, 0.3f, 0.4f, 0}, {18, 0.4f, 0.45f, 0.25f, 2}},
    {{2, 0.35f, 0.5f, 0.3f, 0}, {20, 0.5f, 0.5f, 0.3f, 0}}, {{4, 0.45f, 0.2f, 0.4f, 0}, {20, 0.6f, 0.65f, 0.45f, 0}}},
   "Hard drums in a gated room, a driving sequenced bass with chorus, cold leads."},
  {"Warehouse Hard", {"Rubber Acid", "LD - VHS Wobble Brass", "KY - Drawbar House Organ", "PD - Solina Horizon"},
   {{{22, 0.55f, 0.4f, 1.0f, 0}, {1, 0.45f, 0.5f, 0.35f, 1}}, {{1, 0.45f, 0.45f, 0.4f, 1}, {0, 0, 0, 0, 0}}, {{15, 0.5f, 0.45f, 0.5f, 0}, {18, 0.4f, 0.45f, 0.25f, 2}},
    {{13, 0.5f, 0.5f, 0.5f, 0}, {20, 0.45f, 0.45f, 0.3f, 0}}, {{4, 0.45f, 0.2f, 0.45f, 0}, {20, 0.55f, 0.6f, 0.4f, 0}}},
   "Driven drums with toms and percussion up front, a rubber bass, an organ through a rotary speaker."},
  {"DR - Neuro Scalpel", {"BS - FM Neuro Growl", "LD - Vector Laser Lead", "PL - Vector Cyber Arp", "PD - Shimmer Fifth Pad"},
   {{{22, 0.55f, 0.4f, 1.0f, 0}, {23, 0.3f, 0.5f, 0.15f, 0}}, {{6, 0.55f, 0.4f, 1.0f, 0}, {0, 0, 0, 0, 0}}, {{12, 0.5f, 0.3f, 0.45f, 0}, {29, 0.5f, 0.45f, 0.4f, 2}},
    {{31, 0.45f, 0.5f, 0.4f, 0}, {20, 0.5f, 0.5f, 0.3f, 0}}, {{30, 0.35f, 0.2f, 0.3f, 0}, {27, 0.5f, 0.4f, 0.45f, 0}}},
   "A tight kick, a rolling bass kept clean and compressed, lasers and arps in flanger and dub delay."},
};

// ---- which patterns make which part of the song ----
// Six grooves: A intro, B main groove, C break, D peak, E build, F outro. Per groove: the drum section and, for each
// synth, the role it plays there (-1: silent) and which of that role's patterns (the bass has two).
struct Part { int drums; int role[4]; int which[4]; };
inline const Part kParts[6] = {
  {0, {-1, 2, -1, 4}, {0, 0, 0, 0}},        // A intro: the arp and the pad
  {1, {0, 2, -1, -1}, {0, 0, 0, 0}},        // B groove: bass and arp
  {3, {-1, 1, 3, 4}, {0, 0, 0, 0}},         // C break: lead, chords and pad, no bass
  {2, {0, 1, 3, 4}, {1, 0, 0, 0}},          // D peak: the second bass, the lead, chords, pad
  {4, {0, 2, 3, -1}, {0, 0, 0, 0}},         // E build: bass, arp, chords
  {5, {0, -1, -1, 4}, {0, 0, 0, 0}},        // F outro: bass and pad
};
inline const int kSongChain[12] = {0, 1, 1, 4, 2, 3, 3, 1, 4, 3, 3, 5};      // A B B E C D D B E D D F

inline const Pattern* findPattern(int genre, bool drum, int cat, int which) {
  const Pattern* first = nullptr;
  int n = 0;
  for (const Pattern& x : pack().patterns)
    if (x.genre == genre && x.drum == drum && x.cat == cat) { if (!first) first = &x; if (n++ == which) return &x; }
  return first;
}
}  // namespace techno
