// Loop Draft - Android glue: Oboe audio output and the JNI bridge to the Kotlin UI.
// The UI thread calls the JNI functions; Oboe's audio thread calls onAudioReady. They only meet inside
// the engine's lock-free queues.
#include <jni.h>

#include <cstdio>
#include <cstring>

#include <android/log.h>
#include <oboe/Oboe.h>

#include <memory>
#include <mutex>
#include <string>

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
    wantRunning_ = true;
    return stream_ ? true : open();
  }

  void stop() {
    std::lock_guard<std::mutex> g(lock_);
    wantRunning_ = false;
    if (session_) { session_->engine().stop(); session_->save(savePath_.c_str()); }
    if (stream_) { stream_->stop(); stream_->close(); stream_.reset(); }
  }

  ld::Session* session() { return session_.get(); }
  void saveIfDirty() { if (session_ && session_->dirty()) session_->save(savePath_.c_str()); }

  std::string info() {
    std::lock_guard<std::mutex> g(lock_);
    if (!stream_) return "audio off";
    char buf[160];
    const auto lat = stream_->calculateLatencyMillis();
    std::snprintf(buf, sizeof(buf), "%s %d Hz, burst %d, buffer %d%s", oboe::convertToText(stream_->getAudioApi()),
                  stream_->getSampleRate(), stream_->getFramesPerBurst(), stream_->getBufferSizeInFrames(),
                  lat ? (", out " + std::to_string(int(lat.value() + 0.5)) + " ms").c_str() : "");
    return buf;
  }

  oboe::DataCallbackResult onAudioReady(oboe::AudioStream* s, void* data, int32_t frames) override {
    if (!denormalsOff_) { flushDenormalsToZero(); denormalsOff_ = true; }
    float* out = static_cast<float*>(data);
    if (session_ && s->getChannelCount() == 2) session_->engine().renderInterleaved(out, frames);
    else std::memset(out, 0, sizeof(float) * size_t(frames) * size_t(s->getChannelCount()));
    return oboe::DataCallbackResult::Continue;
  }

  // Called on its own thread when the output device goes away (headphones unplugged, Bluetooth switched, ...).
  void onErrorAfterClose(oboe::AudioStream*, oboe::Result r) override {
    LOGI("stream closed: %s", oboe::convertToText(r));
    std::lock_guard<std::mutex> g(lock_);
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
    stream_->setBufferSizeInFrames(stream_->getFramesPerBurst() * 2);
    if (!session_) {
      sampleRate_ = stream_->getSampleRate();
      session_ = std::make_unique<ld::Session>(double(sampleRate_));
      if (session_->load(savePath_.c_str())) LOGI("project restored"); else LOGI("starting with the demo project");
    }
    const oboe::Result s = stream_->requestStart();
    if (s != oboe::Result::OK) { LOGE("cannot start audio: %s", oboe::convertToText(s)); stream_->close(); stream_.reset(); return false; }
    return true;
  }

  std::mutex lock_;                       // guards opening and closing the stream, never taken on the audio thread
  std::shared_ptr<oboe::AudioStream> stream_;
  std::unique_ptr<ld::Session> session_;
  std::string savePath_;
  int32_t sampleRate_ = 0;
  bool wantRunning_ = false;
  bool denormalsOff_ = false;
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
// 8 rows of 128 steps, row after row.
JNI(jbyteArray, drumGrid)(JNIEnv* env, jobject, jint scene) {
  const jsize n = ld::kNumPads * ld::kMaxSteps;
  jbyteArray arr = env->NewByteArray(n);
  if (arr && S() && sceneOk(scene))
    env->SetByteArrayRegion(arr, 0, n, reinterpret_cast<const jbyte*>(S()->project().scenes[scene].drums.steps));
  return arr;
}
JNI(jboolean, toggleNote)(JNIEnv*, jobject, jint scene, jint synth, jint step, jint degree, jboolean accent) {
  return S() && S()->toggleNote(scene, synth, step, degree, accent == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}
// Flat list: step, degree, length, velocity for every note.
JNI(jintArray, notes)(JNIEnv* env, jobject, jint scene, jint synth) {
  if (!S() || !sceneOk(scene) || synth < 0 || synth >= ld::kNumSynths) return env->NewIntArray(0);
  const ld::SynthPattern& p = S()->project().scenes[scene].synth[synth];
  const jsize n = p.count * 4;
  jintArray arr = env->NewIntArray(n);
  if (!arr || n == 0) return arr;
  jint* d = env->GetIntArrayElements(arr, nullptr);
  if (!d) return arr;
  for (int i = 0; i < p.count; ++i) {
    d[i * 4] = p.notes[i].step; d[i * 4 + 1] = p.notes[i].degree; d[i * 4 + 2] = p.notes[i].len; d[i * 4 + 3] = p.notes[i].vel;
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
JNI(void, resetToDemo)(JNIEnv*, jobject) { if (S()) S()->resetToDemo(); }

JNI(void, setDrumStep)(JNIEnv*, jobject, jint scene, jint pad, jint step, jint value) {
  if (S()) S()->setDrumStep(scene, pad, step, value);
}
JNI(void, loadJungleKit)(JNIEnv*, jobject) { if (S()) S()->loadJungleKit(); }
JNI(void, loadStandardKit)(JNIEnv*, jobject) { if (S()) S()->loadStandardKit(); }
JNI(void, setChainAuto)(JNIEnv*, jobject, jint pos, jint lane, jint param, jfloat startVal, jfloat endVal) {
  if (S()) S()->setChainAuto(pos, lane, param, startVal, endVal);
}
JNI(jfloatArray, chainAuto)(JNIEnv* env, jobject, jint pos) {
  jfloatArray arr = env->NewFloatArray(6);
  if (!arr || !S()) return arr;
  const ld::Project& p = S()->project();
  float out[6] = {0, 0, 0, 0, 0, 0};
  if (pos >= 0 && pos < p.chainLen) {
    const ld::ChainAuto& ca = p.chainAuto[pos];
    out[0] = float(ca.lanes[0].param); out[1] = ca.lanes[0].startVal; out[2] = ca.lanes[0].endVal;
    out[3] = float(ca.lanes[1].param); out[4] = ca.lanes[1].startVal; out[5] = ca.lanes[1].endVal;
  }
  env->SetFloatArrayRegion(arr, 0, 6, out);
  return arr;
}
