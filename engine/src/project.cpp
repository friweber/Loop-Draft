#include "loopdraft/types.h"

#include <initializer_list>

namespace ld {

namespace {
const int kMinor[] = {0, 2, 3, 5, 7, 8, 10};
const int kMajor[] = {0, 2, 4, 5, 7, 9, 11};
const int kDorian[] = {0, 2, 3, 5, 7, 9, 10};
const int kPenta[] = {0, 3, 5, 7, 10};
const int* intervals(Scale s) {
  switch (s) {
    case Scale::Major: return kMajor;
    case Scale::Dorian: return kDorian;
    case Scale::Pentatonic: return kPenta;
    default: return kMinor;
  }
}
}  // namespace

int scaleSize(Scale s) { return s == Scale::Pentatonic ? 5 : 7; }

int degreeToMidi(const Project& p, int octaveShift, int degree) {
  const int n = scaleSize(p.scale);
  int oct = degree / n, idx = degree % n;
  if (idx < 0) { idx += n; --oct; }
  int m = 12 * (p.octave + octaveShift + 1) + p.root + 12 * oct + intervals(p.scale)[idx];
  return m < 0 ? 0 : (m > 127 ? 127 : m);
}

int chordMidis(const Project& p, const SynthParams& sp, int degree, int out[3]) {
  out[0] = degreeToMidi(p, sp.octaveShift, degree);
  if (!sp.chord) return 1;
  out[1] = degreeToMidi(p, sp.octaveShift, degree + 2);   // stacked thirds inside the scale
  out[2] = degreeToMidi(p, sp.octaveShift, degree + 4);
  return 3;
}

bool addNote(SynthPattern& p, int step, int degree, int len, int vel) {
  if (step < 0 || step >= kMaxSteps || p.count >= kMaxNotes) return false;
  for (int i = 0; i < p.count; ++i)
    if (p.notes[i].step == step && p.notes[i].degree == degree) {
      p.notes[i].len = uint8_t(len < 1 ? 1 : len); p.notes[i].vel = uint8_t(vel);
      return true;
    }
  Note& n = p.notes[p.count++];
  n.step = uint16_t(step); n.degree = int8_t(degree); n.len = uint8_t(len < 1 ? 1 : len); n.vel = uint8_t(vel);
  return true;
}

bool removeNote(SynthPattern& p, int step, int degree) {
  for (int i = 0; i < p.count; ++i)
    if (p.notes[i].step == step && p.notes[i].degree == degree) {
      p.notes[i] = p.notes[--p.count];
      return true;
    }
  return false;
}

Project makeDemoProject() {
  Project p;
  struct PadDef { float vol, decay; uint8_t choke, gm; };
  //                       Kick             Snare            Clap             CH hat           OH hat           Rim              Tom              Perc
  const PadDef pads[8] = {{.95f,.35f,0,36},{.85f,.22f,0,38},{.80f,.20f,0,39},{.70f,.08f,1,42},{.65f,.38f,1,46},{.75f,.10f,0,37},{.80f,.28f,2,45},{.70f,.16f,2,64}};
  for (int i = 0; i < kNumPads; ++i) {
    p.pads[i].vol = pads[i].vol; p.pads[i].decay = pads[i].decay; p.pads[i].choke = pads[i].choke; p.pads[i].gmNote = pads[i].gm;
  }

  SynthParams& bass = p.synths[0];
  bass.wave1 = Wave::Saw; bass.wave2 = Wave::Square; bass.detune = 5; bass.cutoff = 600; bass.res = 2.0f;
  bass.attack = 0.004f; bass.decay = 0.25f; bass.sustain = 0.55f; bass.release = 0.06f; bass.level = 0.9f;
  bass.lfoDepth = 0; bass.lfoTarget = LfoTarget::Off; bass.octaveShift = -1;
  // synths[1] keeps the defaults: the v0.2 lead sound.

  p.fx[0] = TrackFx{0.62f, 0.08f, 2.0f, 1.0f, 0.05f, 0.10f, false};
  p.fx[1] = TrackFx{0.85f, 0.15f, 1.0f, 0.0f, 0.00f, 0.03f, false};
  p.fx[2] = TrackFx{0.80f, 0.15f, 0.0f, 1.0f, 0.30f, 0.25f, false};
  p.fx[3] = TrackFx{0.75f, 0.05f, 0.0f, 0.5f, 0.20f, 0.30f, false};
  p.fx[4] = TrackFx{0.75f, 0.10f, 0.0f, 0.5f, 0.25f, 0.20f, false};
  p.synths[2].wave1 = Wave::Triangle; p.synths[2].wave2 = Wave::Sine; p.synths[2].attack = 0.12f; p.synths[2].sustain = 0.65f; p.synths[2].release = 0.35f; p.synths[2].cutoff = 1400.0f;
  p.synths[3].mode = SynthMode::Fm; p.synths[3].fmRatio = 3.0f; p.synths[3].fmIndex = 3.2f; p.synths[3].decay = 0.28f;

  auto d = [&](int sc, int pad, std::initializer_list<int> steps) { for (int s : steps) p.scenes[sc].drums.steps[pad][s] = 1; };
  auto n = [&](int sc, int syn, std::initializer_list<int> steps, std::initializer_list<int> degs, int len) {
    auto di = degs.begin();
    for (int s : steps) addNote(p.scenes[sc].synth[syn], s, *di++, len, 1);
  };
  d(0,0,{0,4,8,12}); d(0,3,{2,6,10,14});
  n(0,1,{0,3,6,10},{0,2,4,0},1);             n(0,0,{0,8},{0,0},3);
  d(1,0,{0,4,8,12,14}); d(1,1,{4,12}); d(1,3,{0,2,4,6,8,10,12,14});
  n(1,1,{0,3,6,8,11,14},{0,2,4,5,4,2},1);    n(1,0,{0,6,8,14},{0,0,3,4},2);
  d(2,0,{0,4,8,10,12}); d(2,1,{4,12}); d(2,2,{4,12}); d(2,3,{0,2,4,6,8,10,12,14}); d(2,4,{15});
  n(2,1,{0,2,4,6,8,10,12,14},{0,4,5,7,6,4,2,3},1); n(2,0,{0,3,6,8,11,14},{0,0,3,4,4,2},2);
  d(3,0,{0,8}); d(3,5,{6,14});
  n(3,1,{0,4,8,12},{5,4,2,0},1);             n(3,0,{0,8},{5,4},6);
  return p;
}

}  // namespace ld
