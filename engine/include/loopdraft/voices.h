// Loop Draft engine - drum sampler and polyphonic synth.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>

#include "loopdraft/dsp.h"
#include "loopdraft/types.h"

namespace ld {

// Decoded audio held fully in RAM (no disk access while playing).
struct Sample {
  std::vector<float> l, r;     // r is empty for mono
  double rate = 48000.0;
  size_t frames() const { return l.size(); }
};

// Built-in synthesized drum sounds, used until the user loads their own WAVs. pad = 0..7.
std::unique_ptr<Sample> makeBuiltinSample(int pad, double sampleRate);
std::unique_ptr<Sample> makeDrumKitSample(DrumEngine engine, int pad, double sampleRate);
std::unique_ptr<Sample> makeCustomDrumSample(const DrumSynthParams& d, double sampleRate);
// Built-in 1-bar Amen-style Jungle breakbeat for instant breakbeat slicing.
std::unique_ptr<Sample> makeJungleBreakSample(double sampleRate);
// Reads a WAV file (8/16/24/32-bit PCM or float). Returns nullptr if it cannot be read. Never call on the audio thread.
std::unique_ptr<Sample> loadWav(const char* path);
bool writeWav16(const char* path, const float* l, const float* r, size_t frames, double sampleRate);

class Sampler {
 public:
  void init(double sr);
  void trigger(int pad, const PadParams& p, const Sample* s, int vel);
  void stopPad(int pad);                       // hard stop, used before a sample is swapped out
  void process(float* l, float* r, int n);     // adds into l/r
 private:
  struct Voice {
    const Sample* s = nullptr;
    double pos = 0, inc = 1, endPos = 0;
    float gl = 0, gr = 0, env = 0, envMul = 1;
    int pad = -1, choke = 0;
    uint64_t age = 0;
    bool active = false, reverse = false;
  };
  Voice v_[kDrumVoices];
  double sr_ = 48000.0;
  float chokeMul_ = 0.9f;
  uint64_t clock_ = 0;
};

class Synth {
 public:
  void init(double sr);
  // gateFrames < 0 holds the note until noteOff(tag). tag identifies a held note (the scale degree).
  // slide: if a note of this synth is still sounding, it glides to the new pitch instead of starting again.
  void noteOn(int midi, int vel, int64_t gateFrames, int voicesInChord, int tag, bool slide = false);
  void noteOff(int tag);
  void allNotesOff();
  void process(float* out, int n, const SynthParams& p);   // adds into out (mono)
 private:
  enum Stage : uint8_t { Off, Attack, Decay, Release };
  struct Voice {
    Stage stage = Off;
    int tag = 0;
    float freq = 0, ph1 = 0, ph2 = 0, inc1 = 0, inc2 = 0;
    float env = 0, gain = 0, lfoPh = 0, amp = 1, ampStep = 0, cut = 0, modEnv = 1.0f, lastMod = 0.0f;
    float target = 0;                      // pitch a slide is heading for
    float acc = 0;                         // accent: extra filter opening, fades quickly
    float ph3 = 0, ph4 = 0, inc3 = 0, inc4 = 0, vx = -1, vy = -1;   // Vector: FM modulator, sub, smoothed position
    float ksDelay = 0, ksLast = 0, ksGain = 0; int ksPos = 0; bool ksReady = false;   // Pluck string
    int64_t gate = 0;
    uint64_t age = 0, born = 0;
    Svf flt;
  };
  Voice v_[kSynthVoices];
  static constexpr int kStringLen = 4096;  // one delay line per voice for the Pluck engine, allocated in init()
  std::vector<float> strings_;
  double sr_ = 48000.0;
  uint64_t clock_ = 0;
};

}  // namespace ld
