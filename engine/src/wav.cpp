#include <cstdio>
#include <cstring>

#include "loopdraft/voices.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_WCHAR
#include "dr_wav.h"

namespace ld {

std::unique_ptr<Sample> loadWav(const char* path) {
  unsigned int channels = 0, rate = 0;
  drwav_uint64 frames = 0;
  float* data = drwav_open_file_and_read_pcm_frames_f32(path, &channels, &rate, &frames, nullptr);
  if (!data) return nullptr;
  auto s = std::make_unique<Sample>();
  if (channels >= 1 && frames >= 2 && rate > 0) {
    s->rate = double(rate);
    s->l.resize(size_t(frames));
    if (channels >= 2) s->r.resize(size_t(frames));
    for (size_t i = 0; i < size_t(frames); ++i) {
      s->l[i] = data[i * channels];
      if (channels >= 2) s->r[i] = data[i * channels + 1];
    }
  } else {
    s.reset();
  }
  drwav_free(data, nullptr);
  return s;
}

bool writeWav16(const char* path, const float* l, const float* r, size_t frames, double sampleRate) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return false;
  const uint32_t sr = uint32_t(sampleRate), bytes = uint32_t(frames * 4);
  unsigned char h[44];
  const auto u32 = [&](int o, uint32_t v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; h[o + 2] = (v >> 16) & 255; h[o + 3] = (v >> 24) & 255; };
  const auto u16 = [&](int o, uint32_t v) { h[o] = v & 255; h[o + 1] = (v >> 8) & 255; };
  std::memcpy(h, "RIFF", 4); u32(4, 36 + bytes); std::memcpy(h + 8, "WAVEfmt ", 8);
  u32(16, 16); u16(20, 1); u16(22, 2); u32(24, sr); u32(28, sr * 4); u16(32, 4); u16(34, 16);
  std::memcpy(h + 36, "data", 4); u32(40, bytes);
  bool ok = std::fwrite(h, 1, 44, f) == 44;
  std::vector<unsigned char> buf(frames * 4);
  const auto put = [&](size_t o, float x) {
    x = x < -1 ? -1 : (x > 1 ? 1 : x);
    const int v = int(std::lrintf(x * 32767.0f));
    buf[o] = (unsigned char)(v & 255); buf[o + 1] = (unsigned char)((v >> 8) & 255);
  };
  for (size_t i = 0; i < frames; ++i) { put(i * 4, l[i]); put(i * 4 + 2, r[i]); }
  ok = ok && std::fwrite(buf.data(), 1, buf.size(), f) == buf.size();
  return std::fclose(f) == 0 && ok;
}

}  // namespace ld
