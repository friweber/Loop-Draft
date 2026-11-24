#import "LoopDraftBridge.h"
#import <AVFoundation/AVFoundation.h>

#include <atomic>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <cstring>
#include <vector>
#include "loopdraft/content.h"
#include "loopdraft/engine.h"
#include "loopdraft/session.h"
#include "loopdraft/voices.h"

namespace {
inline void flushDenormalsToZero() {
#if defined(__aarch64__)
  uint64_t fpcr;
  __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
  fpcr |= (1ull << 24);
  __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#endif
}
}

// How much of the time between two callbacks the engine needs: a running mean, and the highest since the last look.
static std::atomic<float> g_loadAvg{0.0f}, g_loadPeak{0.0f};

@implementation LoopDraftBridge {
  AVAudioEngine *_audioEngine;
  AVAudioSourceNode *_sourceNode;
  std::unique_ptr<ld::Session> _session;
  std::string _savePath;
  double _sampleRate;
  BOOL _running;
  int _audioMode;
}

- (instancetype)init {
  if ((self = [super init])) { _audioMode = 1; }
  return self;
}

- (double)bufferSeconds { return _audioMode == 0 ? 0.003 : (_audioMode == 2 ? 0.012 : 0.006); }

- (void)setAudioMode:(int)mode {
  _audioMode = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
  if (_running) {
    NSError *err = nil;
    [[AVAudioSession sharedInstance] setPreferredIOBufferDuration:[self bufferSeconds] error:&err];
  }
}

+ (instancetype)shared {
  static LoopDraftBridge *inst = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{ inst = [[LoopDraftBridge alloc] init]; });
  return inst;
}

- (BOOL)startWithFilesDir:(NSString *)dir {
  if (_running && _audioEngine.isRunning) return YES;
  _savePath = std::string(dir.UTF8String) + "/autosave.ldp";

  AVAudioSession *as = [AVAudioSession sharedInstance];
  NSError *err = nil;
  [as setCategory:AVAudioSessionCategoryPlayback
             mode:AVAudioSessionModeDefault
          options:AVAudioSessionCategoryOptionMixWithOthers
            error:&err];
  [as setPreferredIOBufferDuration:[self bufferSeconds] error:&err];
  [as setPreferredSampleRate:48000.0 error:&err];
  [as setActive:YES error:&err];

  double sr = as.sampleRate > 8000.0 ? as.sampleRate : 48000.0;
  _sampleRate = sr;

  if (!_session) {
    _session = std::make_unique<ld::Session>(sr);
    if (!_session->load(_savePath.c_str())) _session->loadStartPreset();   // fresh install: the Warehouse 303 preset
  }

  _audioEngine = [[AVAudioEngine alloc] init];
  AVAudioFormat *fmt = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:sr channels:2];

  ld::Session *sesPtr = _session.get();
  __block bool denormalsOff = false;

  _sourceNode = [[AVAudioSourceNode alloc] initWithFormat:fmt renderBlock:^OSStatus(
      BOOL *isSilence,
      const AudioTimeStamp *timestamp,
      AVAudioFrameCount frameCount,
      AudioBufferList *outputData) {
    if (!denormalsOff) { flushDenormalsToZero(); denormalsOff = true; }
    if (!sesPtr || outputData->mNumberBuffers < 1) return noErr;
    timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);

    if (outputData->mNumberBuffers >= 2) {
      float *outL = static_cast<float *>(outputData->mBuffers[0].mData);
      float *outR = static_cast<float *>(outputData->mBuffers[1].mData);
      sesPtr->engine().render(outL, outR, int(frameCount));
    } else {
      float *out = static_cast<float *>(outputData->mBuffers[0].mData);
      sesPtr->engine().renderInterleaved(out, int(frameCount));
    }
    timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
    const double took = double(t1.tv_sec - t0.tv_sec) + double(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    const float load = float(took * sr / double(frameCount > 0 ? frameCount : 1));
    const float avg = g_loadAvg.load(std::memory_order_relaxed);
    g_loadAvg.store(avg + 0.02f * (load - avg), std::memory_order_relaxed);
    if (load > g_loadPeak.load(std::memory_order_relaxed)) g_loadPeak.store(load, std::memory_order_relaxed);
    *isSilence = NO;
    return noErr;
  }];

  [_audioEngine attachNode:_sourceNode];
  [_audioEngine connect:_sourceNode to:_audioEngine.mainMixerNode format:fmt];
  [_audioEngine prepare];
  BOOL ok = [_audioEngine startAndReturnError:&err];
  _running = ok;
  return ok;
}

- (void)stop {
  _running = NO;
  if (_session) {
    _session->engine().stop();
    _session->save(_savePath.c_str());
  }
  if (_audioEngine) {
    [_audioEngine stop];
    _audioEngine = nil;
    _sourceNode = nil;
  }
}

- (NSString *)audioInfo {
  if (!_running) return @"Audio Off";
  AVAudioSession *as = [AVAudioSession sharedInstance];
  int sr = (int)as.sampleRate;
  int ms = (int)(as.IOBufferDuration * 1000.0 + 0.5);
  const int peak = int(g_loadPeak.exchange(0.0f, std::memory_order_relaxed) * 100.0f + 0.5f);
  return [NSString stringWithFormat:@"CoreAudio %d Hz, %d ms, load %d%% peak %d%%", sr, ms,
          int(g_loadAvg.load(std::memory_order_relaxed) * 100.0f + 0.5f), peak];
}

- (void)saveIfDirty { if (_session && _session->dirty()) _session->save(_savePath.c_str()); }
- (BOOL)poll { return _session && _session->poll(); }

- (void)playScene:(int)scene { if (_session) _session->engine().play(scene); }
- (void)stopTransport { if (_session) _session->engine().stop(); }
- (void)queueScene:(int)scene { if (_session) _session->engine().queueScene(scene); }
- (void)setSongMode:(BOOL)on { if (_session) _session->engine().setSongMode(on); }
- (void)setRecording:(BOOL)on { if (_session) _session->engine().setRecording(on); }
- (void)triggerPad:(int)pad vel:(int)vel { if (_session) _session->engine().triggerPad(pad, vel); }
- (void)noteOnSynth:(int)synth degree:(int)degree vel:(int)vel { if (_session) _session->engine().noteOn(synth, degree, vel); }
- (void)noteOffSynth:(int)synth degree:(int)degree { if (_session) _session->engine().noteOff(synth, degree); }

- (BOOL)isPlaying { return _session && _session->engine().isPlaying(); }
- (int)currentStep { return _session ? _session->engine().currentStep() : -1; }
- (int)currentScene { return _session ? _session->engine().currentScene() : 0; }
- (int)queuedScene { return _session ? _session->engine().queuedScene() : -1; }
- (int)chainPos { return _session ? _session->engine().chainPos() : 0; }
- (float)peakForChannel:(int)ch { return _session ? _session->engine().peak(ch) : 0.0f; }

- (void)beginEdit { if (_session) _session->beginEdit(); }
- (BOOL)undo { return _session && _session->undo(); }
- (BOOL)redo { return _session && _session->redo(); }
- (BOOL)canUndo { return _session && _session->canUndo(); }
- (BOOL)canRedo { return _session && _session->canRedo(); }

- (float)getParam:(int)paramId index:(int)index { return _session ? _session->getParam(paramId, index) : 0.0f; }
- (void)setParam:(int)paramId index:(int)index value:(float)value { if (_session) _session->setParam(paramId, index, value); }

- (int)toggleStepScene:(int)scene pad:(int)pad step:(int)step accent:(BOOL)accent {
  return _session ? _session->toggleDrumStep(scene, pad, step, accent) : 0;
}
- (void)setDrumStepScene:(int)scene pad:(int)pad step:(int)step value:(int)value {
  if (_session) _session->setDrumStep(scene, pad, step, value);
}
- (NSData *)drumGridForScene:(int)scene {
  const size_t n = ld::kNumPads * ld::kMaxSteps;
  if (!_session || scene < 0 || scene >= ld::kNumScenes) return [NSMutableData dataWithLength:n];
  return [NSData dataWithBytes:_session->project().scenes[scene].drums.steps length:n];
}
- (BOOL)toggleNoteScene:(int)scene synth:(int)synth step:(int)step degree:(int)degree accent:(BOOL)accent {
  return _session && _session->toggleNote(scene, synth, step, degree, accent);
}
- (NSArray<NSNumber *> *)notesForScene:(int)scene synth:(int)synth {
  NSMutableArray<NSNumber *> *out = [NSMutableArray array];
  if (!_session || scene < 0 || scene >= ld::kNumScenes || synth < 0 || synth >= ld::kNumSynths) return out;
  const ld::SynthPattern &p = _session->project().scenes[scene].synth[synth];
  for (int i = 0; i < p.count; ++i) {
    [out addObject:@(p.notes[i].step)];
    [out addObject:@(p.notes[i].degree)];
    [out addObject:@(p.notes[i].len)];
    [out addObject:@(p.notes[i].vel)];
  }
  return out;
}
- (int)degreeMidiForSynth:(int)synth degree:(int)degree {
  if (!_session || synth < 0 || synth >= ld::kNumSynths) return 60;
  return ld::degreeToMidi(_session->project(), _session->project().synths[synth].octaveShift, degree);
}

- (void)setDrumEngine:(int)engine { if (_session) _session->setDrumEngine(engine); }
- (void)applyJungleTuningPitch:(float)pitch tight:(float)tight crunch:(float)crunch {
  if (_session) _session->applyJungleTuning(pitch, tight, crunch);
}
- (BOOL)loadJungleBreakWav:(NSString *)path {
  return _session && path && _session->loadJungleBreakWav(path.UTF8String);
}
- (void)resampleSceneToJungle:(int)scene { if (_session) _session->resampleSceneToJungle(scene); }

- (int)sceneBars:(int)scene {
  return (_session && scene >= 0 && scene < ld::kNumScenes) ? _session->project().scenes[scene].bars : 1;
}
- (void)setSceneBars:(int)scene bars:(int)bars { if (_session) _session->setSceneBars(scene, bars); }
- (void)copySceneFrom:(int)from to:(int)to { if (_session) _session->copyScene(from, to); }
- (void)clearScene:(int)scene { if (_session) _session->clearScene(scene); }
- (NSArray<NSNumber *> *)chain {
  NSMutableArray<NSNumber *> *out = [NSMutableArray array];
  if (!_session) return out;
  const ld::Project &p = _session->project();
  for (int i = 0; i < p.chainLen; ++i) [out addObject:@(p.chain[i])];
  return out;
}
- (void)chainAdd:(int)scene { if (_session) _session->chainAdd(scene); }
- (void)chainRemoveAt:(int)pos { if (_session) _session->chainRemove(pos); }
- (void)resetToDemo { if (_session) _session->resetToDemo(); }

- (void)setChainAutoPos:(int)pos track:(int)track lane:(int)lane paramId:(int)paramId startVal:(float)startVal endVal:(float)endVal {
  if (_session) _session->setChainAuto(pos, track, lane, paramId, startVal, endVal);
}
- (NSArray<NSNumber *> *)chainAutoPos:(int)pos track:(int)track {
  NSMutableArray<NSNumber *> *out = [NSMutableArray arrayWithCapacity:9];
  if (!_session || pos < 0 || pos >= _session->project().chainLen || track < 0 || track >= ld::kNumTracks) {
    return @[@(-1), @(0), @(0), @(-1), @(0), @(0), @(-1), @(0), @(0)];
  }
  for (int ln = 0; ln < ld::kAutoPerTrack; ++ln) {
    const auto &al = _session->project().chainAuto[pos].tracks[track][ln];
    [out addObject:@(al.paramId)];
    [out addObject:@(al.startVal)];
    [out addObject:@(al.endVal)];
  }
  return out;
}
- (void)setFxSlotTrack:(int)track slot:(int)slot type:(int)type amount:(float)amount rate:(float)rate bypass:(BOOL)bypass {
  if (_session) _session->setFxSlot(track, slot, type, amount, rate, bypass);
}
- (NSArray<NSNumber *> *)fxSlotTrack:(int)track slot:(int)slot {
  if (!_session || track < 0 || track >= ld::kNumTracks || slot < 0 || slot >= ld::kFxSlots) {
    return @[@(0), @(0.5f), @(0.4f), @(0)];
  }
  const auto &fs = _session->project().fx[track].slots[slot];
  return @[@(int(fs.type)), @(fs.amount), @(fs.rate), @(fs.bypass ? 1.0f : 0.0f)];
}
- (void)applyInstrumentSynth:(int)synth preset:(int)preset { if (_session) _session->applyInstrument(synth, preset); }
- (void)setActiveSynths:(int)count { if (_session) _session->setActiveSynths(count); }


- (float)defaultParam:(int)paramId index:(int)index { return ld::Session::defaultParam(paramId, index); }

- (NSString *)importContent:(NSString *)json scene:(int)scene synth:(int)synth tempoKey:(BOOL)tempoKey {
  std::string err = "the audio engine is not running";
  if (_session && json) {
    ld::ContentTarget tg;
    tg.scene = scene; tg.synth = synth; tg.tempoAndKey = tempoKey;
    if (_session->importContent(std::string(json.UTF8String), tg, err)) err.clear();
  }
  return [NSString stringWithUTF8String:err.c_str()] ?: @"could not be loaded";
}

- (NSArray<NSString *> *)contentInfo:(NSString *)json {
  if (!json) return @[];
  std::string err;
  ld::ContentInfo info;
  if (!ld::checkContent(std::string(json.UTF8String), err, &info)) return @[];
  return @[[NSString stringWithUTF8String:info.kind.c_str()] ?: @"",
           [NSString stringWithUTF8String:info.name.c_str()] ?: @"",
           [NSString stringWithUTF8String:info.style.c_str()] ?: @""];
}

- (BOOL)saveProjectCopy:(NSString *)path { return _session && path && _session->saveCopy(path.UTF8String); }
- (BOOL)openProject:(NSString *)path { return _session && path && _session->openProject(path.UTF8String); }

- (void)exportTo:(NSString *)path midi:(BOOL)midi song:(BOOL)song scene:(int)scene stem:(int)stem completion:(void (^)(BOOL ok))completion {
  if (!_session || !path) { if (completion) completion(NO); return; }
  // The copy is taken here, on the UI thread. The worker renders from it, so editing can go on meanwhile.
  std::shared_ptr<ld::Project> proj = std::make_shared<ld::Project>(_session->project());
  std::shared_ptr<const ld::Sample> brk = _session->activeBreak();
  const std::string file = path.UTF8String;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    bool ok = false;
    if (midi) {
      const std::vector<uint8_t> bytes = ld::buildMidi(*proj, song, scene);
      FILE *f = std::fopen(file.c_str(), "wb");
      if (f) {
        ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        ok = (std::fclose(f) == 0) && ok;
      }
    } else {
      ld::RenderOptions o;
      o.songMode = song; o.scene = scene; o.soloTrack = stem;
      if (brk) for (int i = 0; i < ld::kNumPads; ++i) o.samples[i] = brk.get();
      const ld::RenderResult r = ld::renderOffline(*proj, o);
      ok = ld::writeWav16(file.c_str(), r.l.data(), r.r.data(), r.l.size(), r.sampleRate);
    }
    dispatch_async(dispatch_get_main_queue(), ^{ if (completion) completion(ok ? YES : NO); });
  });
}

- (NSString *)exportContent:(int)kind scene:(int)scene synth:(int)synth name:(NSString *)name {
  std::string out;
  if (_session) {
    const std::string nm = name ? std::string(name.UTF8String) : std::string();
    if (kind == 0) out = ld::instrumentToJson(_session->project(), synth, nm);
    else if (kind == 1) out = ld::drumkitToJson(_session->project(), nm);
    else out = ld::grooveToJson(_session->project(), scene, nm);
  }
  return [NSString stringWithUTF8String:out.c_str()] ?: @"";
}

- (void)loadStartPreset { if (_session) _session->loadStartPreset(); }
- (void)writeBreakPattern:(int)scene { if (_session) _session->writeBreakPattern(scene); }
- (NSArray<NSNumber *> *)padWave:(int)pad count:(int)count {
  int n = count < 1 ? 1 : (count > 256 ? 256 : count);
  float buf[256] = {};
  if (_session) _session->padWaveform(pad, buf, n);
  NSMutableArray<NSNumber *> *out = [NSMutableArray arrayWithCapacity:n];
  for (int i = 0; i < n; ++i) [out addObject:@(buf[i])];
  return out;
}
- (BOOL)hasOwnBreak { return _session && _session->activeBreak(); }
- (BOOL)bounceScene:(int)scene { return _session && _session->resampleSceneToJungle(scene); }
- (void)applyDrumSynthTemplate:(int)pad tmpl:(int)tmpl { if (_session) _session->applyDrumSynthTemplate(pad, tmpl); }
- (void)clearCustomPad:(int)pad { if (_session) _session->clearCustomPad(pad); }
- (BOOL)padIsSynth:(int)pad {
  return _session && pad >= 0 && pad < ld::kNumPads && _session->project().customDrums[pad].enabled;
}

- (BOOL)toggleNote:(int)scene synth:(int)synth step:(int)step degree:(int)degree accent:(BOOL)accent slide:(BOOL)slide {
  return _session && _session->toggleNote(scene, synth, step, degree, accent, 1, slide);
}
- (NSArray<NSNumber *> *)slideNotes:(int)scene synth:(int)synth {
  NSMutableArray<NSNumber *> *out = [NSMutableArray array];
  if (!_session || scene < 0 || scene >= ld::kNumScenes || synth < 0 || synth >= ld::kNumSynths) return out;
  const ld::SynthPattern &p = _session->project().scenes[scene].synth[synth];
  for (int i = 0; i < p.count; ++i) {
    if (p.notes[i].flags & ld::kNoteSlide) { [out addObject:@(p.notes[i].step)]; [out addObject:@(p.notes[i].degree)]; }
  }
  return out;
}
- (void)setChainAutoLive:(int)pos track:(int)track lane:(int)lane paramId:(int)paramId startVal:(float)startVal endVal:(float)endVal {
  if (_session) _session->setChainAuto(pos, track, lane, paramId, startVal, endVal, false);
}

@end
