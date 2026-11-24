#include "loopdraft/voices.h"

#include <cmath>
#include <cstring>

namespace ld {

// ------------------------------------------------------------------ Sampler

void Sampler::init(double sr) {
  sr_ = sr;
  chokeMul_ = float(std::exp(std::log(1e-3) / (0.008 * sr)));   // choked voices fade out in 8 ms
  for (auto& v : v_) v = Voice{};
  clock_ = 0;
}

void Sampler::trigger(int pad, const PadParams& p, const Sample* s, int vel) {
  if (!s || s->frames() < 2) return;
  if (p.choke > 0)
    for (auto& v : v_)
      if (v.active && v.choke == p.choke) v.envMul = chokeMul_;
  Voice* t = nullptr;
  for (auto& v : v_) if (!v.active) { t = &v; break; }
  if (!t) { t = &v_[0]; for (auto& v : v_) if (v.age < t->age) t = &v; }   // steal the oldest
  const float g = p.vol * velGain(vel);
  const float a = (p.pan < -1 ? -1 : (p.pan > 1 ? 1 : p.pan)) * 0.25f * float(kPi) + 0.25f * float(kPi);
  const double lastF = double(s->frames() - 1);
  float s0 = p.sliceStart < 0.0f ? 0.0f : (p.sliceStart > 0.99f ? 0.99f : p.sliceStart);
  float s1 = p.sliceEnd <= s0 + 0.01f ? (s0 + 0.01f > 1.0f ? 1.0f : s0 + 0.01f) : (p.sliceEnd > 1.0f ? 1.0f : p.sliceEnd);
  const double startPos = double(s0) * lastF;
  const double endPos = double(s1) * lastF;
  const double speed = std::pow(2.0, double(p.pitch) / 12.0) * s->rate / sr_;
  t->s = s;
  t->reverse = p.reverse;
  if (p.reverse) { t->pos = endPos; t->endPos = startPos; t->inc = -speed; }
  else           { t->pos = startPos; t->endPos = endPos; t->inc = speed; }
  t->gl = g * 1.41421356f * std::cos(a); t->gr = g * 1.41421356f * std::sin(a);
  t->env = 1.0f;
  const double dec = p.decay < 0.01f ? 0.01 : double(p.decay);
  t->envMul = float(std::exp(std::log(1e-3) / (dec * sr_)));
  t->pad = pad; t->choke = p.choke; t->age = ++clock_; t->active = true;
}

void Sampler::stopPad(int pad) {
  for (auto& v : v_) if (v.pad == pad) { v.active = false; v.s = nullptr; }
}

void Sampler::process(float* l, float* r, int n) {
  for (auto& v : v_) {
    if (!v.active) continue;
    const float* sl = v.s->l.data();
    const float* sr = v.s->r.empty() ? sl : v.s->r.data();
    const size_t last = v.s->frames() - 1;
    for (int i = 0; i < n; ++i) {
      if (v.reverse ? (v.pos <= v.endPos || v.pos < 0.0) : (v.pos >= v.endPos || v.pos >= double(last))) {
        v.active = false; break;
      }
      const size_t idx = size_t(v.pos);
      const size_t idx1 = idx < last ? idx + 1 : last;
      const float f = float(v.pos - double(idx));
      const float xl = sl[idx] + f * (sl[idx1] - sl[idx]);
      const float xr = sr[idx] + f * (sr[idx1] - sr[idx]);
      l[i] += xl * v.gl * v.env; r[i] += xr * v.gr * v.env;
      v.env *= v.envMul; v.pos += v.inc;
      if (v.env < 1e-4f) { v.active = false; break; }
    }
  }
}

// ------------------------------------------------------------------ Synth

namespace {
inline float osc(Wave w, float t, float dt) {
  switch (w) {
    case Wave::Saw: return 2.0f * t - 1.0f - polyBlep(t, dt);
    case Wave::Square: {
      float v = t < 0.5f ? 1.0f : -1.0f;
      v += polyBlep(t, dt);
      float t2 = t + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
      return v - polyBlep(t2, dt);
    }
    case Wave::Triangle: return 4.0f * std::fabs(t - 0.5f) - 1.0f;
    default: return std::sin(2.0f * float(kPi) * t);
  }
}
}  // namespace

void Synth::init(double sr) {
  sr_ = sr; clock_ = 0;
  for (auto& v : v_) v = Voice{};
  strings_.assign(size_t(kSynthVoices) * kStringLen, 0.0f);
}

void Synth::noteOn(int midi, int vel, int64_t gateFrames, int voicesInChord, int tag, bool slide) {
  const float freq = 440.0f * std::pow(2.0f, float(midi - 69) / 12.0f);
  Voice* t = nullptr;
  if (slide && voicesInChord <= 1) {               // 303 style: the newest sounding note bends over, its envelope carries on
    for (auto& v : v_) if (v.stage != Off && (!t || v.born > t->born)) t = &v;
    if (t) {
      t->target = freq; t->tag = tag; t->gate = gateFrames;
      if (t->stage == Release) t->stage = Decay;
      if (vel >= 2) t->acc = 1.0f;
      return;
    }
  }
  for (auto& v : v_) if (v.stage == Off) { t = &v; break; }
  if (!t) {                                      // prefer the quietest releasing voice, else the oldest
    for (auto& v : v_) if (v.stage == Release && (!t || v.env < t->env)) t = &v;
    if (!t) { t = &v_[0]; for (auto& v : v_) if (v.born < t->born) t = &v; }
  }
  *t = Voice{};
  t->stage = Attack; t->tag = tag;
  t->freq = t->target = freq;
  t->acc = vel >= 2 ? 1.0f : 0.0f;
  t->gain = 0.6f * velGain(vel) / std::sqrt(float(voicesInChord < 1 ? 1 : voicesInChord));
  t->gate = gateFrames; t->cut = -1.0f; t->born = ++clock_;
}

void Synth::noteOff(int tag) {
  for (auto& v : v_) if (v.stage != Off && v.gate < 0 && v.tag == tag) v.stage = Release;
}

void Synth::allNotesOff() {
  for (auto& v : v_) if (v.stage != Off) v.stage = Release;
}

void Synth::process(float* out, int n, const SynthParams& p) {
  const float sr = float(sr_);
  const float attInc = 1.0f / ((p.attack < 0.0005f ? 0.0005f : p.attack) * sr);
  const float decMul = std::exp(std::log(1e-3f) / ((p.decay < 0.005f ? 0.005f : p.decay) * sr));
  const float relMul = std::exp(std::log(1e-3f) / ((p.release < 0.005f ? 0.005f : p.release) * sr));
  const float sus = p.sustain < 0 ? 0 : (p.sustain > 1 ? 1 : p.sustain);
  const float effDetune = p.detune + (p.feelMotion - 0.3f) * 22.0f;
  const float detuneMul = std::pow(2.0f, effDetune / 1200.0f);
  const float modDecMul = std::exp(std::log(1e-3f) / ((p.fmModDecay < 0.005f ? 0.005f : p.fmModDecay) * sr));
  const float lfoInc = 16.0f * p.lfoRate / sr;
  const float effLfoDepth = (p.lfoDepth + (p.feelMotion - 0.3f) * 0.35f) < 0.0f ? 0.0f : ((p.lfoDepth + (p.feelMotion - 0.3f) * 0.35f) > 1.0f ? 1.0f : (p.lfoDepth + (p.feelMotion - 0.3f) * 0.35f));
  const bool lfoOn = p.lfoTarget != LfoTarget::Off && effLfoDepth > 0.0f;
  const float slideK = 1.0f - std::exp(-16.0f / ((p.slideTime < 0.005f ? 0.005f : p.slideTime) * sr));
  const float accMul = std::exp(-16.0f / (0.12f * sr));
  const float foldGain = 1.0f + 5.0f * (p.fold < 0 ? 0 : (p.fold > 1 ? 1 : p.fold));
  const bool pluck = p.mode == SynthMode::Pluck, vector = p.mode == SynthMode::Vector;
  const float bright = p.pluckBright < 0 ? 0 : (p.pluckBright > 1 ? 1 : p.pluckBright);
  const float ksBlend = 0.5f * (1.0f - bright);      // how much each trip round the string smooths it

  for (auto& v : v_) {
    if (v.stage == Off) continue;
    float* const str = strings_.data() + size_t(&v - v_) * kStringLen;
    for (int i = 0; i < n; ++i) {
      // Control-rate work every 16 samples, counted from the note start. Tying it to the note's own age
      // (not to the audio block) keeps the output identical for any block size.
      if ((v.age & 15u) == 0) {
        // feelWarmth gently darkens high-end harshness and adds subtle analog warmth
        float cutoff = p.cutoff * std::pow(2.0f, (0.5f - p.feelWarmth) * 1.1f), pitchMul = 1.0f, ampT = 1.0f;
        if (lfoOn) {
          const float lfo = std::sin(2.0f * float(kPi) * v.lfoPh);
          v.lfoPh += lfoInc; if (v.lfoPh >= 1.0f) v.lfoPh -= std::floor(v.lfoPh);
          if (p.lfoTarget == LfoTarget::Cutoff) cutoff *= std::pow(2.0f, lfo * effLfoDepth * 1.5f);
          else if (p.lfoTarget == LfoTarget::Pitch) pitchMul = std::pow(2.0f, lfo * effLfoDepth * 100.0f / 1200.0f);
          else ampT = 1.0f + 0.5f * effLfoDepth * lfo;
        }
        if (v.acc > 0.001f) { cutoff *= std::pow(2.0f, 1.1f * v.acc); v.acc *= accMul; }
        if (cutoff > 16000.0f) cutoff = 16000.0f;
        v.freq += slideK * (v.target - v.freq);
        v.cut = v.cut < 0.0f ? cutoff : v.cut + 0.6f * (cutoff - v.cut);
        v.flt.set(sr_, v.cut, p.res);
        v.inc1 = v.freq * pitchMul / sr;
        if (vector) {
          v.inc3 = v.inc1 * p.fmRatio; v.inc4 = v.inc1 * 0.5f;
          const float tx = p.vecX < 0 ? 0 : (p.vecX > 1 ? 1 : p.vecX), ty = p.vecY < 0 ? 0 : (p.vecY > 1 ? 1 : p.vecY);
          v.vx = v.vx < 0.0f ? tx : v.vx + 0.25f * (tx - v.vx);
          v.vy = v.vy < 0.0f ? ty : v.vy + 0.25f * (ty - v.vy);
        }
        if (pluck) {
          const float f = v.freq * pitchMul < 12.0f ? 12.0f : v.freq * pitchMul;
          float d = sr / f - ksBlend - 1.0f;         // the loop filter and the interpolation add their own delay
          v.ksDelay = d < 2.0f ? 2.0f : (d > float(kStringLen - 4) ? float(kStringLen - 4) : d);
          // the Decay knob is how long the string rings (to -60 dB)
          const float t60 = (p.decay < 0.03f ? 0.03f : p.decay) * 4.0f;
          v.ksGain = std::pow(10.0f, -3.0f / (f * t60));
          if (!v.ksReady) {                          // pluck the string: a burst of noise one period long
            Rng rng(0x9E37u + uint32_t(v.born) * 2654435761u);
            const int len = int(v.ksDelay) + 2;
            float lp = 0.0f;
            const float a = 0.25f + 0.75f * bright;
            for (int k = 0; k < kStringLen; ++k) str[k] = 0.0f;
            for (int k = 0; k < len; ++k) { lp += a * (rng.next() - lp); str[k] = lp; }
            v.ksPos = len; v.ksLast = 0.0f; v.ksReady = true;
          }
        }
        v.inc2 = (p.mode == SynthMode::Fm) ? (v.inc1 * p.fmRatio) : (v.inc1 * detuneMul);
        v.ampStep = (ampT - v.amp) / 16.0f;
      }
      v.amp += v.ampStep;
      float x;
      if (p.mode == SynthMode::Fm) {
        const float mod = std::sin(2.0f * float(kPi) * v.ph2 + p.fmFeedback * 2.5f * v.lastMod);
        v.lastMod = mod;
        x = std::sin(2.0f * float(kPi) * v.ph1 + p.fmIndex * v.modEnv * mod);
        v.modEnv *= modDecMul;
      } else if (pluck) {
        float rp = float(v.ksPos) - v.ksDelay;
        if (rp < 0.0f) rp += float(kStringLen);
        const int i0 = int(rp);
        const float fr = rp - float(i0);
        const float y0 = str[i0 & (kStringLen - 1)], y1 = str[(i0 + 1) & (kStringLen - 1)];
        x = y0 + fr * (y1 - y0);
        str[v.ksPos & (kStringLen - 1)] = v.ksGain * ((1.0f - ksBlend) * x + ksBlend * v.ksLast);
        v.ksLast = x;
        v.ksPos = (v.ksPos + 1) & (kStringLen - 1);
      } else if (vector) {
        const float mod = std::sin(2.0f * float(kPi) * v.ph3 + p.fmFeedback * 2.5f * v.lastMod);
        v.lastMod = mod;
        const float a = osc(p.wave1, v.ph1, v.inc1), b = osc(p.wave2, v.ph2, v.inc2);
        const float c = std::sin(2.0f * float(kPi) * v.ph1 + p.fmIndex * v.modEnv * mod);
        const float d = osc(Wave::Square, v.ph4, v.inc4);
        v.modEnv *= modDecMul;
        x = 0.8f * ((1.0f - v.vx) * ((1.0f - v.vy) * a + v.vy * c) + v.vx * ((1.0f - v.vy) * b + v.vy * d));
        v.ph3 += v.inc3; if (v.ph3 >= 1.0f) v.ph3 -= std::floor(v.ph3);
        v.ph4 += v.inc4; if (v.ph4 >= 1.0f) v.ph4 -= std::floor(v.ph4);
      } else {
        x = 0.5f * (osc(p.wave1, v.ph1, v.inc1) + osc(p.wave2, v.ph2, v.inc2));
      }
      if (foldGain > 1.0f) x = std::sin(x * foldGain * 0.5f * float(kPi));   // wavefolder: louder input wraps round
      v.ph1 += v.inc1; if (v.ph1 >= 1.0f) v.ph1 -= std::floor(v.ph1);
      v.ph2 += v.inc2; if (v.ph2 >= 1.0f) v.ph2 -= std::floor(v.ph2);
      float y = v.flt.low(x);
      if (p.feelWarmth > 0.55f) { const float g = 1.0f + (p.feelWarmth - 0.55f) * 3.5f; y = std::tanh(y * g) / std::tanh(g); }

      if (v.stage == Attack && pluck) { v.env = 1.0f; v.stage = Decay; }
      else if (v.stage == Attack) { v.env += attInc; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Decay; } }
      else if (v.stage == Decay && pluck) { if ((v.age & 1023u) == 0 && float(v.age) > (p.decay * 4.0f + 0.5f) * 1.6f * sr) { v.stage = Off; break; } }
      else if (v.stage == Decay) { v.env = sus + (v.env - sus) * decMul; if (sus <= 0.0f && v.env < 1e-4f) { v.stage = Off; break; } }
      else { v.env *= relMul; if (v.env < 1e-4f) { v.stage = Off; break; } }
      if (v.gate > 0 && v.stage != Release && --v.gate == 0) v.stage = Release;

      out[i] += y * v.env * v.gain * p.level * v.amp;
      ++v.age;
    }
  }
}

// ------------------------------------------------------------------ built-in drum kit

std::unique_ptr<Sample> makeBuiltinSample(int pad, double sr) {
  static const double lengths[8] = {0.9, 0.5, 0.5, 0.3, 1.0, 0.2, 0.8, 0.4};
  auto s = std::make_unique<Sample>();
  s->rate = sr;
  const size_t n = size_t(lengths[pad & 7] * sr);
  s->l.assign(n, 0.0f);
  Rng rng(1000u + uint32_t(pad));
  double ph = 0.0;
  float lp1 = 0, lp2 = 0;
  const auto onePole = [&](double hz) { return float(1.0 - std::exp(-2.0 * kPi * hz / sr)); };
  for (size_t i = 0; i < n; ++i) {
    const double t = double(i) / sr;
    float x = 0;
    switch (pad & 7) {
      case 0: ph += (42.0 + 98.0 * std::exp(-t / 0.05)) / sr; x = float(std::sin(2 * kPi * ph) * std::exp(-t * 4.0)); break;
      case 6: ph += (85.0 + 125.0 * std::exp(-t / 0.06)) / sr; x = float(std::sin(2 * kPi * ph) * std::exp(-t * 4.5)); break;
      case 1: {
        const float nz = rng.next();
        lp1 += onePole(900) * (nz - lp1); lp2 += onePole(6000) * ((nz - lp1) - lp2);
        ph += (150.0 + 40.0 * std::exp(-t / 0.03)) / sr;
        const double tri = 4.0 * std::fabs(ph - std::floor(ph) - 0.5) - 1.0;
        x = float((lp2 * 0.9 + tri * 0.5 * std::exp(-t * 18.0)) * std::exp(-t * 9.0));
        break;
      }
      case 2: {
        const float nz = rng.next();
        lp1 += onePole(700) * (nz - lp1); lp2 += onePole(3500) * ((nz - lp1) - lp2);
        double e = 0.6 * std::exp(-t * 14.0);
        for (double t0 : {0.0, 0.011, 0.022}) if (t >= t0) e = std::fmax(e, std::exp(-(t - t0) * 120.0));
        x = float(lp2 * e);
        break;
      }
      case 3: case 4: {
        const float nz = rng.next();
        lp1 += onePole(6000) * (nz - lp1);
        x = float((nz - lp1) * std::exp(-t * 5.0));
        break;
      }
      case 5: {
        const float nz = rng.next();
        lp1 += onePole(2500) * (nz - lp1);
        ph += 820.0 / sr;
        const double tri = 4.0 * std::fabs(ph - std::floor(ph) - 0.5) - 1.0;
        x = float(tri * std::exp(-t * 60.0) + (nz - lp1) * 0.6 * std::exp(-t * 200.0));
        break;
      }
      default: ph += (420.0 + 120.0 * std::exp(-t / 0.04)) / sr; x = float(std::sin(2 * kPi * ph) * std::exp(-t * 12.0)); break;
    }
    s->l[i] = x;
  }
  float pk = 1e-9f;
  for (float x : s->l) pk = std::fmax(pk, std::fabs(x));
  for (float& x : s->l) x *= 0.9f / pk;
  return s;
}

std::unique_ptr<Sample> makeJungleBreakSample(double sr) {
  // 1-bar Amen-inspired breakbeat at 168 BPM (approx 1.428 s) split cleanly into 8 slices
  const double barSec = 60.0 / 168.0 * 4.0;
  const size_t n = size_t(std::lround(barSec * sr));
  auto s = std::make_unique<Sample>();
  s->rate = sr;
  s->l.assign(n, 0.0f);
  const double stepSec = barSec / 16.0;
  Rng rng(909808u);
  auto addHit = [&](int step, int kind, float amp) {
    const size_t start = size_t(std::lround(step * stepSec * sr));
    const size_t len = size_t(0.18 * sr);
    double ph = 0.0;
    float lp1 = 0, lp2 = 0;
    const auto op = [&](double hz) { return float(1.0 - std::exp(-2.0 * kPi * hz / sr)); };
    for (size_t i = 0; i < len && start + i < n; ++i) {
      const double t = double(i) / sr;
      float x = 0.0f;
      if (kind == 0) { // punchy break kick
        ph += (48.0 + 130.0 * std::exp(-t / 0.035)) / sr;
        x = float(std::sin(2 * kPi * ph) * std::exp(-t * 9.0));
      } else if (kind == 1) { // crisp snappy jungle snare
        const float nz = rng.next();
        lp1 += op(1200) * (nz - lp1); lp2 += op(7500) * ((nz - lp1) - lp2);
        ph += (195.0 + 45.0 * std::exp(-t / 0.02)) / sr;
        x = float((lp2 * 1.1 + std::sin(2 * kPi * ph) * 0.45 * std::exp(-t * 20.0)) * std::exp(-t * 14.0));
      } else { // bright ride / shuffle hat
        const float nz = rng.next();
        lp1 += op(6800) * (nz - lp1);
        x = float((nz - lp1) * std::exp(-t * 18.0));
      }
      s->l[start + i] += x * amp;
    }
  };
  // Classic 16-step Amen pattern
  for (int st : {0, 2, 10, 11}) addHit(st, 0, st == 0 ? 1.0f : 0.85f);
  for (int st : {4, 12})        addHit(st, 1, 0.95f);
  for (int st : {7, 9, 15})     addHit(st, 1, 0.42f); // ghost snares
  for (int st = 0; st < 16; st += 2) addHit(st, 2, 0.45f);
  for (int st : {1, 3, 5, 13, 14})   addHit(st, 2, 0.25f);
  float pk = 1e-9f;
  for (float x : s->l) pk = std::fmax(pk, std::fabs(x));
  for (float& x : s->l) x = std::tanh(x * 1.25f / pk) * 0.92f; // subtle vintage sampler crunch
  return s;
}

std::unique_ptr<Sample> makeDrumKitSample(DrumEngine engine, int pad, double sr) {
  if (engine == DrumEngine::Classic808) return makeBuiltinSample(pad, sr);
  auto s = std::make_unique<Sample>();
  s->rate = sr;
  const int p = pad & 7;
  Rng rng(2000u + uint32_t(engine) * 100u + uint32_t(p));
  double ph = 0.0, ph2 = 0.0;
  float lp1 = 0.0f, lp2 = 0.0f;
  const auto op = [&](double hz) { return float(1.0 - std::exp(-2.0 * kPi * hz / sr)); };

  if (engine == DrumEngine::Punch909) {
    static const double lens[8] = {0.55, 0.35, 0.38, 0.18, 0.75, 0.16, 0.55, 0.35};
    const size_t n = size_t(lens[p] * sr);
    s->l.assign(n, 0.0f);
    for (size_t i = 0; i < n; ++i) {
      const double t = double(i) / sr;
      float x = 0.0f;
      switch (p) {
        case 0: { // 909 Kick: punchy click + fast pitch drop
          ph += (50.0 + 180.0 * std::exp(-t / 0.018)) / sr;
          const float click = rng.next() * float(std::exp(-t * 320.0)) * 0.45f;
          x = float(std::tanh((std::sin(2 * kPi * ph) * 1.35 + click) * std::exp(-t * 7.0)));
          break;
        }
        case 1: { // 909 Snare: snappy body + bright noise
          const float nz = rng.next();
          lp1 += op(1400) * (nz - lp1); lp2 += op(9000) * ((nz - lp1) - lp2);
          ph += (210.0 + 60.0 * std::exp(-t / 0.015)) / sr;
          x = float(lp2 * 1.15 * std::exp(-t * 11.0) + std::sin(2 * kPi * ph) * 0.65 * std::exp(-t * 22.0));
          break;
        }
        case 2: { // 909 Clap
          const float nz = rng.next();
          lp1 += op(900) * (nz - lp1); lp2 += op(4500) * ((nz - lp1) - lp2);
          double e = 0.55 * std::exp(-t * 16.0);
          for (double t0 : {0.0, 0.009, 0.018, 0.027}) if (t >= t0) e = std::fmax(e, std::exp(-(t - t0) * 140.0));
          x = float(lp2 * e);
          break;
        }
        case 3: case 4: { // 909 Metallic Hat
          const float nz = rng.next();
          lp1 += op(7500) * (nz - lp1);
          ph += 3120.0 / sr; ph2 += 4780.0 / sr;
          const float ring = float((std::sin(2 * kPi * ph) > 0 ? 1 : -1) * (std::sin(2 * kPi * ph2) > 0 ? 1 : -1));
          x = float(((nz - lp1) * 0.8 + ring * 0.25) * std::exp(-t * (p == 3 ? 28.0 : 5.5)));
          break;
        }
        case 5: { // 909 Rimshot
          ph += 1150.0 / sr; ph2 += 490.0 / sr;
          x = float((std::sin(2 * kPi * ph) * 0.7 + std::sin(2 * kPi * ph2) * 0.6) * std::exp(-t * 48.0));
          break;
        }
        case 6: { // 909 Tom
          ph += (95.0 + 140.0 * std::exp(-t / 0.04)) / sr;
          x = float(std::sin(2 * kPi * ph) * std::exp(-t * 6.5));
          break;
        }
        default: { // 909 Ride / Perc
          const float nz = rng.next();
          lp1 += op(5200) * (nz - lp1);
          ph += 1840.0 / sr;
          x = float(((nz - lp1) * 0.6 + std::sin(2 * kPi * ph) * 0.4) * std::exp(-t * 9.0));
          break;
        }
      }
      s->l[i] = x;
    }
  } else if (engine == DrumEngine::LoFiAcoustic) {
    static const double lens[8] = {0.65, 0.42, 0.35, 0.22, 0.70, 0.20, 0.60, 0.35};
    const size_t n = size_t(lens[p] * sr);
    s->l.assign(n, 0.0f);
    float hold = 0.0f;
    for (size_t i = 0; i < n; ++i) {
      const double t = double(i) / sr;
      float x = 0.0f;
      if (p == 0) {
        ph += (55.0 + 75.0 * std::exp(-t / 0.04)) / sr;
        x = float(std::sin(2 * kPi * ph) * std::exp(-t * 6.5));
      } else if (p == 1 || p == 2) {
        const float nz = rng.next();
        lp1 += op(600) * (nz - lp1); lp2 += op(2800) * ((nz - lp1) - lp2);
        ph += 175.0 / sr;
        x = float((lp2 * 0.9 + std::sin(2 * kPi * ph) * 0.45) * std::exp(-t * 11.0));
      } else if (p == 3 || p == 4) {
        const float nz = rng.next();
        lp1 += op(3500) * (nz - lp1); lp2 += op(8000) * ((nz - lp1) - lp2);
        x = float(lp2 * std::exp(-t * (p == 3 ? 22.0 : 6.0)));
      } else {
        ph += (160.0 + p * 70.0) / sr;
        x = float(std::sin(2 * kPi * ph) * std::exp(-t * 14.0));
      }
      // Subtle vintage 12-bit lo-fi warmth & sample-hold grit
      if ((i & 1u) == 0) hold = std::round(x * 32.0f) / 32.0f;
      s->l[i] = std::tanh(hold * 1.4f);
    }
  } else { // FmIndustrial
    static const double lens[8] = {0.60, 0.40, 0.35, 0.22, 0.65, 0.25, 0.55, 0.45};
    const size_t n = size_t(lens[p] * sr);
    s->l.assign(n, 0.0f);
    const double ratios[8] = {1.5, 2.71, 3.41, 5.13, 6.27, 4.19, 1.41, 2.83};
    const double baseF[8]  = {58.0, 165.0, 290.0, 820.0, 640.0, 510.0, 110.0, 340.0};
    for (size_t i = 0; i < n; ++i) {
      const double t = double(i) / sr;
      const double f = baseF[p] * (1.0 + (p == 0 || p == 6 ? 2.2 * std::exp(-t / 0.025) : 0.2 * std::exp(-t / 0.03)));
      ph += f / sr;
      ph2 += (f * ratios[p]) / sr;
      const double idx = (p == 0 ? 3.5 : 6.5) * std::exp(-t * 12.0);
      const float fm = float(std::sin(2 * kPi * ph + idx * std::sin(2 * kPi * ph2)));
      s->l[i] = float(std::tanh(fm * 1.3f) * std::exp(-t * (p == 3 ? 24.0 : 8.5)));
    }
  }

  float pk = 1e-9f;
  for (float x : s->l) pk = std::fmax(pk, std::fabs(x));
  for (float& x : s->l) x *= 0.9f / pk;
  return s;
}

}  // namespace ld
