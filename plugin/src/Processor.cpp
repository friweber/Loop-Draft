#include "Processor.h"

#include "BinaryData.h"
#include "Editor.h"

namespace {
const char* const kTrackNames[ld::kNumTracks] = {"Drums", "Bass", "Lead", "Syn 3", "Syn 4"};
}

juce::AudioProcessorValueTreeState::ParameterLayout LoopMasterProcessor::createLayout() {
  using Float = juce::AudioParameterFloat;
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  const auto unit = [](float hi) { return juce::NormalisableRange<float>(0.0f, hi, 0.0f); };
  layout.add(std::make_unique<Float>(juce::ParameterID{"master", 1}, "Master", unit(1.0f), 0.8f));
  for (int t = 0; t < ld::kNumTracks; ++t) {
    const juce::String n = kTrackNames[t], i(t);
    layout.add(std::make_unique<Float>(juce::ParameterID{"lvl" + i, 1}, n + " Level", unit(1.0f), 0.8f));
    layout.add(std::make_unique<Float>(juce::ParameterID{"drv" + i, 1}, n + " Drive", unit(1.0f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID{"dly" + i, 1}, n + " Delay Send", unit(0.8f), 0.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID{"rev" + i, 1}, n + " Reverb Send", unit(0.8f), 0.0f));
  }
  for (int s = 0; s < ld::kNumSynths; ++s) {
    const juce::String n = kTrackNames[s + 1], i(s);
    juce::NormalisableRange<float> cut(80.0f, 8000.0f, 0.0f);
    cut.setSkewForCentre(900.0f);
    layout.add(std::make_unique<Float>(juce::ParameterID{"cut" + i, 1}, n + " Cutoff", cut, 2000.0f));
    layout.add(std::make_unique<Float>(juce::ParameterID{"res" + i, 1}, n + " Resonance", juce::NormalisableRange<float>(0.5f, 15.0f, 0.0f), 1.0f));
  }
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"scene", 1}, "Scene", juce::StringArray{"A", "B", "C", "D", "E", "F", "G", "H"}, 0));
  layout.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sync", 1}, "Follow Host", true));
  layout.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"miditrack", 1}, "MIDI Plays",
             juce::StringArray{"By Channel", "Drums", "Bass", "Lead", "Syn 3", "Syn 4"}, 0));
  return layout;
}

LoopMasterProcessor::LoopMasterProcessor()
    : juce::AudioProcessor(BusesProperties()
          .withOutput("Main", juce::AudioChannelSet::stereo(), true)
          .withOutput("Drums", juce::AudioChannelSet::stereo(), false)
          .withOutput("Bass", juce::AudioChannelSet::stereo(), false)
          .withOutput("Lead", juce::AudioChannelSet::stereo(), false)
          .withOutput("Syn 3", juce::AudioChannelSet::stereo(), false)
          .withOutput("Syn 4", juce::AudioChannelSet::stereo(), false)),
      params(*this, nullptr, "PARAMS", createLayout()) {
  session_ = std::make_unique<ld::Session>(rate_);
  session_->loadStartPreset();
  scratch_.assign(size_t(ld::kNumTracks) * 2 * kScratchFrames, 0.0f);

  const auto add = [this](const juce::String& id, int pid, int index) {
    maps_.push_back({id, pid, index});
    raw_.push_back(params.getRawParameterValue(id));
    mapParam_.push_back(params.getParameter(id));
  };
  add("master", ld::P_MASTER, 0);
  for (int t = 0; t < ld::kNumTracks; ++t) {
    const juce::String i(t);
    add("lvl" + i, ld::P_FX_LEVEL, t);
    add("drv" + i, ld::P_FX_DRIVE, t);
    add("dly" + i, ld::P_FX_DELAY, t);
    add("rev" + i, ld::P_FX_REVERB, t);
  }
  for (int s = 0; s < ld::kNumSynths; ++s) {
    const juce::String i(s);
    add("cut" + i, ld::P_SYN_CUTOFF, s);
    add("res" + i, ld::P_SYN_RES, s);
  }
  lastParam_.assign(maps_.size(), 0.0f);
  lastSess_.assign(maps_.size(), 0.0f);
  sceneRaw_ = params.getRawParameterValue("scene");
  syncRaw_ = params.getRawParameterValue("sync");
  midiTrackRaw_ = params.getRawParameterValue("miditrack");
  session_->engine().setSongLoop(false);          // in a host the song ends where it ends
  for (auto& ch : noteTaken_) for (auto& n : ch) n.store(false);

  buildLibrary();
  adoptProjectValues();
  startTimerHz(30);
}

LoopMasterProcessor::~LoopMasterProcessor() { stopTimer(); }

void LoopMasterProcessor::buildLibrary() {
  for (int i = 0; i < LibraryData::namedResourceListSize; ++i) {
    int size = 0;
    const char* data = LibraryData::getNamedResource(LibraryData::namedResourceList[i], size);
    if (!data || size <= 0) continue;
    std::string err;
    ld::ContentInfo info;
    if (!ld::checkContent(std::string(data, size_t(size)), err, &info)) continue;
    LibraryEntry e;
    e.kind = juce::String(info.kind);
    e.name = juce::String::fromUTF8(info.name.c_str());
    e.style = juce::String::fromUTF8(info.style.c_str());
    e.data = data;
    e.size = size;
    library_.push_back(e);
  }
  std::sort(library_.begin(), library_.end(), [](const LibraryEntry& a, const LibraryEntry& b) {
    return a.name.compareIgnoreCase(b.name) < 0;
  });
}

juce::RangedAudioParameter* LoopMasterProcessor::hostParam(int paramId, int index) const {
  for (size_t i = 0; i < maps_.size(); ++i)
    if (maps_[i].pid == paramId && maps_[i].index == index) return mapParam_[i];
  return nullptr;
}

// After a project arrived (start, load, state from the host): the host parameters show what the project holds.
void LoopMasterProcessor::adoptProjectValues() {
  const juce::ScopedLock sl(sessionLock_);
  for (size_t i = 0; i < maps_.size(); ++i) {
    const float sv = session_->getParam(maps_[i].pid, maps_[i].index);
    mapParam_[i]->setValueNotifyingHost(mapParam_[i]->convertTo0to1(sv));
    lastParam_[i] = raw_[i]->load();
    lastSess_[i] = sv;
  }
}

void LoopMasterProcessor::timerCallback() {
  const juce::ScopedLock sl(sessionLock_);
  session_->poll();
  {  // keys and knobs of a controller that are tied to something live
    const auto scope = liveFifo_.read(liveFifo_.getNumReady());
    const auto handle = [this](const LiveEvent& ev) {
      const bool isNote = ev.kind == 2, down = isNote ? ev.val > 0 : ev.val >= 64;
      const int want = learn_.load();
      if (want >= 0 && want < kLiveTargets) {
        if (isNote && !down) return;
        liveMap_[want] = LiveMap{int(ev.kind), int(ev.ch), int(ev.num)};
        learn_.store(-1);
        rebuildNoteMap();
        return;
      }
      for (int t = 0; t < kLiveTargets; ++t)
        if (liveMap_[t].kind == int(ev.kind) && liveMap_[t].ch == int(ev.ch) && liveMap_[t].num == int(ev.num)) applyLive(t, int(ev.val), isNote, down);
    };
    for (int i = 0; i < scope.blockSize1; ++i) handle(liveEvents_[scope.startIndex1 + i]);
    for (int i = 0; i < scope.blockSize2; ++i) handle(liveEvents_[scope.startIndex2 + i]);
  }
  // A host parameter that moved (automation, a control in the editor) goes into the project.
  // A project value that moved (undo, a preset from the library) goes to the host parameter.
  for (size_t i = 0; i < maps_.size(); ++i) {
    const float pv = raw_[i]->load();
    const float sv = session_->getParam(maps_[i].pid, maps_[i].index);
    const float span = mapParam_[i]->getNormalisableRange().getRange().getLength();
    const float eps = span * 1e-4f;
    if (std::abs(pv - lastParam_[i]) > eps) {
      if (std::abs(sv - pv) > eps) session_->setParam(maps_[i].pid, maps_[i].index, pv);
      lastParam_[i] = pv;
      lastSess_[i] = session_->getParam(maps_[i].pid, maps_[i].index);
    } else if (std::abs(sv - lastSess_[i]) > eps) {
      mapParam_[i]->setValueNotifyingHost(mapParam_[i]->convertTo0to1(sv));
      lastParam_[i] = raw_[i]->load();
      lastSess_[i] = sv;
    }
  }
  if (!hostLeads_.load()) shownBpm_.store(double(session_->getParam(ld::P_BPM, 0)));
}

void LoopMasterProcessor::prepareToPlay(double sampleRate, int) {
  if (sampleRate <= 0.0 || std::abs(sampleRate - rate_) < 0.5) return;
  // The engine is built for one sample rate: a new session takes over the project.
  const juce::ScopedLock sl(sessionLock_);
  juce::MemoryBlock project, breakWav;
  const bool have = projectToMemory(project, breakWav);
  auto fresh = std::make_unique<ld::Session>(sampleRate);
  {
    const juce::SpinLock::ScopedLockType swap(swapLock_);
    session_.swap(fresh);
    rate_ = sampleRate;
  }
  if (have) projectFromMemory(project.getData(), project.getSize(), breakWav.getData(), breakWav.getSize());
  else session_->loadStartPreset();
  session_->engine().setSongMode(songMode_.load());
  session_->engine().setSongLoop(songLoop_.load());
  session_->engine().setDjFilter(djFilter_);
  lastScene_ = -1;
}

bool LoopMasterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
  for (int b = 1; b < layouts.outputBuses.size(); ++b) {
    const auto& set = layouts.outputBuses.getReference(b);
    if (!set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
  }
  return layouts.inputBuses.isEmpty();
}

void LoopMasterProcessor::handleMidi(const juce::MidiMessage& m) {
  ld::Engine& e = session_->engine();
  const int target = int(midiTrackRaw_->load() + 0.5f);       // 0 by channel, 1 drums, 2.. synths
  int track;                                                    // 0 drums, 1.. synths
  if (target == 0) {
    const int ch = m.getChannel();                              // 1..16
    track = ch == 10 ? 0 : (ch >= 1 && ch <= ld::kNumSynths ? ch : 1);
  } else {
    track = target - 1;
  }
  // Loop Master Live: controllers, and the notes that are tied to something, go to the message thread
  const int chan = juce::jlimit(1, 16, m.getChannel());
  const bool learning = learn_.load(std::memory_order_relaxed) >= 0;
  if (m.isController() || ((m.isNoteOn() || m.isNoteOff()) && (learning || noteTaken_[chan - 1][m.getNoteNumber()].load(std::memory_order_relaxed)))) {
    if (m.isController() && (m.getControllerNumber() >= 120)) { if (m.isAllNotesOff() || m.isAllSoundOff()) e.rtAllNotesOff(); return; }
    const auto scope = liveFifo_.write(1);
    if (scope.blockSize1 > 0)
      liveEvents_[scope.startIndex1] = LiveEvent{uint8_t(m.isController() ? 1 : 2), uint8_t(chan),
                                                 uint8_t(m.isController() ? m.getControllerNumber() : m.getNoteNumber()),
                                                 uint8_t(m.isController() ? m.getControllerValue() : (m.isNoteOn() ? m.getVelocity() : 0))};
    return;
  }
  if (m.isNoteOn()) {
    const int vel = m.getVelocity() >= 100 ? 2 : 1;
    if (track == 0) {
      const int pad = e.rtPadForMidiNote(m.getNoteNumber());
      if (pad >= 0) e.rtTriggerPad(pad, vel);
    } else {
      e.rtNoteOn(track - 1, m.getNoteNumber(), vel);
    }
  } else if (m.isNoteOff()) {
    if (track > 0) e.rtNoteOff(track - 1, m.getNoteNumber());
  } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
    e.rtAllNotesOff();
  }
}

void LoopMasterProcessor::renderSpan(juce::AudioBuffer<float>& buffer, int start, int count) {
  ld::Engine& e = session_->engine();
  auto mainBus = getBusBuffer(buffer, false, 0);
  while (count > 0) {
    const int n = juce::jmin(count, kScratchFrames);
    float* tl[ld::kNumTracks];
    float* tr[ld::kNumTracks];
    for (int t = 0; t < ld::kNumTracks; ++t) {
      auto bus = getBusBuffer(buffer, false, 1 + t);
      if (bus.getNumChannels() >= 2) {
        tl[t] = bus.getWritePointer(0) + start;
        tr[t] = bus.getWritePointer(1) + start;
      } else {                                                  // not connected: somewhere harmless
        tl[t] = scratch_.data() + size_t(2 * t) * kScratchFrames;
        tr[t] = scratch_.data() + size_t(2 * t + 1) * kScratchFrames;
      }
    }
    e.setTrackOutputs(tl, tr);
    e.render(mainBus.getWritePointer(0) + start, mainBus.getWritePointer(1) + start, n);
    start += n;
    count -= n;
  }
}

void LoopMasterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int frames = buffer.getNumSamples();
  const juce::SpinLock::ScopedTryLockType swap(swapLock_);
  if (!swap.isLocked() || getBusBuffer(buffer, false, 0).getNumChannels() < 2) {   // a session is being replaced
    buffer.clear();
    return;
  }
  ld::Engine& e = session_->engine();

  // host clock
  bool leads = false, plays = false;
  if (syncRaw_->load() > 0.5f) {
    if (auto* ph = getPlayHead()) {
      if (const auto pos = ph->getPosition()) {
        const auto ppq = pos->getPpqPosition();
        const auto bpm = pos->getBpm();
        if (ppq.hasValue() && bpm.hasValue()) {
          e.hostTransport(pos->getIsPlaying(), *ppq - (songMode_.load() ? double(songStartBar_.load() - 1) * 4.0 : 0.0), *bpm);
          shownBpm_.store(*bpm);
          leads = true;
          plays = pos->getIsPlaying();
        }
      }
    }
  }
  if (!leads) e.hostTransportOff();
  hostLeads_.store(leads);
  hostPlays_.store(plays);

  // host automation
  for (size_t i = 0; i < maps_.size(); ++i) e.rtSetOverride(maps_[i].pid, maps_[i].index, raw_[i]->load());
  const int scene = juce::jlimit(0, ld::kNumScenes - 1, int(sceneRaw_->load() + 0.5f));
  if (scene != lastScene_) { e.rtQueueScene(scene); lastScene_ = scene; }

  // notes land on their sample
  int done = 0;
  for (const auto meta : midi) {
    const int at = juce::jlimit(0, frames, meta.samplePosition);
    if (at > done) { renderSpan(buffer, done, at - done); done = at; }
    handleMidi(meta.getMessage());
  }
  if (frames > done) renderSpan(buffer, done, frames - done);
  midi.clear();
}

juce::AudioProcessorEditor* LoopMasterProcessor::createEditor() { return new LoopMasterEditor(*this); }

// ---- project in and out ----

bool LoopMasterProcessor::projectToMemory(juce::MemoryBlock& project, juce::MemoryBlock& breakWav) {
  const juce::File tmp = juce::File::createTempFile(".ldp");
  const juce::File brk(tmp.getFullPathName() + ".break.wav");
  bool ok = session_->saveCopy(tmp.getFullPathName().toRawUTF8());
  if (ok) ok = tmp.loadFileAsData(project);
  if (ok && brk.existsAsFile()) brk.loadFileAsData(breakWav);
  tmp.deleteFile();
  brk.deleteFile();
  return ok && project.getSize() > 0;
}

bool LoopMasterProcessor::projectFromMemory(const void* project, size_t projectSize, const void* breakWav, size_t breakSize) {
  if (!project || projectSize == 0) return false;
  const juce::File tmp = juce::File::createTempFile(".ldp");
  const juce::File brk(tmp.getFullPathName() + ".break.wav");
  bool ok = tmp.replaceWithData(project, projectSize);
  if (ok && breakWav && breakSize > 0) brk.replaceWithData(breakWav, breakSize);
  if (ok) ok = session_->load(tmp.getFullPathName().toRawUTF8());
  tmp.deleteFile();
  brk.deleteFile();
  return ok;
}

void LoopMasterProcessor::getStateInformation(juce::MemoryBlock& destData) {
  const juce::ScopedLock sl(sessionLock_);
  juce::ValueTree root("LoopMaster");
  root.setProperty("version", 1, nullptr);
  root.setProperty("songMode", songMode_.load(), nullptr);
  root.setProperty("songLoop", songLoop_.load(), nullptr);
  root.setProperty("midiFolder", midiFolder, nullptr);
  root.setProperty("songStartBar", songStartBar_.load(), nullptr);
  root.setProperty("liveMode", liveMode, nullptr);
  root.setProperty("guideSeen", guideSeen, nullptr);
  root.setProperty("fewerControls", fewerControls, nullptr);
  {
    juce::String map;
    for (const auto& lm : liveMap_) map << lm.kind << ":" << lm.ch << ":" << lm.num << ",";
    root.setProperty("liveMap", map, nullptr);
  }
  juce::MemoryBlock project, breakWav;
  if (projectToMemory(project, breakWav)) {
    root.setProperty("project", project, nullptr);
    if (breakWav.getSize() > 0) root.setProperty("break", breakWav, nullptr);
  }
  root.appendChild(params.copyState(), nullptr);
  juce::MemoryOutputStream out(destData, false);
  root.writeToStream(out);
}

void LoopMasterProcessor::setStateInformation(const void* data, int sizeInBytes) {
  if (!data || sizeInBytes <= 0) return;
  const juce::ValueTree root = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
  if (!root.isValid() || !root.hasType("LoopMaster")) return;
  const juce::ScopedLock sl(sessionLock_);
  if (const auto* project = root.getProperty("project").getBinaryData()) {
    const auto* brk = root.getProperty("break").getBinaryData();
    projectFromMemory(project->getData(), project->getSize(), brk ? brk->getData() : nullptr, brk ? brk->getSize() : 0);
  }
  setSongMode(bool(root.getProperty("songMode", false)));
  setSongLoop(bool(root.getProperty("songLoop", false)));
  midiFolder = root.getProperty("midiFolder", juce::String()).toString();
  setSongStartBar(int(root.getProperty("songStartBar", 1)));
  liveMode = bool(root.getProperty("liveMode", false));
  guideSeen = bool(root.getProperty("guideSeen", false));
  fewerControls = bool(root.getProperty("fewerControls", false));
  {
    juce::StringArray parts;
    parts.addTokens(root.getProperty("liveMap", juce::String()).toString(), ",", "");
    const juce::ScopedLock sl(sessionLock_);
    for (int t = 0; t < kLiveTargets; ++t) {
      liveMap_[t] = LiveMap{};
      if (t >= parts.size()) continue;
      juce::StringArray f;
      f.addTokens(parts[t], ":", "");
      if (f.size() == 3) liveMap_[t] = LiveMap{juce::jlimit(0, 2, f[0].getIntValue()), juce::jlimit(0, 16, f[1].getIntValue()), juce::jlimit(0, 127, f[2].getIntValue())};
    }
    rebuildNoteMap();
  }
  const juce::ValueTree saved = root.getChildWithName(params.state.getType());
  if (saved.isValid()) params.replaceState(saved);
  // The project is the truth for the controls it holds; the saved parameters agree with it anyway.
  adoptProjectValues();
  lastScene_ = -1;
}

bool LoopMasterProcessor::openProjectFile(const juce::File& f) {
  const juce::ScopedLock sl(sessionLock_);
  return session_->openProject(f.getFullPathName().toRawUTF8());
}

bool LoopMasterProcessor::saveProjectFile(const juce::File& f) {
  const juce::ScopedLock sl(sessionLock_);
  return session_->saveCopy(f.getFullPathName().toRawUTF8());
}

juce::String LoopMasterProcessor::importLibrary(const LibraryEntry& entry, int scene, int synth, bool tempoAndKey) {
  const juce::ScopedLock sl(sessionLock_);
  ld::ContentTarget target;
  target.scene = scene;
  target.synth = synth;
  target.tempoAndKey = tempoAndKey;
  std::string err;
  if (session_->importContent(std::string(entry.data, size_t(entry.size)), target, err)) return {};
  return juce::String(err.empty() ? "could not be loaded" : err);
}

// ---- Loop Master Live ----

namespace {
struct LiveFx { const char* name; ld::FxType type; int voice; float a, r, p3, p4; };
const LiveFx kLiveFx[LoopMasterProcessor::kLiveFxCount] = {
  {"STUTTER", ld::FxType::Slicer, 1, 1.0f, 0.6f, 1.0f, 1.0f},        // the last sixteenth, again and again
  {"GATE", ld::FxType::Slicer, 0, 1.0f, 0.6f, 0.45f, 0.45f},
  {"CRUSH", ld::FxType::Crusher, 2, 0.8f, 0.5f, 0.6f, 0.5f},
  {"ECHO", ld::FxType::DubDelay, 1, 0.8f, 0.6f, 0.75f, 0.35f},       // dotted eighths from side to side, ringing out
  {"WASH", ld::FxType::Reverb, 3, 0.9f, 0.8f, 0.5f, 0.3f},
  {"LO-FI", ld::FxType::LoFi, 2, 0.9f, 0.9f, 0.6f, 0.7f},
};
}  // namespace

const char* LoopMasterProcessor::liveFxName(int i) { return i >= 0 && i < kLiveFxCount ? kLiveFx[i].name : ""; }

void LoopMasterProcessor::liveFx(int i, bool down) {
  const juce::ScopedLock sl(sessionLock_);
  ld::Engine& e = session_->engine();
  if (down && i >= 0 && i < kLiveFxCount) {
    const LiveFx& f = kLiveFx[i];
    e.setPerformFx(int(f.type), f.voice, f.a, f.r, f.p3, f.p4);
    e.holdPerformFx(true);
    fxHeld_ = i;
  } else if (!down && (i == fxHeld_ || i < 0)) {
    e.holdPerformFx(false);
    fxHeld_ = -1;
  }
}

void LoopMasterProcessor::setLiveFilter(float v) {
  const juce::ScopedLock sl(sessionLock_);
  djFilter_ = std::abs(v) < 0.04f ? 0.0f : juce::jlimit(-1.0f, 1.0f, v);   // a little rest in the middle
  session_->engine().setDjFilter(djFilter_);
}

void LoopMasterProcessor::liveToggleMute(int track) {
  if (track < 0 || track >= ld::kNumTracks) return;
  const juce::ScopedLock sl(sessionLock_);
  solo_ = -1;
  session_->beginEdit();
  session_->setParam(ld::P_FX_MUTE, track, session_->getParam(ld::P_FX_MUTE, track) > 0.5f ? 0.0f : 1.0f);
}

void LoopMasterProcessor::liveSolo(int track) {
  const juce::ScopedLock sl(sessionLock_);
  if (track == solo_) track = -1;
  session_->beginEdit();
  if (solo_ < 0 && track >= 0) for (int t = 0; t < ld::kNumTracks; ++t) soloWas_[t] = session_->getParam(ld::P_FX_MUTE, t) > 0.5f;
  for (int t = 0; t < ld::kNumTracks; ++t) session_->setParam(ld::P_FX_MUTE, t, track < 0 ? (soloWas_[t] ? 1.0f : 0.0f) : (t == track ? 0.0f : 1.0f));
  solo_ = track;
}

void LoopMasterProcessor::livePlayStop() {
  if (hostPlays_.load()) return;
  const juce::ScopedLock sl(sessionLock_);
  ld::Engine& e = session_->engine();
  if (e.isPlaying()) e.stop(); else e.play(e.currentScene());
}

void LoopMasterProcessor::applyLive(int target, int value, bool isNote, bool down) {
  if (target >= kLiveGroove && target < kLiveGroove + 8) { if (down) setScene(target - kLiveGroove); }
  else if (target >= kLiveMute && target < kLiveMute + ld::kNumTracks) { if (down) liveToggleMute(target - kLiveMute); }
  else if (target >= kLiveLevel && target < kLiveLevel + ld::kNumTracks) {
    if (isNote) return;
    if (auto* hp = params.getParameter("lvl" + juce::String(target - kLiveLevel))) hp->setValueNotifyingHost(float(value) / 127.0f);
  }
  else if (target >= kLiveFx && target < kLiveFx + kLiveFxCount) liveFx(target - kLiveFx, down);
  else if (target == kLiveFilter) { if (!isNote) setLiveFilter(float(value) / 63.5f - 1.0f); }
  else if (target == kLivePlay) { if (down) livePlayStop(); }
}

void LoopMasterProcessor::rebuildNoteMap() {
  for (auto& ch : noteTaken_) for (auto& n : ch) n.store(false);
  for (const auto& lm : liveMap_) if (lm.kind == 2 && lm.ch >= 1 && lm.ch <= 16) noteTaken_[lm.ch - 1][lm.num].store(true);
}

void LoopMasterProcessor::liveForget(int target) {
  if (target < 0 || target >= kLiveTargets) return;
  const juce::ScopedLock sl(sessionLock_);
  liveMap_[target] = LiveMap{};
  rebuildNoteMap();
}

juce::String LoopMasterProcessor::liveMapName(int target) const {
  if (target < 0 || target >= kLiveTargets) return {};
  const LiveMap& lm = liveMap_[target];
  if (lm.kind == 1) return "CC " + juce::String(lm.num);
  if (lm.kind == 2) return juce::MidiMessage::getMidiNoteName(lm.num, true, true, 3);
  return {};
}

float LoopMasterProcessor::shownParam(int pid, int index) {
  ld::Session& s = *session_;
  const ld::Engine& e = s.engine();
  if (e.isPlaying() && !s.previewing()) {
    const int sc = e.currentScene(), st = e.currentStep();
    const int lane = st >= 0 ? s.findLane(sc, pid, index) : -1;
    if (lane >= 0) {
      const ld::SceneLane& ln = s.project().scenes[sc].lanes[lane];
      const float beat = float(st) * 0.25f;
      const int b0 = juce::jlimit(0, ld::kLanePoints - 2, int(beat));
      return ln.points[b0] + (beat - float(b0)) * (ln.points[b0 + 1] - ln.points[b0]);
    }
  }
  return s.getParam(pid, index);
}

void LoopMasterProcessor::setSongLoop(bool on) {
  const juce::ScopedLock sl(sessionLock_);
  songLoop_.store(on);
  session_->engine().setSongLoop(on);
}

void LoopMasterProcessor::setSongMode(bool on) {
  const juce::ScopedLock sl(sessionLock_);
  songMode_.store(on);
  session_->engine().setSongMode(on);
}

void LoopMasterProcessor::setScene(int scene) {
  if (auto* p = params.getParameter("scene")) {
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(float(juce::jlimit(0, ld::kNumScenes - 1, scene))));
    p->endChangeGesture();
  }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoopMasterProcessor(); }
