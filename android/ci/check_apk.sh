#!/usr/bin/env bash
# Static checks on the built APK: signature, native library, page alignment, and that every
# function the Kotlin side declares really exists in the native library.
set -euo pipefail
APK="$1"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="$(mktemp -d)"
unzip -q "$APK" -d "$WORK"

APKSIGNER="$(ls "$ANDROID_HOME"/build-tools/*/apksigner | sort -V | tail -1)"
"$APKSIGNER" verify --print-certs "$APK" | head -3

for abi in arm64-v8a x86_64; do
  so="$WORK/lib/$abi/libloopdraft_jni.so"
  test -f "$so" || { echo "missing $so"; exit 1; }
  align="$(readelf -lW "$so" | awk '$1=="LOAD"{print $NF}' | sort -u | tr '\n' ' ')"
  echo "$abi: $(stat -c %s "$so") bytes, LOAD alignment $align"
  case "$align" in *0x1000*) echo "not aligned for 16 KB pages"; exit 1;; esac
  readelf --dyn-syms -W "$so" | grep -o 'Java_com_loopdraft_app_Native_[A-Za-z0-9]*' | sed 's/.*Native_//' | sort -u > "$WORK/native.txt"
  grep -o 'external fun [A-Za-z0-9]*' "$ROOT/android/app/src/main/java/com/loopdraft/app/Native.kt" | awk '{print $3}' | sort -u > "$WORK/kotlin.txt"
  diff "$WORK/native.txt" "$WORK/kotlin.txt" || { echo "Kotlin and native function lists differ"; exit 1; }
done
echo "APK checks passed: $(wc -l < "$WORK/kotlin.txt") bridge functions, $(stat -c %s "$APK") bytes"
