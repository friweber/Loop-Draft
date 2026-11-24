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

# The emulator's own launcher sometimes hangs and puts a dialog over everything. Keep such dialogs off the screenshots.
adb shell settings put global hide_error_dialogs 1 || true
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
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit lib; sleep 3; shot 17_library
# song automation: lanes added from controls, and the screen while a control is being picked
adb shell am start -W -S -n "$ACT" --ei tab 3 --es kit autopick; shot 18_song_automation
adb shell am start -W -S -n "$ACT" --ei tab 2 --es kit autopick --ez picking true; shot 19_pick_a_control
# the new drum kits and the drum synth; after a restart the synthesized pad must still be there
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit dsyn;    shot 16_drum_synth; sleep 7
adb shell am start -W -S -n "$ACT" --ei tab 0 --es kit dsyncheck; sleep 2
# setup dialog, the layers of the drum and synth screens, a large and a compact size, another look, rail on the right
adb shell am start -W -S -n "$ACT" --ei tab 0 --ez setup true;            shot 20_setup
adb shell am start -W -S -n "$ACT" --ei tab 0 --ei layer 1;               shot 21_drums_pads
adb shell am start -W -S -n "$ACT" --ei tab 0 --ei layer 2;               shot 22_drums_sound
adb shell am start -W -S -n "$ACT" --ei tab 1 --ei layer 1;               shot 23_synth_sculpt
adb shell am start -W -S -n "$ACT" --ei tab 1 --ei layer 2;               shot 24_synth_deep
adb shell am start -W -S -n "$ACT" --ei tab 0 --ei uisize 2;              shot 25_large
adb shell am start -W -S -n "$ACT" --ei tab 2 --ei uisize 0 --ei rig 2;   shot 26_compact_cream
adb shell am start -W -S -n "$ACT" --ei tab 1 --ei rig 3 --ei layer 1;    shot 27_cyber_knobs
adb shell cmd window user-rotation lock 1; sleep 3
adb shell am start -W -S -n "$ACT" --ei tab 0 --ez lefthand true; adb shell cmd window user-rotation lock 1; sleep 2; shot 28_land_rail_right
adb shell cmd window user-rotation lock 0; sleep 1
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
grep -Eq 'lib items=1[0-9][0-9] bad=0 groove=\[Loaded "' "$OUT/selftest.txt" || { echo "FAIL: the built-in library did not load"; fail=1; }
grep -q 'own=\[Loaded "CI bass"' "$OUT/selftest.txt" || { echo "FAIL: saving and loading an own instrument did not work"; fail=1; }
grep -q "autopick bass=23,81 drums=62" "$OUT/selftest.txt" || { echo "FAIL: adding automation from a control did not work"; fail=1; }
[ "$fail" = 0 ] && echo "SMOKE TEST PASSED"
exit "$fail"
