// The stompbox collection.
//
// Every pedal has up to four knobs (a, r, p3, p4, each 0 .. 1) and a character switch (`voice`). A character is a
// way that kind of pedal is built in the real world: where it clips, what it does to the mids, how its sweep moves.
// They are voicings in that spirit, not copies of any one circuit.
//
// The six standard pedals of Loop Draft keep their old sound as character 0, handled by the engine itself; their
// other characters, and all of the fifteen further pedals, live here.
//
// One Pedal object per slot holds the state for whichever pedal is in the slot.
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
  std::vector<float> bufL, bufR;        // delay lines, a power of two long, a little over one second
  int mask = 0, w = 0;
  std::vector<float> comb[2][4], allp[2][2];
  int ci[2][4] = {}, ai[2][2] = {};
  float cl[2][4] = {};                  // damping inside the reverb's combs
  float ph = 0.0f, ph2 = 0.0f;          // LFOs, in turns
  float z[2][6] = {};                   // one-pole filter states, per channel
  float apx[2][6] = {}, apy[2][6] = {}; // all-pass stages (phaser, spring)
  float fbk[2] = {};                    // what is fed back (phaser)
  float env = 0.0f, env2 = 0.0f, gate = 1.0f, gain = 1.0f;
  float flip[2] = {1.0f, 1.0f}, flip2[2] = {1.0f, 1.0f}, prev[2] = {};
  float ic1[2] = {}, ic2[2] = {};       // state-variable filter
  float dTime = -1.0f;                  // delay time in samples, gliding to where the knob points
  int hold = 0;
  unsigned age = 0;
  int last = -1;                        // pedal and character that used this state last
  float la = -1.0f, lr = -1.0f, l3 = -1.0f, l4 = -1.0f;
  float k[8] = {};                      // values worked out from the knobs
  float cph[4] = {};                    // phaser stage coefficients
  float hil[2][2][4][4] = {};           // frequency shifter: two all-pass chains a quarter turn apart
  float s[8] = {};                      // odds and ends of the newer pedals
  uint32_t rng = 0x2545f491u;
  struct Grain { float pos = 0, len = 1, d0 = 0, rate = 1, panL = 0.7f, panR = 0.7f; bool on = false; };
  Grain grains[8];
  float grainClock = 0.0f;
  // The clock, set by the engine before every block: tempo, where in the bar we are (in quarter notes), whether this
  // pedal follows it, and the notes of the project's scale for the pedals that are tuned (15 of them, low to high).
  double beat = 0.0;
  float bpm = 120.0f;
  bool sync = false;
  const float* scale = nullptr;
  void setClock(float tempo, bool haveBeat, double beatNow, bool follow, const float* scaleHz) {
    bpm = tempo; sync = follow; scale = scaleHz;
    if (haveBeat) beat = beatNow;
  }

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

  // Back to silence: when another pedal takes the slot over, or the track has been quiet for long.
  void reset() {
    std::memset(bufL.data(), 0, bufL.size() * sizeof(float));
    std::memset(bufR.data(), 0, bufR.size() * sizeof(float));
    for (int c = 0; c < 2; ++c) {
      for (int i = 0; i < 4; ++i) { std::memset(comb[c][i].data(), 0, comb[c][i].size() * sizeof(float)); ci[c][i] = 0; cl[c][i] = 0.0f; }
      for (int i = 0; i < 2; ++i) { std::memset(allp[c][i].data(), 0, allp[c][i].size() * sizeof(float)); ai[c][i] = 0; }
      for (int i = 0; i < 6; ++i) z[c][i] = apx[c][i] = apy[c][i] = 0.0f;
      fbk[c] = prev[c] = ic1[c] = ic2[c] = 0.0f; flip[c] = flip2[c] = 1.0f;
    }
    ph = ph2 = env = env2 = 0.0f; gate = 1.0f; gain = 1.0f; dTime = -1.0f; w = 0; hold = 0; age = 0;
    std::memset(hil, 0, sizeof(hil));
    for (auto& v : s) v = 0.0f;
    for (auto& g : grains) g.on = false;
    grainClock = 0.0f;
    la = lr = l3 = l4 = -1.0f;
  }
  void sleep() { last = -1; }           // the next note finds it clean

  // Does this pedal keep sounding for seconds after its input stopped?
  static bool rings(FxType t) {
    return t == FxType::Delay || t == FxType::TapeEcho || t == FxType::Reverb || t == FxType::Cloud || t == FxType::DubDelay ||
           t == FxType::Resonator || t == FxType::Shimmer;
  }

  inline void tick(FxType type, int voice, float& l, float& r, float a, float rt, float p3, float p4) {
    const int key = int(type) * 8 + voice;
    if (key != last) { reset(); last = key; }
    a = unit(a); rt = unit(rt); p3 = unit(p3); p4 = unit(p4);
    const float fs = float(sr);
    const bool moved = a != la || rt != lr || p3 != l3 || p4 != l4;
    if (moved) { la = a; lr = rt; l3 = p3; l4 = p4; }
    float* ch[2] = {&l, &r};
    const float level = p3 * 2.0f;                 // where the third knob is LEVEL: unity in the middle
    ++age;
    beat += double(bpm) / (60.0 * sr);
    switch (type) {
      // ------------------------------------------------------------------------------------------ drive
      case FxType::Overdrive: {                    // DRIVE, TONE, LEVEL, BODY
        if (moved) { k[0] = 1.0f + a * a * 60.0f; k[1] = op(600.0f + rt * rt * 5400.0f); k[2] = op(1500.0f + rt * 7500.0f); }
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float y;
          if (voice == 2) {                        // GOLD: the clean signal stays underneath, harder clipping on top
            const float t = x * (1.0f + a * a * 40.0f);
            y = x * (1.0f - 0.6f * a) + (t / (1.0f + std::fabs(t))) * 1.6f * (0.3f + 0.7f * a);
            y += (y - lp(c, 1, y, op(1500.0f))) * (rt * 2.0f - 0.8f);
            y += lp(c, 0, y, op(150.0f)) * (p4 - 0.5f) * 1.5f;
            y *= 0.55f;
          } else if (voice == 3) {                 // BLUE: low gain, nothing added to the mids, opens up when hit hard
            y = fastTanh(x * (1.0f + a * 14.0f)) * (0.9f - 0.3f * a) + x * 0.3f;
            y = lp(c, 1, y, k[2]);
            y += lp(c, 0, y, op(120.0f)) * (p4 - 0.5f) * 1.2f;
            y *= 0.8f;
          } else {                                 // GREEN: only the mids and highs are driven, the famous hump
            const float low = lp(c, 0, x, op(720.0f));
            y = x * 0.6f + fastTanh((x - low) * k[0]);
            y = lp(c, 1, y, k[1]);
            y += low * (p4 - 0.5f) * 1.2f;
            y *= 0.6f;
          }
          *ch[c] = y * level * 0.75f;
        }
        break;
      }
      case FxType::Distortion: {                   // GAIN, TONE, LEVEL, MIDS
        if (moved) { k[0] = 1.0f + a * a * (voice == 3 ? 400.0f : 200.0f); k[1] = op(500.0f + rt * rt * 8000.0f); }
        const float mids = (p4 - 0.5f) * 2.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float y;
          if (voice == 1) {                        // ORANGE: hard clipping and a tone knob that scoops the middle
            y = hardClip(fastTanh(x * k[0] * 0.5f) * 2.0f);
            const float lo = lp(c, 1, y, op(350.0f)), hi = y - lp(c, 3, y, op(1600.0f));
            y = lo * (1.0f - rt) * 1.4f + hi * rt * 1.4f + y * 0.15f;
            y += band(c, y, svfG(650.0f), 1.4f) * mids;
          } else if (voice == 2) {                 // STACK: an amplifier pushed hard, uneven clipping, mids forward
            const float g = k[0] * 0.3f;
            y = hardClip((fastTanh(g * (x + 0.1f)) - fastTanh(g * 0.1f)) * 1.8f);
            y = lp(c, 1, y, k[1]);
            y += band(c, y, svfG(700.0f), 1.4f) * (0.6f + mids);
          } else if (voice == 3) {                 // METAL: tight lows, huge gain, the middle scooped out
            y = hardClip(fastTanh((x - lp(c, 0, x, op(140.0f))) * k[0]) * 2.0f);
            y = lp(c, 1, y, op(2500.0f + rt * 7000.0f));
            y -= band(c, y, svfG(750.0f), 1.4f) * (0.7f - mids * 0.7f);
          } else {                                 // RODENT: hard clipping after a slow amplifier chip that rounds the edges
            const float t = (x - lp(c, 0, x, op(60.0f))) * k[0];
            const float slew = 0.9f * 48000.0f / fs;
            float d = t - z[c][2];
            d = d > slew ? slew : (d < -slew ? -slew : d);
            z[c][2] = hardClip(z[c][2] + d);
            y = lp(c, 1, z[c][2], k[1]);
            y += band(c, y, svfG(650.0f), 1.4f) * mids;
          }
          *ch[c] = y * level * (voice == 2 ? 0.5f : 0.36f) * (1.0f - 0.3f * a);
        }
        break;
      }
      case FxType::Fuzz: {                         // FUZZ, TONE, LEVEL, BIAS
        const float bias = 0.02f + p4 * p4 * 0.4f;
        if (moved) { k[0] = 2.0f + a * a * (voice == 1 ? 200.0f : 120.0f); k[1] = op(1200.0f + rt * 5000.0f); k[2] = fastTanh(k[0] * bias); k[3] = fastTanh(k[0] * 2.0f * bias); }
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.01f : 0.0008f);
        float g = env * (60.0f + p4 * 400.0f); g = g > 1.0f ? 1.0f : g; g *= g;   // more bias: it sputters out sooner
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float y;
          if (voice == 1) {                        // WALL: two clipping stages in a row, endless sustain, scooped tone stack
            const float s2 = fastTanh(fastTanh(x * k[0] * 0.5f) * (4.0f + a * 20.0f));
            const float lo = lp(c, 1, s2, op(500.0f)), hi = s2 - lp(c, 3, s2, op(1300.0f));
            y = lo * (1.0f - rt) * 1.6f + hi * rt * 1.6f;
          } else if (voice == 2) {                 // BENDER: bright, biting, cuts off like a gate
            const float x1 = x - lp(c, 0, x, op(200.0f));
            y = hardClip((fastTanh(k[0] * 2.0f * (x1 + bias)) - k[3]) * 1.5f) * g;
            y = lp(c, 1, y, op(2500.0f + rt * 6500.0f));
          } else if (voice == 3) {                 // OCTAVE: the wave folded over, so an octave above rings on top
            const float x1 = x - lp(c, 0, x, op(150.0f));
            float rect = std::fabs(x1);
            rect -= lp(c, 3, rect, op(30.0f));
            y = lp(c, 1, fastTanh(rect * k[0] * 1.5f), k[1]);
          } else {                                 // ROUND: two soft transistors, warm, cleans up when you play softly
            y = lp(c, 1, (fastTanh(k[0] * (x + bias)) - k[2]) * (0.5f + 0.5f * g), k[1]);
          }
          *ch[c] = y * level * (voice == 1 || voice == 3 ? 0.3f : 0.45f);
        }
        break;
      }
      case FxType::Booster: {                      // BOOST, TREBLE, LEVEL, BASS
        const float g = 1.0f + a * (voice == 1 ? 5.0f : 3.0f);
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          const float hi = x - lp(c, 0, x, op(700.0f)), lo = lp(c, 1, x, op(200.0f));
          float y;
          if (voice == 1) {                        // TREBLE: lets mostly the top through and pushes it into soft clipping
            const float t = (hi * (1.5f + rt * 2.0f) + x * 0.25f + lo * (p4 - 0.5f)) * g * 1.5f;
            y = fastTanh(t + 0.1f) - 0.0996680f;
          } else if (voice == 2) {                 // PREAMP: the front end of a tape echo, a little thicker, a little brighter
            y = fastTanh((x + band(c, x, svfG(350.0f), 1.0f) * 0.5f + hi * rt + lo * (p4 - 0.5f)) * g * 0.9f) * 1.1f;
          } else {                                 // CLEAN: just more, with two tone controls
            y = fastTanh((x + hi * rt * 2.0f + lo * (p4 - 0.5f) * 2.0f) * g * 0.5f) * 2.0f;
          }
          *ch[c] = y * level;
        }
        break;
      }
      // --------------------------------------------------------------------------------------- dynamics
      case FxType::Compressor: {                   // SUSTAIN, ATTACK, LEVEL, BLEND
        if (moved) {
          const float att = voice == 2 ? 0.01f + rt * 0.05f : (voice == 3 ? 0.0002f + rt * 0.005f : 0.001f + rt * 0.04f);
          const float rel = voice == 2 ? 0.4f : (voice == 3 ? 0.06f : 0.15f);
          k[0] = 1.0f - std::exp(-1.0f / (att * fs)); k[1] = 1.0f - std::exp(-1.0f / (rel * fs));
          k[2] = (voice == 1 ? 0.25f : 0.3f) - a * (voice == 1 ? 0.22f : 0.25f);            // threshold
          k[3] = voice == 2 ? 3.0f : (voice == 3 ? 4.0f + a * 8.0f : 10.0f);               // ratio
        }
        const float in = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);
        env += (in - env) * (in > env ? k[0] : k[1]);
        float e = env;
        if (voice == 2) { env2 += (in - env2) * (in > env2 ? k[0] : k[1] * 0.2f); e = env > env2 * 0.7f ? env : env2 * 0.7f; }   // the light cell lets go slowly
        if ((age & 7u) == 0) gain = e > k[2] && e > 1e-5f ? std::pow(e / k[2], 1.0f / k[3] - 1.0f) : 1.0f;
        const float makeup = 1.0f + a * 2.5f, blend = p4 * 2.0f > 1.0f ? 1.0f : p4 * 2.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float wet = x * gain * makeup;
          if (voice == 3) wet = fastTanh(wet * 0.9f) * 1.111f;                  // STUDIO: quick and a little gritty
          else if (voice != 2) wet += band(c, wet, svfG(900.0f), 1.4f) * 0.15f;  // SQUASH: the small push in the mids
          *ch[c] = (x * (1.0f - blend) + wet * blend) * level;
        }
        break;
      }
      case FxType::Gate: {                         // THRESHOLD, DECAY, RANGE, HOLD
        if (moved) { k[0] = 0.001f * std::pow(10.0f, a * 2.0f); k[1] = voice == 1 ? 0.002f + rt * 0.02f : 0.0002f + rt * 0.004f; }
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.05f : 0.001f);
        if (env > k[0]) hold = int(p4 * 0.25f * fs); else if (hold > 0) --hold;
        const float target = env > k[0] || hold > 0 ? 1.0f : 0.0f;
        gate += (target - gate) * (target > gate ? (voice == 1 ? 0.2f : 0.02f) : k[1]);
        const float floor = p3 < 0.5f ? (1.0f - 2.0f * p3) * 0.5f : 0.0f;     // RANGE down: it only turns the noise down
        const float gg = floor + (1.0f - floor) * gate;
        l *= gg; r *= gg;
        break;
      }
      // ------------------------------------------------------------------------------------- modulation
      case FxType::Chorus: {                       // DEPTH, SPEED, MIX, TONE
        lfo(ph, 0.1f + rt * rt * 6.0f, rt);
        write(l, r);
        const std::vector<float>* buf[2] = {&bufL, &bufR};
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float wet;
          if (voice == 2) {                        // WIDE: two voices moving against each other, barely any wobble
            wet = read(*buf[c], (0.010f + (c ? -0.0015f : 0.0015f) * a * sinTurns(ph)) * fs);
          } else if (voice == 3) {                 // TRI: three voices a third of a turn apart, like a string machine
            const float o = c ? 0.3333f : 0.0f;
            wet = (read(*buf[c], (0.009f + 0.003f * a * sinTurns(ph + o)) * fs) + read(*buf[c], (0.011f + 0.003f * a * sinTurns(ph + o + 0.3333f)) * fs)) * 0.6f;
          } else {                                 // ENSEMBLE: one dark voice from a bucket-brigade chip
            wet = read(*buf[c], (0.007f + 0.004f * a * sinTurns(ph + (c ? 0.03f : 0.0f))) * fs);
          }
          wet = lp(c, 0, wet, op(1500.0f + p4 * 6000.0f));
          *ch[c] = x * (1.0f - 0.3f * p3) + wet * p3 * 1.4f;
        }
        break;
      }
      case FxType::Flanger: {                      // FEEDBACK, SPEED, DEPTH, MANUAL
        lfo(ph, 0.1f + rt * rt * 4.0f, rt);
        const float d = (0.0005f + p4 * 0.004f + p3 * 0.005f * (0.5f + 0.5f * sinTurns(ph))) * fs;
        const float wl = read(bufL, d), wr = read(bufR, d);
        if (voice == 2) {                          // ZERO: the sweep passes through the dry signal, which cancels for a moment
          const float dl = read(bufL, 0.003f * fs), dr = read(bufR, 0.003f * fs);
          write(l + wl * a * 0.5f, r + wr * a * 0.5f);
          l = (dl - wl) * 0.8f; r = (dr - wr) * 0.8f;
        } else if (voice == 1) {                   // MISTRESS: feedback the other way round, hollow and glassy
          write(l - wl * a * 0.6f, r - wr * a * 0.6f);
          l = (l + wl) * 0.7f; r = (r + wr) * 0.7f;
        } else {                                   // JET: the classic take-off
          write(l + wl * a * 0.8f, r + wr * a * 0.8f);
          l = (l + wl) * 0.7f; r = (r + wr) * 0.7f;
        }
        break;
      }
      case FxType::Phaser: {                       // DEPTH, SPEED, FEEDBACK, MIX
        lfo(ph, 0.05f + rt * rt * 8.0f, rt);
        if ((age & 15u) == 1) {                    // the sweep is slow: new coefficients every 16 samples
          float u = 0.5f + 0.5f * sinTurns(ph);
          if (voice == 3) u *= u;                  // VIBE: a lamp that glows up fast and fades slowly
          static const float spread[2][4] = {{1.0f, 1.0f, 1.0f, 1.0f}, {0.6f, 1.0f, 2.2f, 4.5f}};
          for (int i = 0; i < 4; ++i) {
            const float g = svfG(300.0f * (1.0f + u * a * 8.0f) * spread[voice == 3 ? 1 : 0][i]);
            cph[i] = (1.0f - g) / (1.0f + g);
          }
        }
        // SCRIPT has next to no feedback, BLOCK has plenty: that is the whole argument about the two
        const float fb = voice == 2 ? p3 * 0.85f : (voice == 3 ? p3 * 0.4f : (p3 > 0.5f ? (p3 - 0.5f) * 0.5f : 0.0f));
        const float mix = p4 * 2.0f > 1.0f ? 1.0f : p4 * 2.0f, dry = p4 > 0.5f ? 1.0f - (p4 - 0.5f) * 2.0f : 1.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float y = x + fbk[c] * fb;
          for (int i = 0; i < 4; ++i) {
            const float o = -cph[i] * y + apx[c][i] + cph[i] * apy[c][i];
            apx[c][i] = y; apy[c][i] = o; y = o;
          }
          fbk[c] = y;
          *ch[c] = (x * dry + y * mix) * 0.6f;
        }
        break;
      }
      case FxType::Tremolo: {                      // DEPTH, SPEED, SHAPE, PAN
        lfo(ph, 1.0f + rt * 13.0f, rt);
        const float hard = p3 > 0.5f ? (p3 - 0.5f) * 2.0f : 0.0f, sg = 1.0f + hard * hard * 30.0f, norm = 1.0f / fastTanh(sg);
        const float spread = std::fabs(p4 - 0.5f);
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          const float m = fastTanh(sinTurns(ph + (c ? spread : 0.0f)) * sg) * norm;      // -1 .. 1
          float y;
          if (voice == 2) {                        // HARMONIC: lows and highs take turns, it swirls more than it pulses
            const float lo = lp(c, 0, x, op(800.0f));
            y = lo * (1.0f - a * (0.5f - 0.5f * m)) + (x - lo) * (1.0f - a * (0.5f + 0.5f * m));
          } else if (voice == 1) {                 // OPTICAL: a light cell, quick up and lazy down
            float u = 0.5f + 0.5f * m; u *= u;
            y = x * (1.0f - a * (1.0f - u));
          } else if (voice == 3) {                 // CHOP: on or off
            y = x * lp(c, 1, m > 0.0f ? 1.0f : 1.0f - a, 0.02f);
          } else {                                 // BIAS: the smooth throb of an amplifier's own tremolo
            y = x * (1.0f - a * (0.5f - 0.5f * m));
          }
          *ch[c] = y;
        }
        break;
      }
      case FxType::Vibrato: {                      // DEPTH, SPEED, MIX, TONE
        lfo(ph, voice == 1 ? 0.4f + rt * 2.0f : 2.0f + rt * 7.0f, rt);
        advance(ph2, 7.3f / fs);
        write(l, r);
        float d = 0.005f + 0.003f * a * sinTurns(ph);
        if (voice == 1) d += 0.0004f * a * sinTurns(ph2);                       // TAPE: slow wow and a quick flutter on top
        const float mix = p3 * 2.0f > 1.0f ? 1.0f : p3 * 2.0f, tone = op(1500.0f + p4 * 9000.0f);
        l = l * (1.0f - mix) + lp(0, 0, read(bufL, d * fs), tone) * mix;
        r = r * (1.0f - mix) + lp(1, 0, read(bufR, d * fs), tone) * mix;
        break;
      }
      case FxType::Rotary: {                       // DEPTH, SPEED, BALANCE, DRIVE
        advance(ph, (0.7f + rt * 6.0f) / fs);
        advance(ph2, (0.6f + rt * 5.0f) / fs);     // the heavy drum never quite keeps up with the horn
        const float s = sinTurns(ph), c1 = sinTurns(ph + 0.25f), s2 = sinTurns(ph2);
        const float m = fastTanh(0.5f * (l + r) * (1.0f + p4 * 4.0f)) / (1.0f + p4);
        if (voice == 1) {                          // SIMPLE: one speaker turning
          write(m, m);
          l = read(bufL, (0.0015f + 0.001f * a * s) * fs) * (1.0f - 0.45f * a * (0.5f + 0.5f * c1));
          r = read(bufR, (0.0015f - 0.001f * a * s) * fs) * (1.0f - 0.45f * a * (0.5f - 0.5f * c1));
        } else {                                   // CABINET: horn for the highs, drum for the lows
          const float lo = lp(0, 0, m, op(800.0f));
          write(m - lo, lo);                       // one line each
          const float hg = p3 * 2.0f > 1.0f ? 1.0f : p3 * 2.0f, dg = (1.0f - p3) * 2.0f > 1.0f ? 1.0f : (1.0f - p3) * 2.0f;
          const float drum = read(bufR, 0.002f * fs);
          l = read(bufL, (0.0015f + 0.001f * a * s) * fs) * (1.0f - 0.5f * a * (0.5f + 0.5f * c1)) * hg + drum * (1.0f - 0.3f * a * (0.5f + 0.5f * s2)) * dg;
          r = read(bufL, (0.0015f - 0.001f * a * s) * fs) * (1.0f - 0.5f * a * (0.5f - 0.5f * c1)) * hg + drum * (1.0f - 0.3f * a * (0.5f - 0.5f * s2)) * dg;
        }
        break;
      }
      case FxType::RingMod: {                      // MIX, PITCH, WOBBLE, WOBBLE SPEED
        if (moved) k[0] = 30.0f * std::pow(2000.0f / 30.0f, rt);
        advance(ph2, (0.1f + p4 * p4 * 12.0f) / fs);
        const float wob = p3 > 0.5f ? (p3 - 0.5f) * 2.0f : 0.0f;
        advance(ph, k[0] * (1.0f + wob * 0.5f * sinTurns(ph2)) / fs);
        float car = sinTurns(ph);
        if (voice == 1) car = fastTanh(car * 8.0f);                             // SQUARE: harsher, more overtones
        const float m = (1.0f - a) + a * car;
        l *= m; r *= m;
        break;
      }
      // -------------------------------------------------------------------------------- filter and pitch
      case FxType::Filter: {                       // CUTOFF, RESONANCE, ENVELOPE, DRIVE
        if (moved) k[0] = 100.0f * std::pow(100.0f, a);
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.004f : 0.0003f);
        float fc = k[0] * (1.0f + (p3 - 0.5f) * 2.0f * env * 20.0f);
        fc = fc < 40.0f ? 40.0f : (fc > fs * 0.3f ? fs * 0.3f : fc);
        const float drive = 1.0f + p4 * 4.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          if (voice == 2) {                        // BAND: only a slice around the cutoff
            *ch[c] = band(c, fastTanh(x * drive) / drive * 2.0f, svfG(fc), 1.0f / (1.0f + rt * 12.0f)) * 1.6f;
          } else {                                 // LADDER: four stages and feedback, the fat synthesizer filter
            const float g = op(fc);
            float y = fastTanh(x * drive - z[c][3] * rt * 3.8f);
            for (int i = 0; i < 4; ++i) { z[c][i] += g * (y - z[c][i]); y = z[c][i]; }
            *ch[c] = y * (1.0f + rt * 1.5f) / (0.5f + 0.5f * drive) * 1.5f;
          }
        }
        break;
      }
      case FxType::Wah: {                          // DEPTH, SPEED, RANGE, SHARPNESS
        lfo(ph, 0.3f + rt * rt * 6.0f, rt);
        const float u = 0.5f + 0.5f * sinTurns(ph), shift = 0.6f + p3 * 0.8f;
        const float f = voice == 1 ? (330.0f + u * 1400.0f) * shift : (voice == 2 ? (120.0f + u * u * 900.0f) * shift : (400.0f + u * u * 1800.0f) * shift);
        const float kq = 1.0f / (voice == 1 ? 1.5f + p4 * 5.0f : 2.0f + p4 * 8.0f), g = svfG(f);
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float low = 0.0f;
          const float bp = svf(c, x, g, kq, low);
          float wet = voice == 2 ? low * 1.1f : bp * 2.2f;                      // BASS keeps everything below the sweep
          if (voice == 1) wet = fastTanh(wet * 1.5f) * 0.8f;                     // CLYDE: lower, rounder, growls a little
          *ch[c] = x * (1.0f - a) + wet * a;                                    // CRY: narrow and sharp
        }
        break;
      }
      case FxType::AutoWah: {                      // SENSITIVITY, PEAK, RANGE, DECAY
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.004f : 0.00005f + (1.0f - p4) * 0.0004f);
        float u = env * (2.0f + a * 14.0f); u = u > 1.0f ? 1.0f : u;
        if (voice == 1) u = 1.0f - u;                                           // DOWN: closes when you dig in
        const float g = svfG((300.0f + u * 2600.0f) * (0.5f + p3)), kq = 1.0f / (2.0f + rt * 8.0f);
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float low = 0.0f;
          const float bp = svf(c, x, g, kq, low);
          *ch[c] = voice == 2 ? low * 1.3f : x * 0.25f + bp * 1.8f;             // LOW-PASS: the funk filter with the lows left in
        }
        break;
      }
      case FxType::Octaver: {                      // OCTAVE, TONE, DRY, SECOND
        if (moved) { k[1] = op(300.0f); k[2] = op(150.0f + rt * 500.0f); }
        const float second = p4 > 0.5f ? (p4 - 0.5f) * 4.0f : 0.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float two = 0.0f;
          const float one = sub(c, x, two);
          float y = x * p3 * 2.0f;
          if (voice == 1) {                        // UP: the wave folded over rings an octave above; SECOND adds the one below
            const float x1 = x - lp(c, 4, x, op(150.0f));
            float up = std::fabs(x1);
            up -= lp(c, 5, up, op(30.0f));
            y += lp(c, 3, up, op(1500.0f + rt * 6000.0f)) * a * 3.0f + one * second;
          } else {                                 // SUB: one octave down, SECOND adds two down
            y += one * a * 2.5f + two * second * 1.5f;
          }
          *ch[c] = y;
        }
        break;
      }
      // --------------------------------------------------------------------------------- echo and space
      case FxType::Delay: {                        // LEVEL, TIME (40 ms .. 1 s), REPEATS, TONE (analog: wobble)
        const float target = sync ? syncSamples(rt) : (0.04f + rt * rt * 0.96f) * fs;
        if (dTime < 0.0f) dTime = target;
        dTime += (target - dTime) * 0.0004f;        // a moved knob bends the pitch for a moment, as on a real one
        float d = dTime;
        if (voice == 1) { advance(ph, 0.5f / fs); d += (p4 > 0.5f ? (p4 - 0.5f) * 2.0f : 0.0f) * 0.002f * fs * sinTurns(ph); }
        float wl = read(bufL, d), wr = read(bufR, d);
        const float fb = p3 * 0.92f;
        if (voice == 1) {                          // ANALOG: every repeat duller and softer than the one before
          const float c1 = op(3500.0f - rt * 2200.0f);
          wl = fastTanh(lp(0, 0, wl, c1) * 1.1f); wr = fastTanh(lp(1, 0, wr, c1) * 1.1f);
          write(l + wl * fb, r + wr * fb);
        } else if (voice == 2) {                   // PING-PONG: the repeats jump from side to side
          const float t = (p4 - 0.5f) * 1.2f;
          wl += (wl - lp(0, 0, wl, op(1000.0f))) * t; wr += (wr - lp(1, 0, wr, op(1000.0f))) * t;
          write(0.5f * (l + r) + wr * fb, wl * fb);
        } else {                                   // DIGITAL: every repeat a clean copy
          const float t = (p4 - 0.5f) * 1.2f;
          wl += (wl - lp(0, 0, wl, op(1000.0f))) * t; wr += (wr - lp(1, 0, wr, op(1000.0f))) * t;
          write(l + hardClip(wl * fb), r + hardClip(wr * fb));
        }
        l += wl * a * 1.4f; r += wr * a * 1.4f;
        break;
      }
      case FxType::TapeEcho: {                     // LEVEL, TIME, REPEATS, AGE
        const float target = sync ? syncSamples(rt) : (0.06f + rt * 0.54f) * fs;
        if (dTime < 0.0f) dTime = target;
        dTime += (target - dTime) * 0.0004f;
        advance(ph, 0.6f / fs); advance(ph2, 7.3f / fs);
        const float worn = voice == 2 ? 2.5f : 1.0f;
        const float d = dTime + worn * ((0.0008f + p4 * 0.003f) * sinTurns(ph) + p4 * 0.0004f * sinTurns(ph2)) * fs;
        float wl = read(bufL, d), wr = read(bufR, d);
        if (voice == 1) {                          // MULTI-HEAD: three playback heads along the tape
          wl = (wl + read(bufL, d * 0.6667f) * 0.7f + read(bufL, d * 0.3333f) * 0.5f) * 0.6f;
          wr = (wr + read(bufR, d * 0.6667f) * 0.7f + read(bufR, d * 0.3333f) * 0.5f) * 0.6f;
        }
        const float dull = op((voice == 2 ? 3200.0f : 5000.0f) - p4 * (voice == 2 ? 2200.0f : 3500.0f)), sat = 1.0f + p4;
        wl = lp(0, 0, wl, dull); wr = lp(1, 0, wr, dull);
        wl = fastTanh((wl - lp(0, 1, wl, op(120.0f))) * sat); wr = fastTanh((wr - lp(1, 1, wr, op(120.0f))) * sat);
        const float fb = p3 * 1.05f;               // past three o'clock it feeds itself, held by the tape's own limit
        write(l + wl * fb, r + wr * fb);
        l += wl * a * 1.4f; r += wr * a * 1.4f;
        break;
      }
      case FxType::Reverb: {                       // LEVEL, DECAY, TONE, PRE-DELAY (spring: DWELL)
        const float tone = op(1500.0f + p3 * 6000.0f);
        if (voice == 0) {                          // SPRING: band-limited, with the drip of the springs
          for (int c = 0; c < 2; ++c) {
            const float x = *ch[c];
            float in = fastTanh(x * (0.5f + p4 * 1.5f));
            in = lp(c, 0, in - lp(c, 1, in, op(150.0f)), op(4500.0f));
            for (int i = 0; i < 6; ++i) {           // a chain of all-passes smears the attack into a chirp
              const float o = -0.6f * in + apx[c][i] + 0.6f * apy[c][i];
              apx[c][i] = in; apy[c][i] = o; in = o;
            }
            *ch[c] = x + lp(c, 2, verb(c, in, 0.75f + rt * 0.2f, 0.35f), tone) * a * 0.9f;
          }
        } else {
          write(l, r);
          const float pre = (voice == 2 ? 0.02f : 0.0f) * fs + 1.0f + p4 * 0.12f * fs;
          float in[2] = {read(bufL, pre), read(bufR, pre)};
          // PLATE: bright and dense. HALL: darker, longer. CLOUD: nearly endless, slowly turning.
          const float fb = voice == 1 ? 0.70f + rt * 0.27f : (voice == 2 ? 0.80f + rt * 0.18f : 0.86f + rt * 0.12f);
          const float damp = voice == 1 ? 0.6f : (voice == 2 ? 0.25f : 0.3f);
          if (voice == 3) advance(ph, 0.3f / fs);
          for (int c = 0; c < 2; ++c) {
            const float x = *ch[c];
            float xin = in[c];
            if (voice == 1) xin -= lp(c, 1, xin, op(250.0f));
            float wet = verb(c, xin, fb, damp);
            if (voice == 3) wet *= 1.0f + 0.25f * sinTurns(ph + (c ? 0.25f : 0.0f));
            *ch[c] = x + lp(c, 2, wet, tone) * a * 0.8f;
          }
        }
        break;
      }
      // -------------------------------------------------------------------------------- made for drums
      case FxType::Punch: {                        // ATTACK, SUSTAIN, LEVEL, CLIP
        if (moved) {
          const float f = voice == 1 ? 0.5f : 1.0f;
          k[0] = 1.0f - std::exp(-1.0f / (0.0004f * f * fs)); k[1] = 1.0f - std::exp(-1.0f / (0.018f * f * fs));
          k[2] = 1.0f - std::exp(-1.0f / (0.2f * fs));
        }
        const float in = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);
        env += (in - env) * (in > env ? k[0] : k[2]);          // jumps up with the hit
        env2 += (in - env2) * (in > env2 ? k[1] : k[2]);       // comes after it
        const float d = (env - env2) / (env + 1e-4f);          // near 1 at the very start of a hit, 0 once it rings
        const float att = (a - 0.5f) * 2.0f, sus = (rt - 0.5f) * 2.0f;
        const float ga = att >= 0.0f ? 1.0f + att * d * 3.0f : 1.0f + att * d * 0.95f;
        const float gs = sus >= 0.0f ? 1.0f + sus * (1.0f - d) * 1.5f : 1.0f + sus * (1.0f - d) * 0.9f;
        const float clip = p4 > 0.5f ? (p4 - 0.5f) * 2.0f : 0.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float y;
          if (voice == 1) {                        // SNAP: only the top of the hit is shaped, the body stays as it is
            const float low = lp(c, 0, x, op(900.0f));
            y = low * gs + (x - low) * ga * gs;
          } else {                                 // PUNCH: the whole hit
            y = x * ga * gs;
          }
          y *= level;
          if (clip > 0.0f) y = fastTanh(y * (1.0f + clip * 4.0f)) / (1.0f + clip);
          *ch[c] = y;
        }
        break;
      }
      case FxType::Crusher: {                      // DRIVE, TONE, GLUE, BODY
        const float in = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);
        const float rs = 48000.0f / fs;
        env += (in - env) * (in > env ? 0.004f : 0.00025f) * rs;
        const float thr = 0.5f - p3 * 0.42f;
        if ((age & 7u) == 0) gain = env > thr ? std::pow(env / thr, -0.75f) : 1.0f;     // glue: four to one
        const float makeup = 1.0f + p3 * 1.5f, d = 1.0f + a * a * 30.0f;
        if (moved) k[0] = op(600.0f + rt * rt * 15000.0f);
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c] * gain * makeup;
          const float low = lp(c, 0, x, op(150.0f));
          float y;
          if (voice == 1) { const float t = x * d; y = t / (1.0f + std::fabs(t)) * 1.3f; }   // DIODE: crunchy
          else if (voice == 2) y = hardClip(x * d);                                         // CLIP: flat tops, the gabber kick
          else y = lp(c, 1, fastTanh(x * d * 0.7f + 0.05f) - 0.04996f, op(9000.0f - a * 3000.0f)) * 1.2f;   // TAPE: round and warm
          y = lp(c, 2, y, k[0]);
          y += low * (p4 - 0.5f) * 1.5f;
          *ch[c] = y * (0.36f - 0.14f * a);
        }
        break;
      }
      case FxType::Slicer: {                       // DEPTH, RATE, WIDTH, PATTERN (stutter: HOW MUCH)
        static const float divs[6] = {1.0f, 0.5f, 0.33333f, 0.25f, 0.16667f, 0.125f};      // 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32
        const double div = double(divs[int(rt * 5.99f)]);
        const double u = beat / div;
        const double fl = std::floor(u);
        const float fr = float(u - fl);
        const float rs = 48000.0f / fs, width = 0.1f + 0.85f * p3;
        if (voice == 1) {                          // STUTTER: at the end of every two beats the last slice is repeated
          write(l, r);
          const double period = div * 8.0, up = beat / period;
          const float pf = float(up - std::floor(up)), zone = 1.0f - (0.125f + p4 * 0.75f);
          const float len = float(div * 60.0 / double(bpm)) * fs;
          const bool in = pf >= zone;
          gate += ((in ? 1.0f : 0.0f) - gate) * 0.02f * rs;
          float wl = l, wr = r;
          if (in) {
            const float t = (pf - zone) * float(period * 60.0 / double(bpm)) * fs;
            const int maxRep = juceFreeMax(1, int(float(mask - 8) / (len + 1.0f)));
            const int rep = 1 + int(t / len) % maxRep;
            const float cut = fr < width || p3 > 0.94f ? 1.0f : 0.0f;
            wl = read(bufL, len * float(rep)) * cut; wr = read(bufR, len * float(rep)) * cut;
          }
          const float m = a * gate;
          l = l * (1.0f - m) + wl * m; r = r * (1.0f - m) + wr * m;
        } else {                                   // GATE: sixteen steps that let the sound through or not
          static const uint16_t pats[8] = {0xFFFF, 0x5555, 0x7777, 0x6DB6, 0x9249, 0x0F0F, 0xD5D5, 0xB5AD};
          const bool on = ((pats[int(p4 * 7.99f)] >> (int64_t(fl) & 15)) & 1) != 0 && fr < width;
          gate += ((on ? 1.0f : 0.0f) - gate) * (on ? 0.03f : 0.012f) * rs;
          const float g = 1.0f - a * (1.0f - gate);
          l *= g; r *= g;
        }
        break;
      }
      case FxType::GatedVerb: {                    // LEVEL, GATE, SIZE, TONE
        const float in = 0.5f * (std::fabs(l) + std::fabs(r)), rs = 48000.0f / fs;
        env += (in - env) * (in > env ? 0.05f : 0.002f) * rs;
        env2 += (in - env2) * 0.0008f * rs;
        const int len = int((0.05f + rt * 0.55f) * fs);
        if (env > 0.01f + env2 * 1.4f) hold = len; else if (hold > 0) --hold;      // every hit opens the gate anew
        float g;
        if (voice == 1) { const float t = hold > 0 ? 1.0f - float(hold) / float(len) : 0.0f; g = hold > 0 ? 0.15f + 0.85f * t * t : 0.0f; }   // REVERSE: swells, then is gone
        else g = hold > 0 ? 1.0f : 0.0f;                                         // GATED: huge, then cut off
        gate += (g - gate) * (g > gate ? 0.02f : 0.006f) * rs;
        const float tone = op(1200.0f + p4 * 9000.0f);
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          *ch[c] = x + lp(c, 0, verb(c, x, 0.80f + p3 * 0.17f, 0.2f), tone) * gate * a * 1.6f;
        }
        break;
      }
      case FxType::Boom: {                         // BOOM, PITCH, DECAY, SENSITIVITY
        const float low = lp(0, 0, 0.5f * (l + r), op(180.0f)), in = std::fabs(low), rs = 48000.0f / fs;
        env += (in - env) * (in > env ? 0.03f : 0.003f) * rs;
        env2 += (in - env2) * 0.0015f * rs;
        if (hold > 0) --hold;
        const float thr = 0.004f + 0.3f * (1.0f - p4) * (1.0f - p4);
        if (hold == 0 && env > thr && env > env2 * 1.6f) { s[0] = 1.0f; s[1] = 0.0f; s[2] = 1.0f; hold = int(0.08f * fs); }   // a low hit sets it off
        if (moved) { k[0] = std::exp(-1.0f / ((0.05f + p3 * 0.6f) * (voice == 1 ? 0.5f : 1.0f) * fs)); k[1] = std::exp(-1.0f / (0.025f * fs)); }
        const float f = (35.0f + rt * 55.0f) * (1.0f + (voice == 1 ? 3.0f : 0.8f) * s[2]);    // THUMP drops further, like a drum machine's kick
        advance(s[1], f / fs);
        const float y = sinTurns(s[1]) * s[0] * a * 0.4f;
        s[0] *= k[0]; s[2] *= k[1];
        l += y; r += y;
        break;
      }
      // ------------------------------------------------------------------------------- made for synths
      case FxType::Cloud: {                        // MIX, GRAIN, SCATTER, FEEDBACK
        write(l + s[4] * p4 * 0.6f, r + s[5] * p4 * 0.6f);
        grainClock -= 1.0f;
        if (grainClock <= 0.0f) {                  // a new grain: a short piece of what came in a moment ago
          const float len = (0.01f + rt * rt * 0.24f) * fs;
          for (auto& g : grains) {
            if (g.on) continue;
            g.on = true; g.pos = 0.0f; g.len = len;
            g.rate = voice == 1 ? -1.0f : (voice == 2 ? (rnd() < 0.5f ? 2.0f : 1.0f) : (voice == 3 ? (rnd() < 0.5f ? 1.5f : 1.0f) : 1.0f));
            g.d0 = 32.0f + rnd() * p3 * 0.6f * fs + (g.rate > 1.0f ? len * (g.rate - 1.0f) : 0.0f);
            const float pan = 0.5f + (rnd() - 0.5f) * p3;
            g.panL = 1.4f * (1.0f - pan); g.panR = 1.4f * pan;
            break;
          }
          grainClock = len * (0.25f + 0.15f * rnd());
        }
        float wl = 0.0f, wr = 0.0f;
        for (auto& g : grains) {
          if (!g.on) continue;
          const float u = g.pos / g.len, win = 4.0f * u * (1.0f - u);
          const float d = g.d0 - g.pos * (g.rate - 1.0f);     // played faster: the delay shrinks; backwards: it grows
          wl += read(bufL, d) * win * g.panL; wr += read(bufR, d) * win * g.panR;
          g.pos += 1.0f;
          if (g.pos >= g.len) g.on = false;
        }
        wl *= 0.5f; wr *= 0.5f;
        s[4] = fastTanh(wl); s[5] = fastTanh(wr);
        l = l * (1.0f - 0.5f * a) + wl * a * 1.3f; r = r * (1.0f - 0.5f * a) + wr * a * 1.3f;
        break;
      }
      case FxType::LoFi: {                         // WOBBLE, AGE, DRIVE, NOISE
        advance(ph, (voice == 2 ? 0.556f : (voice == 1 ? 0.4f : 0.8f)) / fs);
        advance(ph2, (voice == 1 ? 9.0f : 6.5f) / fs);
        if ((age & 4095u) == 0) s[0] = rnd() * 2.0f - 1.0f;      // the transport is never quite steady
        s[1] += (s[0] - s[1]) * 0.0002f;
        const float depth = a * a;
        const float d = (0.006f + depth * (0.0025f * sinTurns(ph) + (voice == 2 ? 0.0f : 0.0004f) * sinTurns(ph2) + 0.0015f * s[1])) * fs;
        write(l, r);
        const float w2[2] = {read(bufL, d), read(bufR, d)};
        const float hi = op(voice == 1 ? 16000.0f - rt * 8000.0f : 14000.0f - rt * 11000.0f), lo = op(20.0f + rt * 180.0f);
        const float g = 1.0f + p3 * 4.0f, norm = 1.0f / std::sqrt(g);
        const float in = 0.5f * (std::fabs(l) + std::fabs(r));
        env += (in - env) * (in > env ? 0.01f : 0.00005f);
        const float nl = p4 * p4 * (env * 30.0f > 1.0f ? 1.0f : env * 30.0f);      // the noise goes when the music goes
        if (voice == 2 && rnd() < (2.0f + p4 * 30.0f) / fs) s[2] = (rnd() * 2.0f - 1.0f) * 0.6f;   // VINYL: a crackle now and then
        s[2] *= 0.86f;
        for (int c = 0; c < 2; ++c) {
          float y = lp(c, 1, lp(c, 0, w2[c], hi), hi);
          y -= lp(c, 2, y, lo);
          y = fastTanh(y * g) * norm * 1.15f;
          if (voice == 1) y += lp(c, 3, y, op(110.0f)) * 0.35f;                  // REEL: the head's bump in the lows
          const float white = rnd() * 2.0f - 1.0f;
          if (voice == 2) y += (s[2] * (c ? 0.8f : 1.0f) + lp(c, 4, white, op(40.0f)) * 0.5f) * nl;
          else y += (white - lp(c, 4, white, op(2500.0f))) * (voice == 1 ? 0.006f : 0.018f) * nl;   // CASSETTE hisses
          *ch[c] = y;
        }
        break;
      }
      case FxType::DubDelay: {                     // LEVEL, TIME (always in time with the tempo), FEEDBACK, FILTER
        const float target = syncSamples(rt);
        if (dTime < 0.0f) dTime = target;
        dTime += (target - dTime) * 0.0004f;
        float wl = read(bufL, voice == 2 ? dTime * 0.75f : dTime), wr = read(bufR, dTime);
        if (moved) {                               // left of the middle the repeats get darker, right of it thinner
          k[0] = op(p4 < 0.5f ? 300.0f * std::pow(40.0f, p4 * 2.0f) : 12000.0f);
          k[1] = op(p4 > 0.5f ? 60.0f * std::pow(30.0f, (p4 - 0.5f) * 2.0f) : 60.0f);
        }
        wl = lp(0, 0, wl, k[0]); wl -= lp(0, 1, wl, k[1]); wl = fastTanh(wl * 1.2f);
        wr = lp(1, 0, wr, k[0]); wr -= lp(1, 1, wr, k[1]); wr = fastTanh(wr * 1.2f);
        const float fb = p3 * 1.02f;
        if (voice == 1) write(0.5f * (l + r) + wr * fb, wl * fb);     // PING-PONG
        else write(l + wl * fb, r + wr * fb);                          // DUB, and WIDE with two different times
        l += wl * a * 1.4f; r += wr * a * 1.4f;
        break;
      }
      case FxType::FreqShift: {                    // SHIFT, MIX, FEEDBACK, SPREAD
        if (moved) k[0] = 0.5f * std::pow(2000.0f, a);
        advance(ph, k[0] / fs);
        for (int c = 0; c < 2; ++c) {
          const float x0 = *ch[c], x = x0 + fbk[c] * p3 * 0.8f;
          float i = 0.0f, q = 0.0f;
          hilbert(c, x, i, q);
          const float pc = ph + (c ? (p4 - 0.5f) * 0.5f + 1.0f : 0.0f);
          const float dir = voice == 0 ? 1.0f : (voice == 1 ? -1.0f : (c ? 1.0f : -1.0f));   // UP, DOWN, or SPLIT: down on the left, up on the right
          const float y = i * sinTurns(pc + 0.25f) - dir * q * sinTurns(pc);
          fbk[c] = fastTanh(y);
          *ch[c] = x0 * (1.0f - rt) + y * rt;
        }
        break;
      }
      case FxType::Resonator: {                    // MIX, NOTE, DECAY, BRIGHT
        const int deg = int(rt * 14.99f);
        const float base = scale ? scale[0] : 65.4f;
        if (moved || base != s[3]) {
          s[3] = base;
          const float t60 = 0.1f + p3 * p3 * 6.0f;
          for (int i = 0; i < 3; ++i) {
            const int dg = deg + 2 * i;                           // CHORD: the note, and the third and fifth of the scale above it
            const float f = scale ? (dg <= 14 ? scale[dg] : scale[dg - 7] * 2.0f) : 65.4f * std::pow(2.0f, float(dg) / 7.0f);
            k[i] = fs / (voice == 1 ? 2.0f * f : f);              // TUBE: closed at one end, only the odd overtones
            k[3 + i] = std::pow(0.001f, 1.0f / (f * t60));
          }
        }
        const int count = voice == 2 ? 3 : 1;
        const float dc = 0.15f + p4 * 0.8f, sign = voice == 1 ? -1.0f : 1.0f;
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          float sum = 0.0f;
          for (int i = 0; i < count; ++i) {
            std::vector<float>& line = comb[c][i];
            const int size = int(line.size());
            float len = k[i];
            if (len > float(size - 3)) len = float(size - 3);
            if (len < 4.0f) len = 4.0f;
            float pos = float(ci[c][i]) - len;
            if (pos < 0.0f) pos += float(size);
            const int i0 = int(pos), i1 = i0 + 1 >= size ? 0 : i0 + 1;
            const float out = line[size_t(i0)] + (pos - float(i0)) * (line[size_t(i1)] - line[size_t(i0)]);
            cl[c][i] += dc * (out - cl[c][i]);
            line[size_t(ci[c][i])] = x * 0.5f + sign * cl[c][i] * k[3 + i];
            if (++ci[c][i] >= size) ci[c][i] = 0;
            sum += out;
          }
          *ch[c] = x * (1.0f - 0.6f * a) + sum * a * (count == 3 ? 0.7f : 1.2f);
        }
        break;
      }
      case FxType::Shimmer: {                      // LEVEL, DECAY, SHIMMER (freeze: HOLD), TONE
        const bool frozen = voice == 2 && p3 > 0.5f;                 // HOLD up: what is in the room now stays, new notes play over it
        const float fb = frozen ? 1.0f : 0.80f + rt * 0.18f, damp = frozen ? 1.0f : 0.35f;
        // the octave is fed back only as far as the room can swallow it, so it rises and fades but never runs away
        const float shim = voice == 2 ? 0.0f : p3 * 3.0f * (1.0f - fb);
        const float win = 0.05f * fs;
        advance(ph, ((voice == 1 ? 1.4983f : 2.0f) - 1.0f) / win);
        const float pb = ph + 0.5f >= 1.0f ? ph - 0.5f : ph + 0.5f;
        const float ga = 1.0f - std::fabs(2.0f * ph - 1.0f), tone = op(1500.0f + p4 * 9000.0f);
        const std::vector<float>* buf[2] = {&bufL, &bufR};
        float wet[2];
        for (int c = 0; c < 2; ++c) {
          const float x = *ch[c];
          const float up = read(*buf[c], 2.0f + (1.0f - ph) * win) * ga + read(*buf[c], 2.0f + (1.0f - pb) * win) * (1.0f - ga);
          float fed = lp(c, 1, up, op(6000.0f));
          fed -= lp(c, 2, fed, op(200.0f));              // nothing that stands still may go round: it would only pile up
          wet[c] = verb(c, (frozen ? 0.0f : x) + fastTanh(fed) * shim, fb, damp);
          *ch[c] = x + lp(c, 0, wet[c], tone) * a * 0.8f;
        }
        write(wet[0], wet[1]);
        break;
      }
      default: break;
    }
  }

 private:
  static float unit(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
  static float hardClip(float x) { return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x); }
  static void advance(float& phase, float by) { phase += by; if (phase >= 1.0f) phase -= 1.0f; }
  static int juceFreeMax(int a, int b) { return a > b ? a : b; }
  float rnd() { rng = rng * 1664525u + 1013904223u; return float(rng >> 8) * (1.0f / 16777216.0f); }   // 0 .. 1
  // An LFO: free at `hz`, or, when the pedal follows the tempo, locked to the bar. The speed knob then picks the
  // length of one cycle: 4 bars, 2 bars, 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32.
  void lfo(float& phase, float hz, float rt) {
    if (!sync) { advance(phase, hz / float(sr)); return; }
    static const double beats[8] = {16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
    const double u = beat / beats[int(rt * 7.99f)];
    phase = float(u - std::floor(u));
  }
  // A delay time in step with the tempo, in samples. The knob picks 1/16, 1/8T, 1/8, 1/4T, 1/8 dotted, 1/4,
  // 1/4 dotted or 1/2; a time too long for the delay line is halved.
  float syncSamples(float rt) const {
    static const float beats[8] = {0.25f, 0.33333f, 0.5f, 0.66667f, 0.75f, 1.0f, 1.5f, 2.0f};
    float t = beats[int(rt * 7.99f)] * 60.0f / (bpm < 20.0f ? 20.0f : bpm) * float(sr);
    while (t > float(mask - 8)) t *= 0.5f;
    return t;
  }
  // Two signals a quarter turn apart at every frequency (after O. Niemitalo): what a frequency shifter is made of.
  void hilbert(int c, float x, float& i, float& q) {
    static const float co[2][4] = {{0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f},
                                   {0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f}};
    float out[2];
    for (int p = 0; p < 2; ++p) {
      float v = x;
      for (int n = 0; n < 4; ++n) {
        float* st = hil[c][p][n];                  // x1, x2, y1, y2
        const float a2 = co[p][n] * co[p][n];
        const float y = a2 * (v + st[3]) - st[1];
        st[1] = st[0]; st[0] = v; st[3] = st[2]; st[2] = y;
        v = y;
      }
      out[p] = v;
    }
    i = z[c][5]; z[c][5] = out[0];                 // the first chain is one sample late by design
    q = out[1];
  }
  // coefficient of a one-pole low-pass at hz
  float op(float hz) const { const float c = 6.2831853f * hz / float(sr); return c > 0.99f ? 0.99f : (c < 0.0f ? 0.0f : c); }
  float svfG(float hz) const { const float g = 3.14159265f * hz / float(sr); return g > 0.9f ? 0.9f : (g < 0.0005f ? 0.0005f : g); }
  float lp(int c, int i, float x, float coef) { z[c][i] += coef * (x - z[c][i]); return z[c][i]; }
  void write(float l, float r) { bufL[size_t(w)] = l; bufR[size_t(w)] = r; w = (w + 1) & mask; }
  // d samples back from the next write position, between two samples
  float read(const std::vector<float>& b, float d) const {
    if (d < 1.0f) d = 1.0f;
    if (d > float(mask - 2)) d = float(mask - 2);
    const int di = int(d);
    const float fr = d - float(di);
    const int i0 = (w - di) & mask, i1 = (w - di - 1) & mask;
    return b[size_t(i0)] + fr * (b[size_t(i1)] - b[size_t(i0)]);
  }
  // state-variable filter; kq = 1 / Q. Returns the band-pass output, `low` gets the low-pass output.
  float svf(int c, float x, float g, float kq, float& low) {
    const float v1 = (ic1[c] + g * (x - ic2[c])) / (1.0f + g * (g + kq));
    const float v2 = ic2[c] + g * v1;
    ic1[c] = 2.0f * v1 - ic1[c]; ic2[c] = 2.0f * v2 - ic2[c];
    low = v2;
    return v1;
  }
  float band(int c, float x, float g, float kq) { float low; return svf(c, x, g, kq, low); }
  // one octave down: the note, turned over at every second wave. `two` gets two octaves down.
  float sub(int c, float x, float& two) {
    const float f = lp(c, 0, x, k[1]);
    if (prev[c] <= 0.0f && f > 0.0f) { flip[c] = -flip[c]; if (flip[c] > 0.0f) flip2[c] = -flip2[c]; }
    prev[c] = f;
    two = lp(c, 2, f * flip2[c], k[2]);
    return lp(c, 1, f * flip[c], k[2]);
  }
  float verb(int c, float x, float fb, float damp) {
    float sum = 0.0f;
    for (int i = 0; i < 4; ++i) {
      std::vector<float>& line = comb[c][i];
      const float out = line[size_t(ci[c][i])];
      cl[c][i] += damp * (out - cl[c][i]);        // the highs die first
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
