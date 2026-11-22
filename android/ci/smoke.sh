#!/usr/bin/env bash
# Runs on an emulator in CI: installs the APK, starts it in self-test mode, and checks that it stays alive,
# that the audio thread is running the sequencer, and that sound is coming out. Saves screenshots and the log.
set -uo pipefail
APK="$1"; OUT="$2"
mkdir -p "$OUT"
fail=0
ACT=com.loopdraft.app/.MainActivity

adb install -r "$APK" || exit 1
adb logcat -c
shot() { sleep 2; adb exec-out screencap -p > "$OUT/$1.png"; }

adb shell am start -W -n "$ACT" --ez selftest true
sleep 12
shot 1_drums_playing
adb shell pidof com.loopdraft.app || { echo "FAIL: app is not running"; fail=1; }

adb shell am start -W -S -n "$ACT" --ei tab 1;            shot 2_synth
adb shell am start -W -S -n "$ACT" --ei tab 1 --ez roll true; shot 3_piano_roll
adb shell am start -W -S -n "$ACT" --ei tab 2;            shot 4_mix
adb shell am start -W -S -n "$ACT" --ei tab 3;            shot 5_song
adb shell pidof com.loopdraft.app || { echo "FAIL: app died while switching screens"; fail=1; }

adb logcat -d > "$OUT/logcat.txt"
grep -E "LoopDraft" "$OUT/logcat.txt" | sed 's/^.*LoopDraft *: //' > "$OUT/selftest.txt"
cat "$OUT/selftest.txt"
if grep -E "FATAL EXCEPTION|UnsatisfiedLinkError|Fatal signal" "$OUT/logcat.txt"; then echo "FAIL: crash in log"; fail=1; fi
grep -q "selftest playing=true" "$OUT/selftest.txt" || { echo "FAIL: transport never started (audio thread not running?)"; fail=1; }
steps=$(grep -o "selftest playing=true step=[0-9]*" "$OUT/selftest.txt" | sort -u | wc -l)
[ "$steps" -ge 3 ] || { echo "FAIL: the step counter is not advancing ($steps distinct steps)"; fail=1; }
grep -E "selftest .*sound=true" "$OUT/selftest.txt" >/dev/null || { echo "FAIL: no sound reached the output"; fail=1; }
grep -q "selftest saved=true" "$OUT/selftest.txt" || { echo "FAIL: project was not saved and restored"; fail=1; }
[ "$fail" = 0 ] && echo "SMOKE TEST PASSED"
exit "$fail"
