// File names are UTF-8 everywhere in the engine. On Windows the C library wants UTF-16 for anything outside the
// system code page (a user folder with an umlaut), so the three calls the engine needs go through here.
#pragma once
#include <cstdio>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ld {

#ifdef _WIN32
inline std::wstring widePath(const char* utf8) {
  if (!utf8) return std::wstring();
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &w[0], n);
  w.resize(size_t(n - 1));
  return w;
}
inline FILE* openFile(const char* path, const char* mode) {
  return _wfopen(widePath(path).c_str(), widePath(mode).c_str());
}
inline bool removeFile(const char* path) { return _wremove(widePath(path).c_str()) == 0; }
// Replaces `to` if it is there, as rename() does everywhere else.
inline bool renameFile(const char* from, const char* to) {
  return MoveFileExW(widePath(from).c_str(), widePath(to).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}
#else
inline FILE* openFile(const char* path, const char* mode) { return std::fopen(path, mode); }
inline bool removeFile(const char* path) { return std::remove(path) == 0; }
inline bool renameFile(const char* from, const char* to) { return std::rename(from, to) == 0; }
#endif

}  // namespace ld
