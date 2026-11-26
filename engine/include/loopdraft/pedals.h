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
    la = lr = l3 = l4 = -1.0f;
  }
  void sleep() { last = -1; }           // the next note finds it clean

  // Does this pedal keep sounding for seconds after its input stopped?
  static bool rings(FxType t) { return t == FxType::Delay || t == FxType::TapeEcho || t == FxType::Reverb; }

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
        advance(ph, (0.1f + rt * rt * 6.0f) / fs);
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
        advance(ph, (0.1f + rt * rt * 4.0f) / fs);
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
        advance(ph, (0.05f + rt * rt * 8.0f) / fs);
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
        advance(ph, (1.0f + rt * 13.0f) / fs);
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
        advance(ph, (voice == 1 ? 0.4f + rt * 2.0f : 2.0f + rt * 7.0f) / fs);
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
        advance(ph, (0.3f + rt * rt * 6.0f) / fs);
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
        const float target = (0.04f + rt * rt * 0.96f) * fs;
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
        const float target = (0.06f + rt * 0.54f) * fs;
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
      default: break;
    }
  }

 private:
  static float unit(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
  static float hardClip(float x) { return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x); }
  static void advance(float& phase, float by) { phase += by; if (phase >= 1.0f) phase -= 1.0f; }
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
