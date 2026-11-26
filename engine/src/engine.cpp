#include "loopdraft/engine.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace ld {

namespace {
constexpr int kBlock = 256;
inline int clampScene(int s) { return s < 0 ? 0 : (s >= kNumScenes ? kNumScenes - 1 : s); }
}  // namespace

Engine::Engine(double sampleRate) : sr_(sampleRate) {
  proj_ = new Project(makeDemoProject());
  for (int i = 0; i < kNumPads; ++i) builtin_[i] = makeBuiltinSample(i, sr_);
  sampler_.init(sr_);
  for (auto& s : synth_) s.init(sr_);
  delay_.init(sr_); reverb_.init(sr_); limiter_.init(sr_);
  master_.setTime(sr_, 15.0); master_.reset(proj_->master);
  for (int t = 0; t < kNumTracks; ++t) {
    tr_[t].level.setTime(sr_, 15.0); tr_[t].dSend.setTime(sr_, 15.0); tr_[t].rSend.setTime(sr_, 15.0);
    tr_[t].level.reset(proj_->fx[t].mute ? 0.0f : proj_->fx[t].level);
    tr_[t].dSend.reset(proj_->fx[t].delaySend); tr_[t].rSend.reset(proj_->fx[t].revSend);
    for (int sl = 0; sl < kFxSlots; ++sl) {
      tr_[t].fxSlots[sl].chorus.init(sr_);
      tr_[t].fxSlots[sl].phaser.init(sr_);
      tr_[t].fxSlots[sl].comp.init(sr_);
      tr_[t].fxSlots[sl].pedal.init(sr_);
    }
  }
  applyProject();
}

Engine::~Engine() {
  Cmd c;
  while (cmds_.pop(c)) {
    if (c.type == Cmd::SetProject) delete static_cast<Project*>(c.ptr);
    else if (c.type == Cmd::SetSample) delete static_cast<Sample*>(c.ptr);
  }
  collectGarbage();
  delete proj_;
  for (auto* s : user_) delete s;
}

// ------------------------------------------------------------------ control thread

void Engine::send(const Cmd& c) {
  if (cmds_.push(c)) return;
  if (c.type == Cmd::SetProject) delete static_cast<Project*>(c.ptr);       // queue full: drop, never block
  else if (c.type == Cmd::SetSample) delete static_cast<Sample*>(c.ptr);
}

void Engine::setProject(const Project& p) {
  collectGarbage();   // keeps the hand-back queue from ever filling up
  Cmd c{Cmd::SetProject}; c.ptr = new Project(p); send(c);
}
void Engine::setPadSample(int pad, std::unique_ptr<Sample> s) {
  if (pad < 0 || pad >= kNumPads) return;
  collectGarbage();
  Cmd c{Cmd::SetSample}; c.a = pad; c.ptr = s.release(); send(c);
}
void Engine::play(int scene)       { Cmd c{Cmd::Play}; c.a = clampScene(scene); send(c); }
void Engine::stop()                { send(Cmd{Cmd::Stop}); }
void Engine::queueScene(int scene) { Cmd c{Cmd::QueueScene}; c.a = clampScene(scene); send(c); }
void Engine::setSongMode(bool on)  { Cmd c{Cmd::SongMode}; c.a = on; send(c); }
void Engine::setRecording(bool on) { Cmd c{Cmd::Record}; c.a = on; send(c); }
void Engine::triggerPad(int pad, int vel) {
  if (pad < 0 || pad >= kNumPads) return;
  Cmd c{Cmd::TrigPad}; c.a = pad; c.b = vel; send(c);
}
void Engine::noteOn(int synth, int degree, int vel) {
  if (synth < 0 || synth >= kNumSynths) return;
  Cmd c{Cmd::NoteOn}; c.a = synth; c.b = degree; c.c = vel; send(c);
}
void Engine::noteOff(int synth, int degree) {
  if (synth < 0 || synth >= kNumSynths) return;
  Cmd c{Cmd::NoteOff}; c.a = synth; c.b = degree; send(c);
}
bool Engine::pollRecorded(RecordedHit& out) { return recorded_.pop(out); }
void Engine::collectGarbage() {
  Trash t;
  while (trash_.pop(t)) {
    if (t.isProject) delete static_cast<Project*>(t.ptr);
    else delete static_cast<Sample*>(t.ptr);
  }
}

// ------------------------------------------------------------------ audio thread

void Engine::applyProject() {
  Project& p = *proj_;
  if (p.chainLen < 1) { p.chainLen = 1; p.chain[0] = 0; }
  if (p.chainLen > kMaxChain) p.chainLen = kMaxChain;
  for (int i = 0; i < p.chainLen; ++i) p.chain[i] = clampScene(p.chain[i]);
  if (p.swing < 0) p.swing = 0;
  if (p.swing > 0.5f) p.swing = 0.5f;
  if (chainPos_ >= p.chainLen) chainPos_ = 0;
  if (step_ >= sceneSteps(p.scenes[scene_])) step_ %= sceneSteps(p.scenes[scene_]);
  for (int t = 0; t < kNumTracks; ++t) {
    const TrackFx& fx = p.fx[t];
    TrackState& ts = tr_[t];
    if (ts.cLow != fx.eqLow)   { ts.lo[0].set(false, sr_, 180.0, fx.eqLow);  ts.lo[1].set(false, sr_, 180.0, fx.eqLow);  ts.cLow = fx.eqLow; }
    if (ts.cHigh != fx.eqHigh) { ts.hi[0].set(true, sr_, 4200.0, fx.eqHigh); ts.hi[1].set(true, sr_, 4200.0, fx.eqHigh); ts.cHigh = fx.eqHigh; }
    if (ts.cDrive != fx.drive) { ts.drive.set(fx.drive); ts.cDrive = fx.drive; }
  }
  delay_.setTime(sr_, 0.75 * 60.0 / double(p.bpm < 20 ? 20 : p.bpm));   // dotted eighth
  if (frame_ == 0) {                             // nothing rendered yet: start exactly at the project's values
    master_.reset(p.master);
    for (int t = 0; t < kNumTracks; ++t) {
      tr_[t].level.reset(p.fx[t].mute ? 0.0f : p.fx[t].level);
      tr_[t].dSend.reset(p.fx[t].delaySend); tr_[t].rSend.reset(p.fx[t].revSend);
    }
  }
}

void Engine::processCommands() {
  Cmd c;
  while (cmds_.pop(c)) {
    switch (c.type) {
      case Cmd::SetProject:
        trash_.push(Trash{true, proj_});
        proj_ = static_cast<Project*>(c.ptr);
        applyProject();
        break;
      case Cmd::SetSample:
        sampler_.stopPad(c.a);                      // no voice may keep pointing at the old sample
        if (user_[c.a]) trash_.push(Trash{false, const_cast<Sample*>(user_[c.a])});
        user_[c.a] = static_cast<Sample*>(c.ptr);
        break;
      case Cmd::Play:
        if (hostSync_) break;                       // the host decides
        playing_ = true; step_ = 0; pending_ = -1;
        stepBase_ = double(frame_); nextFire_ = frame_;
        lastFire_ = std::numeric_limits<int64_t>::min() / 2;
        for (auto& s : sup_) s.on = false;
        for (auto& r : ratchets_) r.active = false;
        if (songMode_) { chainPos_ = 0; scene_ = proj_->chain[0]; } else scene_ = c.a;
        autoPos_ = chainPos_; autoScene_ = scene_; autoStart_ = frame_;
        break;
      case Cmd::Stop:
        if (hostSync_) break;
        stopNow();
        break;
      case Cmd::QueueScene:
        if (!playing_) scene_ = c.a;
        else if (!songMode_) pending_ = (c.a == scene_) ? -1 : c.a;
        break;
      case Cmd::SongMode: songMode_ = c.a != 0; break;
      case Cmd::Record:   recording_ = c.a != 0; break;
      case Cmd::TrigPad:
        sampler_.trigger(c.a, proj_->pads[c.a], padSample(c.a), c.b);
        recordHit(0, c.a, c.b);
        break;
      case Cmd::NoteOn: {
        int m[3];
        const int n = chordMidis(*proj_, proj_->synths[c.a], c.b, m);
        for (int i = 0; i < n; ++i) synth_[c.a].noteOn(m[i], c.c, -1, n, c.b);
        recordHit(1 + c.a, c.b, c.c);
        break;
      }
      case Cmd::NoteOff: synth_[c.a].noteOff(c.b); break;
    }
  }
  aPlaying_.store(playing_, std::memory_order_relaxed);
  aPending_.store(pending_, std::memory_order_relaxed);
  if (!playing_) aScene_.store(scene_, std::memory_order_relaxed);   // while playing, fireStep reports the audible scene
}

// A live hit is snapped to whichever step is closer: the one that just played or the one coming up.
void Engine::stopNow() {
  playing_ = false; pending_ = -1;
  for (auto& s : synth_) s.allNotesOff();
  for (auto& s : sup_) s.on = false;
  for (auto& r : ratchets_) r.active = false;
  aStep_.store(-1, std::memory_order_relaxed);
}

// ---- host clock, MIDI and automation (audio thread) ----

void Engine::hostTransport(bool playing, double quarterNotes, double bpm) {
  processCommands();                             // a new project is in place before positions are worked out
  hostSync_ = true;
  hostBpm_ = bpm >= 20.0 ? bpm : 0.0;
  if (tempo() != delayBpm_) { delayBpm_ = tempo(); delay_.setTime(sr_, 0.75 * 60.0 / delayBpm_); }
  if (!playing) {
    if (playing_) { stopNow(); aPlaying_.store(false, std::memory_order_relaxed); }
    hostNext_ = -1;
    return;
  }
  const double sl = stepLen();
  const double pos = quarterNotes * 4.0;         // in sixteenths
  const int64_t expectNext = int64_t(std::ceil(pos - 1e-6));
  // Still in step with the host (one behind is a swung step that is due): only the timing is renewed, so nothing
  // drifts when the tempo moves. Otherwise the host jumped, looped or just started.
  if (playing_ && hostNext_ >= 0 && (hostNext_ == expectNext || hostNext_ == expectNext - 1) && hostAgrees()) {
    stepBase_ = double(frame_) + (double(hostNext_) - pos) * sl;
    nextFire_ = std::llround(stepBase_ + double(swingOffset(proj_->swing, step_)) * sl);
    return;
  }
  seekHost(pos);
}

// Is the step that fires next the one the host's count stands for? Not after the scene got longer or shorter,
// the song chain changed, or another scene took over.
bool Engine::hostAgrees() const {
  const Project& p = *proj_;
  if (!songMode_) {
    const int steps = sceneSteps(p.scenes[scene_]);
    return int(((hostNext_ % steps) + steps) % steps) == step_;
  }
  int64_t total = 0;
  for (int c = 0; c < p.chainLen; ++c) total += sceneSteps(p.scenes[p.chain[c]]);
  if (total < 1) return false;
  int64_t rel = ((hostNext_ % total) + total) % total;
  for (int c = 0; c < p.chainLen; ++c) {
    const int st = sceneSteps(p.scenes[p.chain[c]]);
    if (rel < st) return c == chainPos_ && int(rel) == step_;
    rel -= st;
  }
  return false;
}

void Engine::seekHost(double pos) {
  const Project& p = *proj_;
  const double sl = stepLen();
  int64_t g = int64_t(std::floor(pos + 1e-6));
  double frac = pos - double(g);
  if (frac < 0.0) frac = 0.0;
  int scene = scene_ < 0 || scene_ >= kNumScenes ? 0 : scene_, chain = 0, steps = 0;
  int64_t rel = 0;
  if (songMode_) {
    int64_t total = 0;
    for (int c = 0; c < p.chainLen; ++c) total += sceneSteps(p.scenes[p.chain[c]]);
    if (total < 1) total = 1;
    rel = ((g % total) + total) % total;
    for (int c = 0; c < p.chainLen; ++c) {
      const int st = sceneSteps(p.scenes[p.chain[c]]);
      if (rel < st) { chain = c; break; }
      rel -= st;
    }
    scene = p.chain[chain];
    steps = sceneSteps(p.scenes[scene]);
  } else {
    steps = sceneSteps(p.scenes[scene]);
    rel = ((g % steps) + steps) % steps;
  }
  playing_ = true;
  for (auto& s : sup_) s.on = false;
  for (auto& r : ratchets_) r.active = false;
  scene_ = scene; chainPos_ = chain;
  autoPos_ = chain; autoScene_ = scene;
  autoStart_ = frame_ - std::llround((double(rel) + frac) * sl);
  lastFire_ = std::numeric_limits<int64_t>::min() / 2;
  if (frac < 1e-4) {                              // exactly on a step: it sounds now
    step_ = int(rel); hostNext_ = g; stepBase_ = double(frame_);
  } else {                                        // inside a step: the next one is the first to sound
    step_ = int(rel) + 1; hostNext_ = g + 1; stepBase_ = double(frame_) + (1.0 - frac) * sl;
    if (step_ >= steps) {
      step_ = 0;
      if (songMode_) { chainPos_ = (chainPos_ + 1) % p.chainLen; scene_ = p.chain[chainPos_]; }
    }
    aStep_.store(int(rel), std::memory_order_relaxed);
    aScene_.store(scene, std::memory_order_relaxed);
    aChainPos_.store(chain, std::memory_order_relaxed);
  }
  nextFire_ = std::llround(stepBase_ + double(swingOffset(p.swing, step_)) * sl);
  aPlaying_.store(true, std::memory_order_relaxed);
}

void Engine::hostTransportOff() {
  if (!hostSync_) return;
  hostSync_ = false; hostBpm_ = 0.0; hostNext_ = -1;
  if (tempo() != delayBpm_) { delayBpm_ = tempo(); delay_.setTime(sr_, 0.75 * 60.0 / delayBpm_); }
}

void Engine::rtTriggerPad(int pad, int vel) {
  if (pad < 0 || pad >= kNumPads) return;
  sampler_.trigger(pad, proj_->pads[pad], padSample(pad), vel >= 2 ? 2 : 1);
}
void Engine::rtNoteOn(int synth, int midiNote, int vel) {
  if (synth < 0 || synth >= kNumSynths || midiNote < 0 || midiNote > 127) return;
  synth_[synth].noteOn(midiNote, vel >= 2 ? 2 : 1, -1, 1, 1000 + midiNote);
}
void Engine::rtNoteOff(int synth, int midiNote) {
  if (synth < 0 || synth >= kNumSynths) return;
  synth_[synth].noteOff(1000 + midiNote);
}
void Engine::rtAllNotesOff() { for (auto& s : synth_) s.allNotesOff(); }
void Engine::rtQueueScene(int scene) {
  scene = clampScene(scene);
  if (!playing_) scene_ = scene;
  else if (!songMode_) pending_ = (scene == scene_) ? -1 : scene;
}
int Engine::rtPadForMidiNote(int midiNote) const {
  for (int i = 0; i < kNumPads; ++i) if (proj_->pads[i].gmNote == midiNote) return i;
  return midiNote >= 60 && midiNote < 60 + kNumPads ? midiNote - 60 : -1;
}
void Engine::rtSetOverride(int paramId, int index, float value) {
  int freeSlot = -1;
  for (int i = 0; i < kMaxOverrides; ++i) {
    if (ov_[i].id == paramId && ov_[i].index == index) { ov_[i].value = value; return; }
    if (ov_[i].id < 0 && freeSlot < 0) freeSlot = i;
  }
  if (freeSlot >= 0) { ov_[freeSlot].id = paramId; ov_[freeSlot].index = index; ov_[freeSlot].value = value; }
}
void Engine::rtClearOverrides() { for (auto& o : ov_) o.id = -1; }
void Engine::setTrackOutputs(float* const* l, float* const* r) {
  for (int t = 0; t < kNumTracks; ++t) { trkL_[t] = l ? l[t] : nullptr; trkR_[t] = (l && r) ? r[t] : nullptr; }
}

void Engine::recordHit(int track, int row, int vel) {
  if (!playing_ || !recording_) return;
  RecordedHit h; h.track = track; h.row = row; h.vel = vel;
  if (frame_ - lastFire_ <= nextFire_ - frame_) { h.scene = lastScene_; h.step = lastStep_; }
  else {
    const bool switching = !songMode_ && pending_ >= 0 && step_ % kStepsPerBar == 0;
    h.scene = switching ? pending_ : scene_; h.step = switching ? 0 : step_;
    for (auto& s : sup_)                        // it was just heard live, so skip it once when the step arrives
      if (!s.on) { s.on = true; s.scene = h.scene; s.track = track; s.row = row; s.step = h.step; break; }
  }
  recorded_.push(h);
}

bool Engine::suppressed(int track, int row) {
  for (auto& s : sup_)
    if (s.on && s.scene == scene_ && s.step == step_ && s.track == track && s.row == row) { s.on = false; return true; }
  return false;
}

void Engine::fireStep() {
  const Project& p = *proj_;
  if (!songMode_ && pending_ >= 0 && step_ % kStepsPerBar == 0) {   // bar line: a queued scene starts here
    scene_ = pending_; pending_ = -1; step_ = 0;
    aPending_.store(-1, std::memory_order_relaxed);
  }
  const Scene& sc = p.scenes[scene_];
  const int steps = sceneSteps(sc);
  if (step_ >= steps) step_ = 0;
  const double sl = stepLen();
  if (step_ == 0 && songMode_) { autoPos_ = chainPos_; autoScene_ = scene_; autoStart_ = nextFire_; }   // a song slot starts sounding

  for (int pad = 0; pad < kNumPads; ++pad) {
    const int v = sc.drums.steps[pad][step_];
    if (v && !suppressed(0, pad)) {
      sampler_.trigger(pad, p.pads[pad], padSample(pad), v >= 2 ? 2 : 1);
      const int hits = rollHits(v);
      if (hits > 1) {                           // roll: the remaining hits are spread evenly inside this step
        Ratchet& rt = ratchets_[pad];
        rt.active = true; rt.pad = pad; rt.hits = hits; rt.done = 1;
        rt.base = double(nextFire_); rt.interval = sl / double(hits);
        rt.nextFrame = std::llround(rt.base + rt.interval);
      }
    }
  }
  for (int s = 0; s < kNumSynths; ++s) {
    const SynthPattern& pat = sc.synth[s];
    const int count = pat.count > kMaxNotes ? kMaxNotes : pat.count;
    for (int k = 0; k < count; ++k) {
      const Note& nt = pat.notes[k];
      if (nt.step != step_ || suppressed(1 + s, nt.degree)) continue;
      int m[3];
      const int n = chordMidis(p, p.synths[s], nt.degree, m);
      int64_t gate = std::llround(double(nt.len) * sl * 0.92);
      // A note that a slide note follows directly is held right into it, so there is no gap to glide across.
      for (int j = 0; j < count; ++j)
        if (int(pat.notes[j].step) == int(nt.step) + int(nt.len) && (pat.notes[j].flags & kNoteSlide)) gate = std::llround((double(nt.len) + 0.25) * sl);
      if (gate < 1) gate = 1;
      for (int i = 0; i < n; ++i) synth_[s].noteOn(m[i], nt.vel, gate, n, nt.degree, (nt.flags & kNoteSlide) != 0);
    }
  }

  lastFire_ = nextFire_; lastStep_ = step_; lastScene_ = scene_;
  if (hostNext_ >= 0) ++hostNext_;
  aStep_.store(step_, std::memory_order_relaxed);
  aScene_.store(scene_, std::memory_order_relaxed);
  aChainPos_.store(chainPos_, std::memory_order_relaxed);

  stepBase_ += sl;
  ++step_;
  if (step_ >= steps) {                         // end of the scene: loop, or move along the song chain
    step_ = 0;
    if (songMode_) { chainPos_ = (chainPos_ + 1) % p.chainLen; scene_ = p.chain[chainPos_]; }
  }
  nextFire_ = std::llround(stepBase_ + double(swingOffset(p.swing, step_)) * sl);
}

void Engine::renderAudio(float* l, float* r, int n) {
  const Project& p = *proj_;
  SynthParams effSynth[kNumSynths];
  TrackFx effFx[kNumTracks];
  for (int s = 0; s < kNumSynths; ++s) effSynth[s] = p.synths[s];
  for (int t = 0; t < kNumTracks; ++t) effFx[t] = p.fx[t];

  // Song automation: each lane sweeps one parameter from start to end across the song slot that is sounding now.
  // Up to kAutoPerTrack lanes per track. render() cuts blocks on a fixed 64-frame grid while this is active,
  // so the result does not depend on the audio block size.
  if (playing_ && songMode_ && autoPos_ >= 0 && autoPos_ < p.chainLen) {
    const double totalSlotFrames = double(sceneSteps(p.scenes[autoScene_ < 0 || autoScene_ >= kNumScenes ? 0 : autoScene_])) * stepLen();
    const int64_t gridFrame = ((frame_ - autoStart_) / 64) * 64;     // same value for the whole 64-frame cell
    double u = totalSlotFrames > 1.0 ? double(gridFrame) / totalSlotFrames : 0.0;
    if (u < 0.0) u = 0.0; else if (u > 1.0) u = 1.0;
    const ChainAuto& ca = p.chainAuto[autoPos_];
    const auto clampF = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
    for (int t = 0; t < kNumTracks; ++t) {
      for (int ln = 0; ln < kAutoPerTrack; ++ln) {
        const AutoLane& al = ca.tracks[t][ln];
        if (al.paramId < 0) continue;
        const float lin = al.startVal + float(u) * (al.endVal - al.startVal);
        // Track FX parameters (work on Drums AND any Synth track)
        switch (al.paramId) {
          case P_FX_LEVEL: effFx[t].level     = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_DRIVE: effFx[t].drive     = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_EQ_LOW: effFx[t].eqLow     = clampF(lin, -12.0f, 12.0f); break;
          case P_FX_EQ_HIGH: effFx[t].eqHigh    = clampF(lin, -12.0f, 12.0f); break;
          case P_FX_DELAY: effFx[t].delaySend = clampF(lin, 0.0f, 0.8f); break;
          case P_FX_REVERB: effFx[t].revSend   = clampF(lin, 0.0f, 0.8f); break;
          case P_FX_SLOT1_AMOUNT: effFx[t].slots[0].amount = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_SLOT1_RATE: effFx[t].slots[0].rate = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_SLOT2_AMOUNT: effFx[t].slots[1].amount = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_SLOT2_RATE: effFx[t].slots[1].rate = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_SLOT3_AMOUNT: effFx[t].slots[2].amount = clampF(lin, 0.0f, 1.0f); break;
          case P_FX_SLOT3_RATE: effFx[t].slots[2].rate = clampF(lin, 0.0f, 1.0f); break;
          case P_KIT_HARDNESS: if (t == 0) effFx[0].drive = clampF(lin, 0.0f, 1.0f); break;   // hardness is the drum track's drive
          default: break;
        }
        // Synth parameters (when t >= 1)
        if (t >= 1 && t - 1 < kNumSynths) {
          SynthParams& sp = effSynth[t - 1];
          switch (al.paramId) {
            case P_SYN_DETUNE: sp.detune     = clampF(lin, -50.0f, 50.0f); break;
            case P_SYN_CUTOFF: { // Cutoff: exponential sweep
              const float s0 = clampF(al.startVal, 60.0f, 12000.0f);
              const float s1 = clampF(al.endVal, 60.0f, 12000.0f);
              sp.cutoff = s0 * std::pow(s1 / s0, float(u));
              break;
            }
            case P_SYN_RES: sp.res        = clampF(lin, 0.5f, 15.0f); break;
            case P_SYN_DECAY: sp.decay      = clampF(lin, 0.02f, 3.0f); break;
            case P_SYN_ATTACK: sp.attack    = clampF(lin, 0.001f, 1.0f); break;
            case P_SYN_SUSTAIN: sp.sustain  = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_RELEASE: sp.release  = clampF(lin, 0.01f, 3.0f); break;
            case P_SYN_LEVEL: sp.level      = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_LFO_RATE: sp.lfoRate    = clampF(lin, 0.05f, 20.0f); break;
            case P_SYN_LFO_DEPTH: sp.lfoDepth   = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_FM_RATIO: sp.fmRatio    = clampF(lin, 0.5f, 8.0f); break;
            case P_SYN_FM_INDEX: sp.fmIndex    = clampF(lin, 0.0f, 8.0f); break;
            case P_SYN_FM_DECAY: sp.fmModDecay = clampF(lin, 0.01f, 3.0f); break;
            case P_SYN_FM_FB: sp.fmFeedback = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_WARMTH: sp.feelWarmth = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_MOTION: sp.feelMotion = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_FOLD: sp.fold = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_VEC_X: sp.vecX = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_VEC_Y: sp.vecY = clampF(lin, 0.0f, 1.0f); break;
            case P_SYN_PLUCK_BRIGHT: sp.pluckBright = clampF(lin, 0.0f, 1.0f); break;
            default: break;
          }
        }
      }
    }
  }
  // Host automation: these values replace the project's own.
  float masterVal = p.master;
  for (int i = 0; i < kMaxOverrides; ++i) {
    const Override& o = ov_[i];
    if (o.id < 0) continue;
    const float v = o.value;
    const auto unit = [](float x, float hi) { return x < 0.0f ? 0.0f : (x > hi ? hi : x); };
    if (o.id == P_MASTER) { masterVal = unit(v, 1.0f); continue; }
    if (o.index < 0) continue;
    if (o.index < kNumTracks) {
      switch (o.id) {
        case P_FX_LEVEL:  effFx[o.index].level = unit(v, 1.0f); break;
        case P_FX_DRIVE:  effFx[o.index].drive = unit(v, 1.0f); break;
        case P_FX_DELAY:  effFx[o.index].delaySend = unit(v, 0.8f); break;
        case P_FX_REVERB: effFx[o.index].revSend = unit(v, 0.8f); break;
        default: break;
      }
    }
    if (o.index < kNumSynths) {
      if (o.id == P_SYN_CUTOFF) effSynth[o.index].cutoff = v < 60.0f ? 60.0f : (v > 12000.0f ? 12000.0f : v);
      else if (o.id == P_SYN_RES) effSynth[o.index].res = v < 0.5f ? 0.5f : (v > 15.0f ? 15.0f : v);
    }
  }
  for (int t = 0; t < kNumTracks; ++t) {
    if (tr_[t].cDrive != effFx[t].drive) { tr_[t].drive.set(effFx[t].drive); tr_[t].cDrive = effFx[t].drive; }
  }
  const bool trackOuts = trkL_[0] != nullptr;
  if (trackOuts) {
    for (int t = 0; t < kNumTracks; ++t) {
      std::memset(trkL_[t] + trkOff_, 0, sizeof(float) * size_t(n));
      if (trkR_[t]) std::memset(trkR_[t] + trkOff_, 0, sizeof(float) * size_t(n));
    }
  }

  float tb[kNumTracks][2][kBlock];
  float mono[kBlock];
  std::memset(tb, 0, sizeof(tb));
  sampler_.process(tb[0][0], tb[0][1], n);
  for (int s = 0; s < kNumSynths; ++s) {
    std::memset(mono, 0, sizeof(float) * size_t(n));
    synth_[s].process(mono, n, effSynth[s]);
    std::memcpy(tb[1 + s][0], mono, sizeof(float) * size_t(n));
    std::memcpy(tb[1 + s][1], mono, sizeof(float) * size_t(n));
  }
  // A track that has had nothing to play for a while costs nothing: its channel strip and effects are skipped.
  // (None of them rings longer than this; delay and reverb are shared and keep running.)
  bool live[kNumTracks];
  for (int t = 0; t < kNumTracks; ++t) {
    // an echo or reverb pedal on the track keeps sounding long after the last note
    bool rings = false;
    for (int sl = 0; sl < kFxSlots; ++sl) rings = rings || (!effFx[t].slots[sl].bypass && Pedal::rings(effFx[t].slots[sl].type));
    const int64_t idleAfter = int64_t(sr_ * (rings ? 12.0 : 0.75));
    bool sound = false;
    for (int i = 0; i < n && !sound; ++i) sound = tb[t][0][i] != 0.0f || tb[t][1][i] != 0.0f;
    TrackState& ts = tr_[t];
    if (sound) ts.idle = 0; else if (ts.idle < idleAfter) ts.idle += n;
    live[t] = ts.idle < idleAfter;
    if (!live[t]) {                                // a silent track takes new fader positions at once: no glide when it wakes up
      ts.level.reset(effFx[t].mute ? 0.0f : effFx[t].level);
      ts.dSend.reset(effFx[t].delaySend); ts.rSend.reset(effFx[t].revSend);
    }
  }
  for (int i = 0; i < n; ++i) {
    float mL = 0, mR = 0, dL = 0, dR = 0, rL = 0, rR = 0;
    for (int t = 0; t < kNumTracks; ++t) {
      if (!live[t]) continue;
      const TrackFx& fx = effFx[t];
      TrackState& ts = tr_[t];
      const float lvl = ts.level.next(fx.mute ? 0.0f : fx.level);
      const float ds = ts.dSend.next(fx.delaySend), rs = ts.rSend.next(fx.revSend);
      float xl = ts.hi[0].tick(ts.lo[0].tick(ts.drive.tick(tb[t][0][i] * lvl)));
      float xr = ts.hi[1].tick(ts.lo[1].tick(ts.drive.tick(tb[t][1][i] * lvl)));
      for (int sl = 0; sl < kFxSlots; ++sl) {
        const FxSlot& fs = fx.slots[sl];
        if (fs.bypass || fs.type == FxType::Off) continue;
        FxSlotState& st = ts.fxSlots[sl];
        switch (fs.type) {
          case FxType::Overdrive:
            if (i == 0) st.drive.set(fs.amount);
            xl = st.drive.tick(xl); xr = st.drive.tick(xr);
            break;
          case FxType::Bitcrush:
            st.crush.tick(xl, xr, fs.amount, fs.rate);
            break;
          case FxType::Chorus:
            st.chorus.tick(xl, xr, fs.amount, fs.rate);
            break;
          case FxType::Phaser:
            st.phaser.tick(xl, xr, fs.amount, fs.rate);
            break;
          case FxType::Filter: {
            if (i == 0) {
              const float cf = 100.0f * std::pow(100.0f, fs.amount);
              const float q = 0.7f + fs.rate * 8.0f;
              st.flt[0].set(sr_, cf, q); st.flt[1].set(sr_, cf, q);
            }
            xl = st.flt[0].low(xl); xr = st.flt[1].low(xr);
            break;
          }
          case FxType::Compressor:
            st.comp.tick(xl, xr, fs.amount);
            break;
          default:                                   // the stompbox collection
            st.pedal.tick(fs.type, xl, xr, fs.amount, fs.rate);
            break;
        }
      }
      if (trackOuts) { trkL_[t][trkOff_ + i] = xl; if (trkR_[t]) trkR_[t][trkOff_ + i] = xr; }
      mL += xl; mR += xr; dL += xl * ds; dR += xr * ds; rL += xl * rs; rR += xr * rs;
    }
    float oL, oR;
    delay_.tick(dL, dR, p.delayFeedback, oL, oR); mL += oL; mR += oR;
    reverb_.tick(rL, rR, oL, oR); mL += oL; mR += oR;
    const float g = master_.next(masterVal);
    mL *= g; mR *= g;
    limiter_.tick(mL, mR);
    l[i] = mL; r[i] = mR;
    peakL_ = std::fabs(mL) > peakL_ * 0.9995f ? std::fabs(mL) : peakL_ * 0.9995f;
    peakR_ = std::fabs(mR) > peakR_ * 0.9995f ? std::fabs(mR) : peakR_ * 0.9995f;
  }
}

void Engine::render(float* outL, float* outR, int frames) {
  processCommands();
  int done = 0;
  while (done < frames) {
    int n = frames - done;
    if (playing_) {
      while (nextFire_ <= frame_) fireStep();           // every step starts on its exact sample
      for (int pad = 0; pad < kNumPads; ++pad) {
        Ratchet& rt = ratchets_[pad];
        while (rt.active && rt.nextFrame <= frame_) {
          sampler_.trigger(rt.pad, proj_->pads[rt.pad], padSample(rt.pad), 2);
          if (++rt.done >= rt.hits) rt.active = false;
          else rt.nextFrame = std::llround(rt.base + double(rt.done) * rt.interval);   // from the step start: no rounding build-up
        }
      }
      int64_t until = nextFire_ - frame_;
      for (int pad = 0; pad < kNumPads; ++pad) {
        if (ratchets_[pad].active) {
          const int64_t ru = ratchets_[pad].nextFrame - frame_;
          if (ru < until) until = ru;
        }
      }
      if (until < n) n = int(until);
      if (songMode_) {                          // automation grid, see renderAudio
        const int g = 64 - int(((frame_ - autoStart_) % 64 + 64) % 64);
        if (g < n) n = g;
      }
    }
    if (n > kBlock) n = kBlock;
    trkOff_ = done;
    renderAudio(outL + done, outR + done, n);
    frame_ += n; done += n;
  }
  for (int t = 0; t < kNumTracks; ++t) { trkL_[t] = nullptr; trkR_[t] = nullptr; }   // the pointers were for this call
  aPeakL_.store(peakL_, std::memory_order_relaxed);
  aPeakR_.store(peakR_, std::memory_order_relaxed);
}

void Engine::renderInterleaved(float* out, int frames) {
  float l[kBlock], r[kBlock];
  int done = 0;
  while (done < frames) {
    const int n = frames - done > kBlock ? kBlock : frames - done;
    render(l, r, n);
    for (int i = 0; i < n; ++i) { out[2 * (done + i)] = l[i]; out[2 * (done + i) + 1] = r[i]; }
    done += n;
  }
}

}  // namespace ld
