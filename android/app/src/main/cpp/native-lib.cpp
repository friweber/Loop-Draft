// Loop Draft - Android glue: Oboe audio output and the JNI bridge to the Kotlin UI.
// The UI thread calls the JNI functions; Oboe's audio thread calls onAudioReady. They only meet inside
// the engine's lock-free queues.
#include <jni.h>

#include <cstdio>
#include <cstring>

#include <android/log.h>
#include <oboe/Oboe.h>

#include <atomic>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "loopdraft/content.h"
#include "loopdraft/session.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "LoopDraft", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "LoopDraft", __VA_ARGS__)

namespace {

// Tiny numbers (denormals) make float maths very slow on ARM. This switches the audio thread to treat them as zero.
inline void flushDenormalsToZero() {
#if defined(__aarch64__)
  uint64_t fpcr;
  __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
  fpcr |= (1ull << 24);
  __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#endif
}

class Host : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
 public:
  bool start(const std::string& filesDir) {
    std::lock_guard<std::mutex> g(lock_);
    savePath_ = filesDir + "/autosave.ldp";
    presetSeenPath_ = filesDir + "/preset_warehouse303.seen";
    beforePresetPath_ = filesDir + "/project_before_preset.ldp";
    wantRunning_ = true;
    return stream_ ? true : open();
  }

  void stop() {
    std::lock_guard<std::mutex> g(lock_);
    wantRunning_ = false;
    if (session_) { session_->engine().stop(); session_->save(savePath_.c_str()); }
    if (stream_) { stream_->stop(); stream_->close(); stream_.reset(); }
    tuner_.reset();
  }

  ld::Session* session() { return session_.get(); }
  void saveIfDirty() { if (session_ && session_->dirty()) session_->save(savePath_.c_str()); }

  std::string info() {
    std::lock_guard<std::mutex> g(lock_);
    if (!stream_) return "audio off";
    char buf[240];
    const auto lat = stream_->calculateLatencyMillis();
    const auto xr = stream_->getXRunCount();
    const double bufMs = 1000.0 * stream_->getBufferSizeInFrames() / double(stream_->getSampleRate());
    // load: how much of the time between two callbacks the engine needs. peak: the highest since the last look.
    const int peak = int(loadPeak_.exchange(0.0f, std::memory_order_relaxed) * 100.0f + 0.5f);
    std::snprintf(buf, sizeof(buf), "%s %d Hz, burst %d, buffer %d (%.0f ms)%s, load %d%% peak %d%%, dropouts %d",
                  oboe::convertToText(stream_->getAudioApi()),
                  stream_->getSampleRate(), stream_->getFramesPerBurst(), stream_->getBufferSizeInFrames(), bufMs,
                  lat ? (", out " + std::to_string(int(lat.value() + 0.5)) + " ms").c_str() : "",
                  int(loadAvg_.load(std::memory_order_relaxed) * 100.0f + 0.5f), peak, xr ? xr.value() : -1);
    return buf;
  }

  // 0 fast (2 bursts), 1 balanced (4), 2 safe (8). The buffer still grows by itself when the phone cannot keep up.
  void setAudioMode(int mode) {
    std::lock_guard<std::mutex> g(lock_);
    audioMode_ = mode < 0 ? 0 : (mode > 2 ? 2 : mode);
    applyAudioMode();
  }

  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) override {
    if (!denormalsOff_) { flushDenormalsToZero(); denormalsOff_ = true; }
    timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    float* out = static_cast<float*>(data);
    if (session_ && s->getChannelCount() == 2) session_->engine().renderInterleaved(out, frames);
    else std::memset(out, 0, sizeof(float) * size_t(frames) * size_t(s->getChannelCount()));
    timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
    const double took = double(t1.tv_sec - t0.tv_sec) + double(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    const float load = float(took * double(s->getSampleRate()) / double(frames > 0 ? frames : 1));
    const float avg = loadAvg_.load(std::memory_order_relaxed);
    loadAvg_.store(avg + 0.02f * (load - avg), std::memory_order_relaxed);
    if (load > loadPeak_.load(std::memory_order_relaxed)) loadPeak_.store(load, std::memory_order_relaxed);
    if (tuner_) tuner_->tune();            // a dropout makes the buffer one burst longer, so it does not repeat
    return oboe::DataCallbackResult::Continue;
  }

  // Called on its own thread when the output device goes away (headphones unplugged, Bluetooth switched, ...).
  void onErrorAfterClose(oboe::AudioStream*, oboe::Result r) override {
    LOGI("stream closed: %s", oboe::convertToText(r));
    std::lock_guard<std::mutex> g(lock_);
    tuner_.reset();
    stream_.reset();
    if (wantRunning_) open();
  }

 private:
  bool open() {
    oboe::AudioStreamBuilder b;
    b.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setUsage(oboe::Usage::Game)
        ->setContentType(oboe::ContentType::Music)
        ->setFormatConversionAllowed(true)
        ->setChannelConversionAllowed(true)
        ->setDataCallback(this)
        ->setErrorCallback(this);
    if (sampleRate_ > 0) {   // the engine is already built for this rate: let Oboe convert if the new device differs
      b.setSampleRate(sampleRate_)->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium);
    }
    denormalsOff_ = false;
    const oboe::Result r = b.openStream(stream_);
    if (r != oboe::Result::OK || !stream_) { LOGE("cannot open audio: %s", oboe::convertToText(r)); stream_.reset(); return false; }
    tuner_ = std::make_unique<oboe::LatencyTuner>(*stream_);
    applyAudioMode();
    // Tells the phone how long each callback may take, so it keeps a fast enough core awake for the audio thread.
    stream_->setPerformanceHintEnabled(true);
    loadAvg_.store(0.0f); loadPeak_.store(0.0f);
    if (!session_) {
      sampleRate_ = stream_->getSampleRate();
      session_ = std::make_unique<ld::Session>(double(sampleRate_));
      const bool restored = session_->load(savePath_.c_str());
      FILE* seen = std::fopen(presetSeenPath_.c_str(), "rb");
      if (seen) {
        std::fclose(seen);
        if (restored) LOGI("project restored");
        else { session_->loadStartPreset(); LOGI("starting with the Warehouse 303 preset"); }
      } else {
        // First start of a build that has the preset: open with it once. A project from an earlier
        // build is kept as a file, and UNDO brings it back right away.
        if (restored) session_->save(beforePresetPath_.c_str());
        session_->loadStartPreset();
        if (FILE* m = std::fopen(presetSeenPath_.c_str(), "wb")) std::fclose(m);
        LOGI("starting with the Warehouse 303 preset (first start, earlier project kept=%d)", restored ? 1 : 0);
      }
    }
    const oboe::Result s = stream_->requestStart();
    if (s != oboe::Result::OK) { LOGE("cannot start audio: %s", oboe::convertToText(s)); stream_->close(); stream_.reset(); tuner_.reset(); return false; }
    return true;
  }

  void applyAudioMode() {                  // lock_ is held
    if (!stream_ || !tuner_) return;
    static const int bursts[3] = {2, 4, 8};
    int32_t want = stream_->getFramesPerBurst() * bursts[audioMode_];
    const int32_t cap = stream_->getBufferCapacityInFrames();
    if (cap > 0 && want > cap) want = cap;
    tuner_->setMinimumBufferSize(want);
    tuner_->requestReset();                // the audio thread sets the size at its next callback
    stream_->setBufferSizeInFrames(want);
  }

  std::string presetSeenPath_, beforePresetPath_;
  std::mutex lock_;                       // guards opening and closing the stream, never taken on the audio thread
  std::shared_ptr<oboe::AudioStream> stream_;
  std::unique_ptr<ld::Session> session_;
  std::string savePath_;
  int32_t sampleRate_ = 0;
  bool wantRunning_ = false;
  bool denormalsOff_ = false;
  std::unique_ptr<oboe::LatencyTuner> tuner_;   // made before the stream starts, freed after it is closed
  int audioMode_ = 1;
  std::atomic<float> loadAvg_{0.0f}, loadPeak_{0.0f};
};

Host g_host;
inline ld::Session* S() { return g_host.session(); }
inline bool sceneOk(int s) { return s >= 0 && s < ld::kNumScenes; }

}  // namespace

#define JNI(ret, name) extern "C" JNIEXPORT ret JNICALL Java_com_loopdraft_app_Native_##name

// ---- lifecycle ----
JNI(jboolean, start)(JNIEnv* env, jobject, jstring dir) {
  const char* c = env->GetStringUTFChars(dir, nullptr);
  const std::string d = c ? c : "";
  if (c) env->ReleaseStringUTFChars(dir, c);
  return g_host.start(d) ? JNI_TRUE : JNI_FALSE;
}
JNI(void, stop)(JNIEnv*, jobject) { g_host.stop(); }
JNI(jstring, info)(JNIEnv* env, jobject) { return env->NewStringUTF(g_host.info().c_str()); }
JNI(void, setAudioMode)(JNIEnv*, jobject, jint mode) { g_host.setAudioMode(mode); }
JNI(void, saveIfDirty)(JNIEnv*, jobject) { g_host.saveIfDirty(); }
JNI(jboolean, poll)(JNIEnv*, jobject) { return S() && S()->poll() ? JNI_TRUE : JNI_FALSE; }

// ---- transport and live play ----
JNI(void, play)(JNIEnv*, jobject, jint scene) { if (S()) S()->engine().play(scene); }
JNI(void, stopTransport)(JNIEnv*, jobject) { if (S()) S()->engine().stop(); }
JNI(void, queueScene)(JNIEnv*, jobject, jint scene) { if (S()) S()->engine().queueScene(scene); }
JNI(void, setSongMode)(JNIEnv*, jobject, jboolean on) { if (S()) S()->engine().setSongMode(on == JNI_TRUE); }
JNI(void, setRecording)(JNIEnv*, jobject, jboolean on) { if (S()) S()->engine().setRecording(on == JNI_TRUE); }
JNI(void, triggerPad)(JNIEnv*, jobject, jint pad, jint vel) { if (S()) S()->engine().triggerPad(pad, vel); }
JNI(void, noteOn)(JNIEnv*, jobject, jint synth, jint degree, jint vel) { if (S()) S()->engine().noteOn(synth, degree, vel); }
JNI(void, noteOff)(JNIEnv*, jobject, jint synth, jint degree) { if (S()) S()->engine().noteOff(synth, degree); }

// ---- status ----
JNI(jboolean, isPlaying)(JNIEnv*, jobject) { return S() && S()->engine().isPlaying() ? JNI_TRUE : JNI_FALSE; }
JNI(jint, currentStep)(JNIEnv*, jobject) { return S() ? S()->engine().currentStep() : -1; }
JNI(jint, currentScene)(JNIEnv*, jobject) { return S() ? S()->engine().currentScene() : 0; }
JNI(jint, queuedScene)(JNIEnv*, jobject) { return S() ? S()->engine().queuedScene() : -1; }
JNI(jint, chainPos)(JNIEnv*, jobject) { return S() ? S()->engine().chainPos() : 0; }
JNI(jfloat, peak)(JNIEnv*, jobject, jint ch) { return S() ? S()->engine().peak(ch) : 0.0f; }

// ---- undo ----
JNI(void, beginEdit)(JNIEnv*, jobject) { if (S()) S()->beginEdit(); }
JNI(jboolean, undo)(JNIEnv*, jobject) { return S() && S()->undo() ? JNI_TRUE : JNI_FALSE; }
JNI(jboolean, redo)(JNIEnv*, jobject) { return S() && S()->redo() ? JNI_TRUE : JNI_FALSE; }
JNI(jboolean, canUndo)(JNIEnv*, jobject) { return S() && S()->canUndo() ? JNI_TRUE : JNI_FALSE; }
JNI(jboolean, canRedo)(JNIEnv*, jobject) { return S() && S()->canRedo() ? JNI_TRUE : JNI_FALSE; }

// ---- parameters ----
JNI(jfloat, getParam)(JNIEnv*, jobject, jint id, jint index) { return S() ? S()->getParam(id, index) : 0.0f; }
JNI(void, setParam)(JNIEnv*, jobject, jint id, jint index, jfloat v) { if (S()) S()->setParam(id, index, v); }

// ---- patterns ----
JNI(jint, toggleStep)(JNIEnv*, jobject, jint scene, jint pad, jint step, jboolean accent) {
  return S() ? S()->toggleDrumStep(scene, pad, step, accent == JNI_TRUE) : 0;
}
// 8 rows of kMaxSteps (256) steps, row after row.
JNI(jbyteArray, drumGrid)(JNIEnv* env, jobject, jint scene) {
  const jsize n = ld::kNumPads * ld::kMaxSteps;
  jbyteArray arr = env->NewByteArray(n);
  if (arr && S() && sceneOk(scene))
    env->SetByteArrayRegion(arr, 0, n, reinterpret_cast<const jbyte*>(S()->project().scenes[scene].drums.steps));
  return arr;
}
JNI(jboolean, toggleNote)(JNIEnv*, jobject, jint scene, jint synth, jint step, jint degree, jboolean accent, jboolean slide) {
  return S() && S()->toggleNote(scene, synth, step, degree, accent == JNI_TRUE, 1, slide == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}
// Flat list: step, degree, length, velocity, flags (1 = slide) for every note.
JNI(jintArray, notes)(JNIEnv* env, jobject, jint scene, jint synth) {
  if (!S() || !sceneOk(scene) || synth < 0 || synth >= ld::kNumSynths) return env->NewIntArray(0);
  const ld::SynthPattern& p = S()->project().scenes[scene].synth[synth];
  const jsize n = p.count * 5;
  jintArray arr = env->NewIntArray(n);
  if (!arr || n == 0) return arr;
  jint* d = env->GetIntArrayElements(arr, nullptr);
  if (!d) return arr;
  for (int i = 0; i < p.count; ++i) {
    d[i * 5] = p.notes[i].step; d[i * 5 + 1] = p.notes[i].degree; d[i * 5 + 2] = p.notes[i].len; d[i * 5 + 3] = p.notes[i].vel;
    d[i * 5 + 4] = p.notes[i].flags;
  }
  env->ReleaseIntArrayElements(arr, d, 0);
  return arr;
}
JNI(jint, degreeMidi)(JNIEnv*, jobject, jint synth, jint degree) {
  if (!S() || synth < 0 || synth >= ld::kNumSynths) return 60;
  return ld::degreeToMidi(S()->project(), S()->project().synths[synth].octaveShift, degree);
}

// ---- scenes and song ----
JNI(jint, sceneBars)(JNIEnv*, jobject, jint scene) { return S() && sceneOk(scene) ? S()->project().scenes[scene].bars : 1; }
JNI(void, setSceneBars)(JNIEnv*, jobject, jint scene, jint bars) { if (S()) S()->setSceneBars(scene, bars); }
JNI(void, copyScene)(JNIEnv*, jobject, jint from, jint to) { if (S()) S()->copyScene(from, to); }
JNI(void, clearScene)(JNIEnv*, jobject, jint scene) { if (S()) S()->clearScene(scene); }
JNI(jintArray, chain)(JNIEnv* env, jobject) {
  if (!S()) return env->NewIntArray(0);
  const ld::Project& p = S()->project();
  jintArray arr = env->NewIntArray(p.chainLen);
  if (arr) env->SetIntArrayRegion(arr, 0, p.chainLen, reinterpret_cast<const jint*>(p.chain));
  return arr;
}
JNI(void, chainAdd)(JNIEnv*, jobject, jint scene) { if (S()) S()->chainAdd(scene); }
JNI(void, chainRemove)(JNIEnv*, jobject, jint pos) { if (S()) S()->chainRemove(pos); }
JNI(jfloat, defaultParam)(JNIEnv*, jobject, jint id, jint index) { return ld::Session::defaultParam(id, index); }
JNI(void, resetToDemo)(JNIEnv*, jobject) { if (S()) S()->resetToDemo(); }
JNI(void, loadStartPreset)(JNIEnv*, jobject) { if (S()) S()->loadStartPreset(); }

JNI(void, setDrumStep)(JNIEnv*, jobject, jint scene, jint pad, jint step, jint value) {
  if (S()) S()->setDrumStep(scene, pad, step, value);
}
JNI(void, loadJungleKit)(JNIEnv*, jobject) { if (S()) S()->loadJungleKit(); }
JNI(void, loadStandardKit)(JNIEnv*, jobject) { if (S()) S()->loadStandardKit(); }
JNI(void, setChainAuto)(JNIEnv*, jobject, jint pos, jint track, jint lane, jint paramId, jfloat startVal, jfloat endVal) {
  if (S()) S()->setChainAuto(pos, track, lane, paramId, startVal, endVal);
}
JNI(void, setChainAutoLive)(JNIEnv*, jobject, jint pos, jint track, jint lane, jint paramId, jfloat startVal, jfloat endVal) {
  if (S()) S()->setChainAuto(pos, track, lane, paramId, startVal, endVal, false);
}
// Named projects: plain files the app keeps in its own folder.
JNI(jboolean, saveProjectCopy)(JNIEnv* env, jobject, jstring path) {
  if (!S() || !path) return JNI_FALSE;
  const char* c = env->GetStringUTFChars(path, nullptr);
  const bool ok = c && S()->saveCopy(c);
  if (c) env->ReleaseStringUTFChars(path, c);
  return ok ? JNI_TRUE : JNI_FALSE;
}
JNI(jboolean, openProject)(JNIEnv* env, jobject, jstring path) {
  if (!S() || !path) return JNI_FALSE;
  const char* c = env->GetStringUTFChars(path, nullptr);
  const bool ok = c && S()->openProject(c);
  if (c) env->ReleaseStringUTFChars(path, c);
  return ok ? JNI_TRUE : JNI_FALSE;
}
JNI(jfloatArray, chainAuto)(JNIEnv* env, jobject, jint pos, jint track) {
  jfloatArray arr = env->NewFloatArray(9);
  if (!arr || !S()) return arr;
  const ld::Project& p = S()->project();
  float out[9] = {-1, 0, 0, -1, 0, 0, -1, 0, 0};
  if (pos >= 0 && pos < p.chainLen && track >= 0 && track < ld::kNumTracks) {
    for (int ln = 0; ln < ld::kAutoPerTrack; ++ln) {
      const auto& al = p.chainAuto[pos].tracks[track][ln];
      out[ln * 3 + 0] = float(al.paramId);
      out[ln * 3 + 1] = al.startVal;
      out[ln * 3 + 2] = al.endVal;
    }
  }
  env->SetFloatArrayRegion(arr, 0, 9, out);
  return arr;
}
JNI(void, writeBreakPattern)(JNIEnv*, jobject, jint scene) { if (S()) S()->writeBreakPattern(scene); }

// Outline of a pad's sound for the display: n peak values 0..1.
JNI(jfloatArray, padWave)(JNIEnv* env, jobject, jint pad, jint n) {
  if (n < 1) n = 1;
  if (n > 256) n = 256;
  jfloatArray arr = env->NewFloatArray(n);
  if (!arr || !S()) return arr;
  float buf[256] = {};
  S()->padWaveform(pad, buf, n);
  env->SetFloatArrayRegion(arr, 0, n, buf);
  return arr;
}

// ---- export ----
// The UI thread takes a snapshot of the project; a worker thread then renders from that copy,
// so editing can go on while a long song is being rendered.
namespace {
std::mutex g_exportLock;
std::unique_ptr<ld::Project> g_exportProject;
std::shared_ptr<const ld::Sample> g_exportBreak;   // the user's own break, if the project plays one
bool exportCopy(ld::Project& out) {
  std::lock_guard<std::mutex> g(g_exportLock);
  if (!g_exportProject) return false;
  out = *g_exportProject;
  return true;
}
std::string jstr(JNIEnv* env, jstring s) {
  const char* c = env->GetStringUTFChars(s, nullptr);
  const std::string out = c ? c : "";
  if (c) env->ReleaseStringUTFChars(s, c);
  return out;
}
}  // namespace

JNI(void, snapshotForExport)(JNIEnv*, jobject) {
  if (!S()) return;
  std::lock_guard<std::mutex> g(g_exportLock);
  g_exportProject = std::make_unique<ld::Project>(S()->project());
  g_exportBreak = S()->activeBreak();
}
JNI(jboolean, exportMidi)(JNIEnv* env, jobject, jstring path, jboolean song, jint scene) {
  auto p = std::make_unique<ld::Project>();
  if (!exportCopy(*p)) return JNI_FALSE;
  const std::vector<uint8_t> bytes = ld::buildMidi(*p, song == JNI_TRUE, scene);
  FILE* f = std::fopen(jstr(env, path).c_str(), "wb");
  if (!f) return JNI_FALSE;
  const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
  return (std::fclose(f) == 0 && ok) ? JNI_TRUE : JNI_FALSE;
}
// stem: -1 = full mix, 0 = drums, 1 = bass, 2 = lead. Safe to call from a worker thread.
JNI(jboolean, exportWav)(JNIEnv* env, jobject, jstring path, jboolean song, jint scene, jint stem) {
  auto p = std::make_unique<ld::Project>();
  if (!exportCopy(*p)) return JNI_FALSE;
  ld::RenderOptions o;
  o.songMode = song == JNI_TRUE; o.scene = scene; o.soloTrack = stem;
  std::shared_ptr<const ld::Sample> brk;
  { std::lock_guard<std::mutex> g(g_exportLock); brk = g_exportBreak; }
  if (brk) for (int i = 0; i < ld::kNumPads; ++i) o.samples[i] = brk.get();
  const ld::RenderResult r = ld::renderOffline(*p, o);
  return ld::writeWav16(jstr(env, path).c_str(), r.l.data(), r.r.data(), r.l.size(), r.sampleRate) ? JNI_TRUE : JNI_FALSE;
}
JNI(void, setFxSlot)(JNIEnv*, jobject, jint track, jint slot, jint type, jfloat amount, jfloat rate, jboolean bypass) {
  if (S()) S()->setFxSlot(track, slot, type, amount, rate, bypass == JNI_TRUE);
}
JNI(void, setFxSlotLive)(JNIEnv*, jobject, jint track, jint slot, jint type, jfloat amount, jfloat rate, jboolean bypass) {
  if (S()) S()->setFxSlot(track, slot, type, amount, rate, bypass == JNI_TRUE, false);
}
JNI(jfloatArray, fxSlot)(JNIEnv* env, jobject, jint track, jint slot) {
  jfloatArray arr = env->NewFloatArray(4);
  if (!arr || !S()) return arr;
  const ld::Project& p = S()->project();
  float out[4] = {0, 0.5f, 0.4f, 0};
  if (track >= 0 && track < ld::kNumTracks && slot >= 0 && slot < ld::kFxSlots) {
    const auto& fs = p.fx[track].slots[slot];
    out[0] = float(fs.type); out[1] = fs.amount; out[2] = fs.rate; out[3] = fs.bypass ? 1.0f : 0.0f;
  }
  env->SetFloatArrayRegion(arr, 0, 4, out);
  return arr;
}
JNI(void, applyInstrument)(JNIEnv*, jobject, jint synth, jint preset) {
  if (S()) S()->applyInstrument(synth, preset);
}
JNI(void, setActiveSynths)(JNIEnv*, jobject, jint count) {
  if (S()) S()->setActiveSynths(count);
}

JNI(void, setDrumEngine)(JNIEnv*, jobject, jint engine) {
  if (S()) S()->setDrumEngine(engine);
}
JNI(void, applyJungleTuning)(JNIEnv*, jobject, jfloat pitch, jfloat tight, jfloat crunch) {
  if (S()) S()->applyJungleTuning(pitch, tight, crunch);
}
JNI(jboolean, loadJungleBreakWav)(JNIEnv* env, jobject, jstring path) {
  if (!S() || !path) return JNI_FALSE;
  const char* c = env->GetStringUTFChars(path, nullptr);
  const bool ok = c && S()->loadJungleBreakWav(c);
  if (c) env->ReleaseStringUTFChars(path, c);
  return ok ? JNI_TRUE : JNI_FALSE;
}
JNI(jboolean, resampleSceneToJungle)(JNIEnv*, jobject, jint scene) {
  return S() && S()->resampleSceneToJungle(scene) ? JNI_TRUE : JNI_FALSE;
}
JNI(jboolean, hasOwnBreak)(JNIEnv*, jobject) {
  return S() && S()->activeBreak() ? JNI_TRUE : JNI_FALSE;
}

JNI(void, setKitHardness)(JNIEnv*, jobject, jfloat hardness) {
  if (S()) S()->setKitHardness(hardness);
}
JNI(void, synthesizeCustomPad)(JNIEnv*, jobject, jint pad, jboolean audition) {
  if (S()) S()->synthesizeCustomPad(pad, audition == JNI_TRUE);
}
JNI(void, applyDrumSynthTemplate)(JNIEnv*, jobject, jint pad, jint tmpl) {
  if (S()) S()->applyDrumSynthTemplate(pad, tmpl);
}
JNI(void, clearCustomPad)(JNIEnv*, jobject, jint pad) { if (S()) S()->clearCustomPad(pad); }
JNI(jboolean, padIsSynth)(JNIEnv*, jobject, jint pad) {
  return S() && pad >= 0 && pad < ld::kNumPads && S()->project().customDrums[pad].enabled ? JNI_TRUE : JNI_FALSE;
}

// Presets and grooves as JSON text (engine/include/loopdraft/content.h). The result is "" when all went well,
// otherwise the error message for the user.
JNI(jstring, importContent)(JNIEnv* env, jobject, jstring json, jint scene, jint synth, jboolean tempoKey) {
  std::string err = "the audio engine is not running";
  if (S()) {
    ld::ContentTarget tg;
    tg.scene = scene; tg.synth = synth; tg.tempoAndKey = tempoKey == JNI_TRUE;
    if (S()->importContent(jstr(env, json), tg, err)) err.clear();
  }
  return env->NewStringUTF(err.c_str());
}
JNI(jstring, checkContent)(JNIEnv* env, jobject, jstring json) {
  std::string err;
  if (ld::checkContent(jstr(env, json), err)) err.clear();
  return env->NewStringUTF(err.c_str());
}
// kind: 0 instrument (of `synth`), 1 drum kit, 2 groove (of `scene`, with its sounds)
JNI(jstring, exportContent)(JNIEnv* env, jobject, jint kind, jint scene, jint synth, jstring name) {
  std::string out;
  if (S()) {
    const std::string nm = jstr(env, name);
    if (kind == 0) out = ld::instrumentToJson(S()->project(), synth, nm);
    else if (kind == 1) out = ld::drumkitToJson(S()->project(), nm);
    else out = ld::grooveToJson(S()->project(), scene, nm);
  }
  return env->NewStringUTF(out.c_str());
}
