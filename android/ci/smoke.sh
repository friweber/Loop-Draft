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
adb shell am start -W -S -n "$ACT" --ei tab 0 --ez jungle true; shot 6_jungle
# landscape, with four synth tracks and an FX chain
adb shell cmd window user-rotation lock 1
sleep 2
adb shell am start -W -S -n "$ACT" --ei tab 0;                shot 7_land_drums
adb shell cmd window user-rotation lock 1; sleep 1
adb shell am start -W -S -n "$ACT" --ei tab 1 --ez four true; shot 8_land_synth
adb shell cmd window user-rotation lock 1; sleep 1
adb shell am start -W -S -n "$ACT" --ei tab 2 --ez four true; shot 9_land_fx
adb shell cmd window user-rotation lock 1; sleep 1
adb shell am start -W -S -n "$ACT" --ei tab 3 --ez four true; shot 10_land_song
adb shell cmd window user-rotation lock 0
sleep 2
adb shell am start -W -S -n "$ACT" --ei tab 2 --ez four true; shot 11_fx_portrait
# the bounced loop must survive leaving the slicer and restarting the app
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit bounce; sleep 9
adb shell am start -W -S -n "$ACT" --ei tab 0;                 shot 12_bounced_after_restart
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit 909;    sleep 9
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit slicer; shot 13_back_in_slicer
# named projects, the Vector engine screen and the new Song screen
adb shell am start -W -S -n "$ACT" --ei tab 1 --es kit project; shot 14_vector_engine
adb shell am start -W -S -n "$ACT" --ei tab 3;                  shot 15_song_projects
# the library: a built-in groove, then an own instrument saved and loaded again
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit lib;     shot 17_library
# song automation: lanes added from controls, and the screen while a control is being picked
adb shell am start -W -S -n "$ACT" --ei tab 3 --es kit autopick; shot 18_song_automation
adb shell am start -W -S -n "$ACT" --ei tab 2 --es kit autopick --ez picking true; shot 19_pick_a_control
# the new drum kits and the drum synth; after a restart the synthesized pad must still be there
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit dsyn;    shot 16_drum_synth; sleep 7
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit dsyncheck; sleep 2
adb shell pidof com.loopdraft.app || { echo "FAIL: app died while switching screens"; fail=1; }

adb logcat -d > "$OUT/logcat.txt"
grep -E "LoopDraft" "$OUT/logcat.txt" | sed 's/^.*LoopDraft *: //' > "$OUT/selftest.txt"
cat "$OUT/selftest.txt"
if grep -E "FATAL EXCEPTION|UnsatisfiedLinkError|Fatal signal" "$OUT/logcat.txt"; then echo "FAIL: crash in log"; fail=1; fi
grep -q "selftest playing=true" "$OUT/selftest.txt" || { echo "FAIL: transport never started (audio thread not running?)"; fail=1; }
steps=$(grep -o "selftest playing=true step=[0-9]*" "$OUT/selftest.txt" | sort -u | wc -l)
[ "$steps" -ge 3 ] || { echo "FAIL: the step counter is not advancing ($steps distinct steps)"; fail=1; }
grep -E "selftest .*sound=true" "$OUT/selftest.txt" >/dev/null || { echo "FAIL: no sound reached the output"; fail=1; }
grep -q "selftest export midi=true wav=true share=true" "$OUT/selftest.txt" || { echo "FAIL: export of MIDI/WAV or the share link did not work"; fail=1; }
grep -q "boot audio=true" "$OUT/selftest.txt" || { echo "FAIL: audio output did not open"; fail=1; }
[ "$(grep -c "boot audio=true bpm=131" "$OUT/selftest.txt")" -ge 4 ] || { echo "FAIL: the saved project was not restored after restarting the app"; fail=1; }
grep -q "starting with the Warehouse 303 preset" "$OUT/logcat.txt" || { echo "FAIL: a fresh install did not open with the preset"; fail=1; }
grep -q "kit bounce ok=true" "$OUT/selftest.txt" || { echo "FAIL: bouncing the loop into the slicer did not work"; fail=1; }
grep -q "kit engine=4 ownBreak=true" "$OUT/selftest.txt" || { echo "FAIL: the bounced loop was gone after restarting the app"; fail=1; }
grep -q "kit back in slicer ownBreak=true" "$OUT/selftest.txt" || { echo "FAIL: the bounced loop was gone after leaving the slicer and restarting"; fail=1; }
grep -q "project reopened mode=3 list=1" "$OUT/selftest.txt" || { echo "FAIL: saving and reopening a named project did not work"; fail=1; }
grep -q "dsyn after restart engine=5 pad0=true" "$OUT/selftest.txt" || { echo "FAIL: the synthesized drum pad was gone after a restart"; fail=1; }
grep -q 'lib items=6 groove=\[Loaded "Dusty House 122"' "$OUT/selftest.txt" || { echo "FAIL: the built-in library did not load"; fail=1; }
grep -q 'own=\[Loaded "CI bass"' "$OUT/selftest.txt" || { echo "FAIL: saving and loading an own instrument did not work"; fail=1; }
grep -q "autopick bass=81,23 drums=62" "$OUT/selftest.txt" || { echo "FAIL: adding automation from a control did not work"; fail=1; }
[ "$fail" = 0 ] && echo "SMOKE TEST PASSED"
exit "$fail"
