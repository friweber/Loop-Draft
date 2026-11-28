// A row of pedals for sound that comes in from outside: a guitar, a bass, a synth. The same pedals, with the same
// characters, as on a track of the groovebox (see engine.cpp), in as many slots as a real board has room for.
#pragma once

#include <cmath>
#include "dsp.h"
#include "pedals.h"
#include "types.h"

namespace ld {

constexpr int kBoardSlots = 10;

class PedalChain {
 public:
  void prepare(double sampleRate) {
    sr_ = sampleRate;
    subK_ = 1.0f - std::exp(-6.2831853f * 120.0f / float(sr_));
    for (auto& st : st_) { st.chorus.init(sr_); st.phaser.init(sr_); st.comp.init(sr_); st.pedal.init(sr_); st.pedal.sleep(); st.sub[0][0] = st.sub[0][1] = st.sub[1][0] = st.sub[1][1] = 0.0f; }
    for (auto& t : lastType_) t = -1;
  }
  // slots: what is on the board, in order. beat: where the host's song stands, if it says (haveBeat).
  void process(float* l, float* r, int n, const FxSlot* slots, int count, float bpm, bool haveBeat, double beat) {
    if (count > kBoardSlots) count = kBoardSlots;
    for (int sl = 0; sl < count; ++sl) {
      const FxSlot& fs = slots[sl];
      State& st = st_[sl];
      const int key = fs.bypass ? -1 : int(fs.type) * 8 + int(fs.voice);
      if (key != lastType_[sl]) { st.pedal.sleep(); lastType_[sl] = key; }       // another pedal in this slot starts clean
      if (fs.bypass || fs.type == FxType::Off) continue;
      st.pedal.setClock(bpm, haveBeat, beat, (fs.flags & kFxSync) != 0, nullptr);
      const bool keepSub = (fs.flags & kFxKeepSub) != 0;
      const bool classic = fs.voice == 0 && uint8_t(fs.type) <= uint8_t(FxType::Compressor);
      if (classic && fs.type == FxType::Overdrive) st.drive.set(fs.amount);
      if (classic && fs.type == FxType::Filter) {
        const float cf = 100.0f * std::pow(100.0f, fs.amount), q = 0.7f + fs.rate * 8.0f;
        st.flt[0].set(sr_, cf, q); st.flt[1].set(sr_, cf, q);
      }
      for (int i = 0; i < n; ++i) {
        float xl = l[i], xr = r[i];
        const float inL = xl, inR = xr;
        float lowMid = 0.0f;
        if (keepSub) {
          st.sub[0][0] += subK_ * (xl - st.sub[0][0]); st.sub[0][1] += subK_ * (st.sub[0][0] - st.sub[0][1]);
          st.sub[1][0] += subK_ * (xr - st.sub[1][0]); st.sub[1][1] += subK_ * (st.sub[1][0] - st.sub[1][1]);
          xl -= st.sub[0][1]; xr -= st.sub[1][1];
          lowMid = 0.5f * (st.sub[0][1] + st.sub[1][1]);
        }
        if (classic) {
          switch (fs.type) {
            case FxType::Overdrive: xl = st.drive.tick(xl); xr = st.drive.tick(xr); break;
            case FxType::Bitcrush: st.crush.tick(xl, xr, fs.amount, fs.rate); break;
            case FxType::Chorus: st.chorus.tick(xl, xr, fs.amount, fs.rate); break;
            case FxType::Phaser: st.phaser.tick(xl, xr, fs.amount, fs.rate); break;
            case FxType::Filter: xl = st.flt[0].low(xl); xr = st.flt[1].low(xr); break;
            case FxType::Compressor: st.comp.tick(xl, xr, fs.amount); break;
            default: break;
          }
          if (fs.p3 != 0.5f) { xl *= fs.p3 * 2.0f; xr *= fs.p3 * 2.0f; }
        } else {
          st.pedal.tick(fs.type, fs.voice, xl, xr, fs.amount, fs.rate, fs.p3, fs.p4);
        }
        if (keepSub) { xl += lowMid; xr += lowMid; }
        if (fs.mix < 1.0f) { xl = inL + (xl - inL) * fs.mix; xr = inR + (xr - inR) * fs.mix; }
        l[i] = xl; r[i] = xr;
      }
    }
  }

 private:
  struct State { Drive drive; Bitcrusher crush; Chorus chorus; Phaser phaser; Svf flt[2]; TrackComp comp; Pedal pedal; float sub[2][2] = {}; };
  State st_[kBoardSlots];
  int lastType_[kBoardSlots] = {};
  double sr_ = 48000.0;
  float subK_ = 0.0f;
};

}  // namespace ld
