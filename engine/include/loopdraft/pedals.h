// The stompbox collection: fifteen more effects for the three slots of a track.
// One Pedal object per slot holds the state for whichever of them is in the slot. Two controls each, like the
// standard pedals: `a` (amount) and `r` (rate), both 0 .. 1.
//
// init() allocates (control thread, once). tick() runs on the audio thread and never allocates.
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

#include "loopdraft/dsp.h"
#include "loopdraft/types.h"

namespace ld {

struct Pedal {
  double sr = 48000.0;
  std::vector<float> bufL, bufR;        // delay line, a power of two long, a little over one second
  int mask = 0, w = 0;
  std::vector<float> comb[2][4], allp[2][2];
  int ci[2][4] = {}, ai[2][2] = {};
  float cl[2][4] = {};                  // damping inside the reverb's combs
  float ph = 0.0f, ph2 = 0.0f;          // LFOs, in turns
  float lp[2] = {}, lp2[2] = {}, env = 0.0f, gate = 1.0f, flip[2] = {1.0f, 1.0f}, prev[2] = {};
  float ic1[2] = {}, ic2[2] = {};       // state-variable filter
  float dTime = -1.0f;                  // delay time in samples, gliding to where the control points
  int last = -1;                        // the pedal that used this state last
  float lastA = -1.0f, lastR = -1.0f, k1 = 0.0f, k2 = 0.0f;   // values worked out from the controls

  void init(double sampleRate) {
    sr = sampleRate;
    int n = 1;
    while (n < int(sr * 1.1)) n <<= 1;
    bufL.assign(size_t(n), 0.0f); bufR.assign(size_t(n), 0.0f);
    mask = n - 1; w = 0;
    static const double combMs[4] = {29.7, 37.1, 41.1, 43.7};
    static const double allpMs[2] = {5.0, 1.7};
    for (int c = 0; c < 2; ++c) {
      for (int i = 0; i < 4; ++i) comb[c][i].assign(size_t((combMs[i] + 0.7 * c) * 0.001 * sr) + 2, 0.0f);
      for (int i = 0; i < 2; ++i) allp[c][i].assign(size_t((allpMs[i] + 0.3 * c) * 0.001 * sr) + 2, 0.0f);
    }
    reset();
    last = -1;
  }

  // Back to silence. Only when another pedal takes the slot over, so it starts without the old one's echoes.
  void reset() {
    std::memset(bufL.data(), 0, bufL.size() * sizeof(float));
    std::memset(bufR.data(), 0, bufR.size() * sizeof(float));
    for (int c = 0; c < 2; ++c) {
      for (int i = 0; i < 4; ++i) { std::memset(comb[c][i].data(), 0, comb[c][i].size() * sizeof(float)); ci[c][i] = 0; cl[c][i] = 0.0f; }
      for (int i = 0; i < 2; ++i) { std::memset(allp[c][i].data(), 0, allp[c][i].size() * sizeof(float)); ai[c][i] = 0; }
      lp[c] = lp2[c] = prev[c] = ic1[c] = ic2[c] = 0.0f; flip[c] = 1.0f;
    }
    ph = ph2 = env = 0.0f; gate = 1.0f; dTime = -1.0f; w = 0;
    lastA = lastR = -1.0f;
  }

  // Does this pedal keep sounding for seconds after its input stopped?
  static bool rings(FxType t) { return t == FxType::Delay || t == FxType::TapeEcho || t == FxType::Reverb; }

  inline void tick(FxType type, float& l, float& r, float a, float rt) {
    if (int(type) != last) { reset(); last = int(type); }
    a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
    rt = rt < 0.0f ? 0.0f : (rt > 1.0f ? 1.0f : rt);
    const float fs = float(sr);
    const bool moved = a != lastA || rt != lastR;
    if (moved) { lastA = a; lastR = rt; }
    switch (type) {
      case FxType::Distortion: {                  // a: gain, r: tone
        if (moved) { k1 = 1.0f + a * a * 80.0f; k2 = onePole(700.0f + rt * rt * 9000.0f); }
        const float out = 0.8f - 0.3f * a;
        lp[0] += k2 * (hardClip(fastTanh(l * k1) * 1.4f) - lp[0]); l = lp[0] * out;
        lp[1] += k2 * (hardClip(fastTanh(r * k1) * 1.4f) - lp[1]); r = lp[1] * out;
        break;
      }
      case FxType::Fuzz: {                        // a: fuzz, r: tone. Sputters out when the note dies, like the real thing.
        if (moved) { k1 = 4.0f + a * a * 300.0f; k2 = onePole(500.0f + rt * 7000.0f); }
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.01f : 0.0008f);
        float g = env * 250.0f; g = g > 1.0f ? 1.0f : g; g *= g;
        const float bias = fastTanh(k1 * 0.08f);
        lp[0] += k2 * ((fastTanh(k1 * (l + 0.08f)) - bias) * g - lp[0]); l = lp[0] * 0.5f;
        lp[1] += k2 * ((fastTanh(k1 * (r + 0.08f)) - bias) * g - lp[1]); r = lp[1] * 0.5f;
        break;
      }
      case FxType::Booster: {                     // a: boost up to +12 dB, r: treble
        if (moved) { k1 = 1.0f + a * 3.0f; k2 = onePole(700.0f); }
        lp[0] += k2 * (l - lp[0]); lp[1] += k2 * (r - lp[1]);
        l = fastTanh((l + (l - lp[0]) * rt * 2.0f) * k1 * 0.8f) * 1.25f;
        r = fastTanh((r + (r - lp[1]) * rt * 2.0f) * k1 * 0.8f) * 1.25f;
        break;
      }
      case FxType::Tremolo: {                     // a: depth, r: speed
        advance(ph, (1.0f + rt * 13.0f) / fs);
        const float g = 1.0f - a * (0.5f - 0.5f * sinTurns(ph));
        l *= g; r *= g;
        break;
      }
      case FxType::Vibrato: {                     // a: depth, r: speed
        advance(ph, (2.0f + rt * 7.0f) / fs);
        write(l, r);
        const float d = (0.005f + 0.003f * a * sinTurns(ph)) * fs;
        l = read(bufL, d); r = read(bufR, d);
        break;
      }
      case FxType::Flanger: {                     // a: feedback, r: speed
        advance(ph, (0.1f + rt * rt * 4.0f) / fs);
        const float d = (0.001f + 0.0045f * (0.5f + 0.5f * sinTurns(ph))) * fs;
        const float wl = read(bufL, d), wr = read(bufR, d);
        write(l + wl * a * 0.75f, r + wr * a * 0.75f);
        l = (l + wl) * 0.7f; r = (r + wr) * 0.7f;
        break;
      }
      case FxType::Rotary: {                      // a: depth, r: speed. A turning speaker: pitch, level and side all move.
        advance(ph, (0.7f + rt * 6.0f) / fs);
        const float s = sinTurns(ph), c = sinTurns(ph + 0.25f);
        const float m = 0.5f * (l + r);
        write(m, m);
        const float wl = read(bufL, (0.0015f + 0.001f * a * s) * fs), wr = read(bufR, (0.0015f - 0.001f * a * s) * fs);
        l = wl * (1.0f - 0.45f * a * (0.5f + 0.5f * c));
        r = wr * (1.0f - 0.45f * a * (0.5f - 0.5f * c));
        break;
      }
      case FxType::Wah: {                         // a: depth, r: speed of the foot
        advance(ph, (0.3f + rt * rt * 6.0f) / fs);
        const float u = 0.5f + 0.5f * sinTurns(ph);
        const float g = svfG(350.0f + u * u * 1900.0f);
        l = l * (1.0f - a) + band(0, l, g, 0.25f) * a * 2.2f;
        r = r * (1.0f - a) + band(1, r, g, 0.25f) * a * 2.2f;
        break;
      }
      case FxType::AutoWah: {                     // a: sensitivity, r: sharpness. The louder the note, the further it opens.
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.004f : 0.00017f);
        float u = env * (2.0f + a * 14.0f); u = u > 1.0f ? 1.0f : u;
        const float g = svfG(300.0f + u * 2600.0f), k = 1.0f / (2.0f + rt * 8.0f);
        l = l * 0.25f + band(0, l, g, k) * 1.8f;
        r = r * 0.25f + band(1, r, g, k) * 1.8f;
        break;
      }
      case FxType::Octaver: {                     // a: level of the octave below, r: its tone
        if (moved) { k1 = onePole(300.0f); k2 = onePole(150.0f + rt * 500.0f); }
        l += sub(0, l) * a * 2.5f;
        r += sub(1, r) * a * 2.5f;
        break;
      }
      case FxType::RingMod: {                     // a: mix, r: pitch of the carrier, 30 Hz .. 2 kHz
        if (moved) k1 = 30.0f * std::pow(2000.0f / 30.0f, rt);
        advance(ph, k1 / fs);
        const float m = (1.0f - a) + a * sinTurns(ph);
        l *= m; r *= m;
        break;
      }
      case FxType::Delay:                         // a: level and repeats, r: time, 60 .. 600 ms
      case FxType::TapeEcho: {                    // the same, with a tape's wobble, dull repeats and soft overload
        const float target = (0.06f + rt * 0.54f) * fs;
        if (dTime < 0.0f) dTime = target;
        dTime += (target - dTime) * 0.0004f;       // a moved knob bends the pitch for a moment, as on a real one
        float d = dTime;
        const bool tape = type == FxType::TapeEcho;
        if (tape) { advance(ph, 0.6f / fs); d += 0.0015f * fs * sinTurns(ph); }
        float wl = read(bufL, d), wr = read(bufR, d);
        const float fb = 0.25f + a * 0.45f;
        if (tape) {
          if (moved) k2 = onePole(2500.0f);
          lp[0] += k2 * (wl - lp[0]); lp[1] += k2 * (wr - lp[1]);
          wl = fastTanh(lp[0] * 1.2f); wr = fastTanh(lp[1] * 1.2f);
        }
        write(l + wl * fb, r + wr * fb);
        l += wl * a * 0.7f; r += wr * a * 0.7f;
        break;
      }
      case FxType::Reverb: {                      // a: level, r: size
        const float fb = 0.72f + rt * 0.2f;
        const float wl = verb(0, l, fb), wr = verb(1, r, fb);
        l += wl * a * 0.3f; r += wr * a * 0.3f;
        break;
      }
      case FxType::Gate: {                        // a: threshold, -60 .. -20 dB, r: how fast it closes
        if (moved) { k1 = 0.001f * std::pow(10.0f, a * 2.0f); k2 = 0.0002f + rt * 0.004f; }
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.05f : 0.001f);
        const float target = env > k1 ? 1.0f : 0.0f;
        gate += (target - gate) * (target > gate ? 0.02f : k2);
        l *= gate; r *= gate;
        break;
      }
      default: break;
    }
  }

 private:
  static float hardClip(float x) { return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x); }
  static void advance(float& phase, float by) { phase += by; if (phase >= 1.0f) phase -= 1.0f; }
  float onePole(float hz) const { const float c = 6.2831853f * hz / float(sr); return c > 1.0f ? 1.0f : c; }
  float svfG(float hz) const { const float g = 3.14159265f * hz / float(sr); return g > 0.9f ? 0.9f : g; }
  void write(float l, float r) { bufL[size_t(w)] = l; bufR[size_t(w)] = r; w = (w + 1) & mask; }
  // d samples back from the next write position, between two samples
  float read(const std::vector<float>& b, float d) const {
    if (d < 1.0f) d = 1.0f;
    const int di = int(d);
    const float fr = d - float(di);
    const int i0 = (w - di) & mask, i1 = (w - di - 1) & mask;
    return b[size_t(i0)] + fr * (b[size_t(i1)] - b[size_t(i0)]);
  }
  // band-pass output of a state-variable filter; k = 1 / Q
  float band(int c, float x, float g, float k) {
    const float v1 = (ic1[c] + g * (x - ic2[c])) / (1.0f + g * (g + k));
    const float v2 = ic2[c] + g * v1;
    ic1[c] = 2.0f * v1 - ic1[c]; ic2[c] = 2.0f * v2 - ic2[c];
    return v1;
  }
  // one octave down: the note, turned over at every second wave
  float sub(int c, float x) {
    lp[c] += k1 * (x - lp[c]);
    if (prev[c] <= 0.0f && lp[c] > 0.0f) flip[c] = -flip[c];
    prev[c] = lp[c];
    lp2[c] += k2 * (lp[c] * flip[c] - lp2[c]);
    return lp2[c];
  }
  float verb(int c, float x, float fb) {
    float sum = 0.0f;
    for (int i = 0; i < 4; ++i) {
      std::vector<float>& line = comb[c][i];
      const float out = line[size_t(ci[c][i])];
      cl[c][i] += 0.35f * (out - cl[c][i]);       // the highs die first
      line[size_t(ci[c][i])] = x * 0.25f + cl[c][i] * fb;
      if (++ci[c][i] >= int(line.size())) ci[c][i] = 0;
      sum += out;
    }
    for (int i = 0; i < 2; ++i) {
      std::vector<float>& line = allp[c][i];
      const float buf = line[size_t(ai[c][i])];
      const float out = buf - 0.5f * sum;
      line[size_t(ai[c][i])] = sum + 0.5f * out;
      if (++ai[c][i] >= int(line.size())) ai[c][i] = 0;
      sum = out;
    }
    return sum;
  }
};

}  // namespace ld
