package com.loopdraft.app

/** The bridge to the C++ engine. Call everything from the main thread. */
object Native {
    init {
        System.loadLibrary("loopdraft_jni")
    }

    // lifecycle
    external fun start(filesDir: String): Boolean
    external fun stop()
    external fun info(): String
    external fun saveIfDirty()
    external fun poll(): Boolean

    // transport and live play
    external fun play(scene: Int)
    external fun stopTransport()
    external fun queueScene(scene: Int)
    external fun setSongMode(on: Boolean)
    external fun setRecording(on: Boolean)
    external fun triggerPad(pad: Int, vel: Int)
    external fun noteOn(synth: Int, degree: Int, vel: Int)
    external fun noteOff(synth: Int, degree: Int)

    // status
    external fun isPlaying(): Boolean
    external fun currentStep(): Int
    external fun currentScene(): Int
    external fun queuedScene(): Int
    external fun chainPos(): Int
    external fun peak(ch: Int): Float

    // undo
    external fun beginEdit()
    external fun undo(): Boolean
    external fun redo(): Boolean
    external fun canUndo(): Boolean
    external fun canRedo(): Boolean

    // parameters, ids below
    external fun getParam(id: Int, index: Int): Float
    external fun setParam(id: Int, index: Int, value: Float)

    // patterns
    external fun toggleStep(scene: Int, pad: Int, step: Int, accent: Boolean): Int
    external fun drumGrid(scene: Int): ByteArray
    /** slide: adds the note as a slide note, or switches slide on/off on an existing one. */
    external fun toggleNote(scene: Int, synth: Int, step: Int, degree: Int, accent: Boolean, slide: Boolean): Boolean
    external fun notes(scene: Int, synth: Int): IntArray
    external fun degreeMidi(synth: Int, degree: Int): Int

    // scenes and song
    external fun sceneBars(scene: Int): Int
    external fun setSceneBars(scene: Int, bars: Int)
    external fun copyScene(from: Int, to: Int)
    external fun clearScene(scene: Int)
    external fun chain(): IntArray
    external fun chainAdd(scene: Int)
    external fun chainRemove(pos: Int)
    external fun resetToDemo()
    external fun loadStartPreset()      // the built-in Warehouse 303 project; undoable
    external fun setDrumStep(scene: Int, pad: Int, step: Int, value: Int)
    external fun loadJungleKit()
    external fun loadStandardKit()
    external fun setChainAuto(pos: Int, track: Int, lane: Int, paramId: Int, startVal: Float, endVal: Float)
    /** Same without its own undo entry: call beginEdit() once before a drag. */
    external fun setChainAutoLive(pos: Int, track: Int, lane: Int, paramId: Int, startVal: Float, endVal: Float)
    /** Presets and grooves as JSON text. Returns "" when it worked, otherwise the error message. One undo step. */
    external fun importContent(json: String, scene: Int, synth: Int, tempoAndKey: Boolean): String
    external fun checkContent(json: String): String
    /** kind: 0 = instrument of `synth`, 1 = drum kit, 2 = groove of `scene`. */
    external fun exportContent(kind: Int, scene: Int, synth: Int, name: String): String
    external fun saveProjectCopy(path: String): Boolean
    external fun openProject(path: String): Boolean    // one undo step
    external fun chainAuto(pos: Int, track: Int): FloatArray
    external fun setFxSlot(track: Int, slot: Int, type: Int, amount: Float, rate: Float, bypass: Boolean)
    /** Same as setFxSlot but without its own undo entry: call beginEdit() once before a drag. */
    external fun setFxSlotLive(track: Int, slot: Int, type: Int, amount: Float, rate: Float, bypass: Boolean)
    external fun fxSlot(track: Int, slot: Int): FloatArray
    external fun applyInstrument(synth: Int, preset: Int)
    external fun setActiveSynths(count: Int)
    external fun writeBreakPattern(scene: Int)
    external fun padWave(pad: Int, n: Int): FloatArray

    // export: snapshotForExport() on the main thread, then the export calls may run on a worker thread
    external fun snapshotForExport()
    external fun exportMidi(path: String, song: Boolean, scene: Int): Boolean
    external fun exportWav(path: String, song: Boolean, scene: Int, stem: Int): Boolean
    external fun setDrumEngine(engine: Int)
    external fun applyJungleTuning(pitch: Float, tight: Float, crunch: Float)
    external fun loadJungleBreakWav(path: String): Boolean
    external fun resampleSceneToJungle(scene: Int): Boolean
    external fun hasOwnBreak(): Boolean
    external fun setKitHardness(hardness: Float)
    external fun synthesizeCustomPad(pad: Int, audition: Boolean)
    external fun applyDrumSynthTemplate(pad: Int, tmpl: Int)
    external fun clearCustomPad(pad: Int)               // back to the kit's own sound
    external fun padIsSynth(pad: Int): Boolean
}

/** Parameter ids. Must match enum Param in engine/include/loopdraft/session.h. */
object P {
    const val BPM = 0
    const val SWING = 1
    const val MASTER = 2
    const val DELAY_FB = 3
    const val ROOT = 4
    const val SCALE = 5
    const val OCTAVE = 6
    const val KIT = 7            // read-only: 0 = standard, 1 = jungle
    const val ACTIVE_SYNTHS = 8  // read-only: use setActiveSynths()
    const val PAD_VOL = 10
    const val PAD_PAN = 11
    const val PAD_PITCH = 12
    const val PAD_DECAY = 13
    const val PAD_CHOKE = 14
    const val PAD_SLICE_START = 15
    const val PAD_SLICE_END = 16
    const val PAD_REVERSE = 17
    const val SYN_WAVE1 = 20
    const val SYN_WAVE2 = 21
    const val SYN_DETUNE = 22
    const val SYN_CUTOFF = 23
    const val SYN_RES = 24
    const val SYN_ATTACK = 25
    const val SYN_DECAY = 26
    const val SYN_SUSTAIN = 27
    const val SYN_RELEASE = 28
    const val SYN_LEVEL = 29
    const val SYN_LFO_RATE = 30
    const val SYN_LFO_DEPTH = 31
    const val SYN_LFO_TARGET = 32
    const val SYN_OCTAVE = 33
    const val SYN_CHORD = 34
    const val SYN_MODE = 35
    const val SYN_FM_RATIO = 36
    const val SYN_FM_INDEX = 37
    const val SYN_FM_DECAY = 38
    const val SYN_FM_FB = 39
    const val SYN_INSTRUMENT = 50
    const val SYN_WARMTH = 51
    const val SYN_MOTION = 52
    const val SYN_FOLD = 57
    const val SYN_SLIDE_TIME = 58
    const val SYN_PLUCK_BRIGHT = 59
    const val SYN_VEC_X = 60
    const val SYN_VEC_Y = 61
    const val DRUM_ENGINE = 53
    const val JUNGLE_PITCH = 54
    const val JUNGLE_TIGHT = 55
    const val JUNGLE_CRUNCH = 56
    const val KIT_HARDNESS = 62
    const val FX_SLOT1_AMOUNT = 81      // then SLOT1_RATE, SLOT2_AMOUNT, SLOT2_RATE, SLOT3_AMOUNT, SLOT3_RATE
    const val DSYN_FREQ = 70
    const val DSYN_DROP = 71
    const val DSYN_PDEC = 72
    const val DSYN_TONE = 73
    const val DSYN_NOISE = 74
    const val DSYN_NFREQ = 75
    const val DSYN_FM = 76
    const val DSYN_FMRATIO = 77
    const val DSYN_DRIVE = 78
    const val DSYN_CLICK = 79
    const val DSYN_LEN = 80
    const val FX_LEVEL = 40
    const val FX_DRIVE = 41
    const val FX_EQ_LOW = 42
    const val FX_EQ_HIGH = 43
    const val FX_DELAY = 44
    const val FX_REVERB = 45
    const val FX_MUTE = 46
}
