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

}  // namespace ld
