// Loop Draft engine - small DSP building blocks. All per-sample, no allocation while running.
#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

namespace ld {

constexpr double kPi = 3.14159265358979323846;

// One-pole parameter smoother, removes zipper noise from level changes.
struct Smooth {
  float y = 0.0f, a = 1.0f;
  void setTime(double sr, double ms) { a = static_cast<float>(1.0 - std::exp(-1.0 / (sr * ms * 0.001))); }
  void reset(float v) { y = v; }
  float next(float target) { y += a * (target - y); return y; }
};

// RBJ shelving biquad.
struct Shelf {
  float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
  void set(bool high, double sr, double freq, double dB) {
    const double A = std::pow(10.0, dB / 40.0), w = 2.0 * kPi * freq / sr;
    const double c = std::cos(w), s = std::sin(w), al = s / 2.0 * std::sqrt(2.0), q = 2.0 * std::sqrt(A) * al;
    double B0, B1, B2, A0, A1, A2;
    if (!high) {
      B0 = A * ((A + 1) - (A - 1) * c + q); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - q);
      A0 = (A + 1) + (A - 1) * c + q;       A1 = -2 * ((A - 1) + (A + 1) * c);    A2 = (A + 1) + (A - 1) * c - q;
    } else {
      B0 = A * ((A + 1) + (A - 1) * c + q); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - q);
      A0 = (A + 1) - (A - 1) * c + q;       A1 = 2 * ((A - 1) - (A + 1) * c);      A2 = (A + 1) - (A - 1) * c - q;
    }
    b0 = float(B0 / A0); b1 = float(B1 / A0); b2 = float(B2 / A0); a1 = float(A1 / A0); a2 = float(A2 / A0);
  }
  float tick(float x) {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};

// State-variable filter (trapezoidal, stable under fast modulation). Low-pass output.
struct Svf {
  float a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
  void set(double sr, double cutoff, double q) {
    if (cutoff < 20.0) cutoff = 20.0;
    if (cutoff > 0.45 * sr) cutoff = 0.45 * sr;
    const double g = std::tan(kPi * cutoff / sr), k = 1.0 / (q < 0.3 ? 0.3 : q);
    a1 = float(1.0 / (1.0 + g * (g + k))); a2 = float(g * a1); a3 = float(g * a2);
  }
  float low(float x) {
    const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
    return v2;
  }
  void reset() { ic1 = ic2 = 0; }
};

// Soft saturation. amount 0 is exactly clean.
struct Drive {
  float m = 0, g = 1, norm = 1;
  void set(float amount) {
    m = amount < 0 ? 0 : (amount > 1 ? 1 : amount);
    g = 1.0f + m * 24.0f; norm = 1.0f / std::tanh(g);
  }
  float tick(float x) const { return m == 0.0f ? x : (1.0f - m) * x + m * std::tanh(g * x) * norm; }
};

// Stereo ping-pong delay with a darkening feedback path.
struct Delay {
  std::vector<float> l, r;
  int size = 0, w = 0, d = 1;
  float lpL = 0, lpR = 0;
  void init(double sr) { size = int(sr * 2.0) + 1; l.assign(size, 0.0f); r.assign(size, 0.0f); w = 0; }
  void setTime(double sr, double seconds) {
    int n = int(std::lround(sr * seconds));
    d = n < 1 ? 1 : (n > size - 1 ? size - 1 : n);
  }
  void tick(float inL, float inR, float fb, float& outL, float& outR) {
    int rd = w - d; if (rd < 0) rd += size;
    outL = l[rd]; outR = r[rd];
    lpL += 0.45f * (outL - lpL); lpR += 0.45f * (outR - lpR);
    l[w] = inL + fb * lpR + 1e-20f;       // crossed feedback = ping-pong; tiny offset avoids denormals
    r[w] = inR + fb * lpL + 1e-20f;
    if (++w >= size) w = 0;
  }
};

// Freeverb-style algorithmic reverb (8 combs + 4 all-passes per channel). Output is wet only.
struct Reverb {
  struct Comb { std::vector<float> b; int i = 0; float store = 0; };
  struct Allp { std::vector<float> b; int i = 0; };
  Comb comb[2][8]; Allp allp[2][4];
  float feedback = 0.84f, damp = 0.25f;
  void init(double sr) {
    static const int ct[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static const int at[4] = {556, 441, 341, 225};
    const double k = sr / 44100.0;
    for (int ch = 0; ch < 2; ++ch) {
      for (int j = 0; j < 8; ++j) { comb[ch][j].b.assign(size_t((ct[j] + ch * 23) * k), 0.0f); comb[ch][j].i = 0; comb[ch][j].store = 0; }
      for (int j = 0; j < 4; ++j) { allp[ch][j].b.assign(size_t((at[j] + ch * 23) * k), 0.0f); allp[ch][j].i = 0; }
    }
  }
  void tick(float inL, float inR, float& outL, float& outR) {
    const float in = (inL + inR) * 0.015f;
    float o[2] = {0, 0};
    for (int ch = 0; ch < 2; ++ch) {
      for (int j = 0; j < 8; ++j) {
        Comb& c = comb[ch][j];
        const float y = c.b[c.i];
        c.store = y * (1.0f - damp) + c.store * damp + 1e-20f;
        c.b[c.i] = in + c.store * feedback;
        if (++c.i >= int(c.b.size())) c.i = 0;
        o[ch] += y;
      }
      for (int j = 0; j < 4; ++j) {
        Allp& a = allp[ch][j];
        const float bo = a.b[a.i], y = bo - o[ch];
        a.b[a.i] = o[ch] + bo * 0.5f;
        if (++a.i >= int(a.b.size())) a.i = 0;
        o[ch] = y;
      }
    }
    outL = o[0] * 3.0f; outR = o[1] * 3.0f;
  }
};

// Brickwall peak limiter: instant attack, smooth release. Output never exceeds the ceiling.
struct Limiter {
  float env = 0, rel = 0.999f, ceiling = 0.944f;    // -0.5 dBFS
  void init(double sr) { rel = float(std::exp(-1.0 / (sr * 0.08))); env = 0; }
  void tick(float& l, float& r) {
    const float pk = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);
    env = pk > env * rel ? pk : env * rel;
    if (env > ceiling) { const float g = ceiling / env; l *= g; r *= g; }
  }
};

// Band-limited oscillator step (polyBLEP), removes most aliasing from saw and square.
inline float polyBlep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}

// Small deterministic random generator, so renders are repeatable bit for bit.
struct Rng {
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
  float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return float(s) / 4294967296.0f * 2.0f - 1.0f; }
};

// Lo-fi bitcrusher + sample-rate reducer (Vital-style Distortion/Crush).
struct Bitcrusher {
  float holdL = 0, holdR = 0, phase = 0;
  void reset() { holdL = holdR = phase = 0; }
  void tick(float& l, float& r, float amount, float rate) {
    const float steps = 2.0f + (1.0f - amount) * 30.0f;
    const float inc = 0.05f + (1.0f - rate) * 0.95f;
    phase += inc;
    if (phase >= 1.0f) {
      phase -= 1.0f;
      holdL = std::round(l * steps) / steps;
      holdR = std::round(r * steps) / steps;
    }
    l = l * (1.0f - amount) + holdL * amount;
    r = r * (1.0f - amount) + holdR * amount;
  }
};

// Stereo modulated Chorus (short 35 ms ring buffer, zero alloc after init).
struct Chorus {
  std::vector<float> bufL, bufR;
  int size = 1, w = 0;
  float ph = 0.0f;
  double sr = 48000.0;
  void init(double sampleRate) {
    sr = sampleRate;
    size = int(sr * 0.04) + 16;
    bufL.assign(size, 0.0f); bufR.assign(size, 0.0f);
    w = 0; ph = 0.0f;
  }
  void tick(float& l, float& r, float amount, float rate) {
    bufL[w] = l; bufR[w] = r;
    const float hz = 0.15f + rate * 3.5f;
    ph += hz / float(sr);
    if (ph >= 1.0f) ph -= 1.0f;
    const float modL = 0.5f + 0.5f * std::sin(2.0f * float(kPi) * ph);
    const float modR = 0.5f + 0.5f * std::sin(2.0f * float(kPi) * (ph + 0.25f));
    const auto readTap = [&](const std::vector<float>& b, float dMs) {
      float dSamp = dMs * 0.001f * float(sr);
      if (dSamp > size - 2) dSamp = float(size - 2);
      float rd = float(w) - dSamp;
      if (rd < 0.0f) rd += float(size);
      int i0 = int(rd) % size;
      int i1 = (i0 + 1) % size;
      float f = rd - std::floor(rd);
      return b[i0] + f * (b[i1] - b[i0]);
    };
    const float wL = readTap(bufL, 6.0f + modL * 14.0f);
    const float wR = readTap(bufR, 6.0f + modR * 14.0f);
    if (++w >= size) w = 0;
    const float mix = amount * 0.65f;
    l = l * (1.0f - mix) + wL * mix;
    r = r * (1.0f - mix) + wR * mix;
  }
};

// 4-stage stereo all-pass Phaser.
struct Phaser {
  float aL[4] = {}, aR[4] = {}, ph = 0.0f;
  double sr = 48000.0;
  void init(double sampleRate) { sr = sampleRate; ph = 0.0f; for (int i = 0; i < 4; ++i) aL[i] = aR[i] = 0.0f; }
  void tick(float& l, float& r, float amount, float rate) {
    const float hz = 0.1f + rate * 4.0f;
    ph += hz / float(sr); if (ph >= 1.0f) ph -= 1.0f;
    const float lfo = 0.5f + 0.5f * std::sin(2.0f * float(kPi) * ph);
    const float f = 220.0f + lfo * 2400.0f;
    const float a = float((1.0 - std::tan(kPi * f / sr)) / (1.0 + std::tan(kPi * f / sr)));
    float yL = l, yR = r;
    for (int i = 0; i < 4; ++i) {
      float tL = a * yL + aL[i]; aL[i] = yL - a * tL; yL = tL;
      float tR = a * yR + aR[i]; aR[i] = yR - a * tR; yR = tR;
    }
    const float mix = amount * 0.6f;
    l = l * (1.0f - mix) + yL * mix;
    r = r * (1.0f - mix) + yR * mix;
  }
};

// Punchy OTT-style track compressor.
struct TrackComp {
  float env = 0.0f, att = 0.01f, rel = 0.001f;
  void init(double sr) {
    env = 0.0f;
    att = float(1.0 - std::exp(-1.0 / (sr * 0.005)));
    rel = float(1.0 - std::exp(-1.0 / (sr * 0.060)));
  }
  void tick(float& l, float& r, float amount) {
    const float pk = std::fabs(l) > std::fabs(r) ? std::fabs(l) : std::fabs(r);
    env += (pk > env ? att : rel) * (pk - env);
    const float thr = 0.45f - amount * 0.30f;
    float gain = 1.0f;
    if (env > thr && env > 1e-5f) {
      const float ratio = 1.0f + amount * 6.0f;
      const float over = env / thr;
      gain = std::pow(over, (1.0f / ratio) - 1.0f);
    }
    const float makeup = 1.0f + amount * 0.75f;
    l *= gain * makeup; r *= gain * makeup;
  }
};

}  // namespace ld
