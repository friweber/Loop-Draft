package com.loopdraft.app

import androidx.compose.ui.layout.positionInParent
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.unit.Density
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.waitForUpOrCancellation
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

val PAD_NAMES = listOf("Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc")
val PAD_SHORT = listOf("Kik", "Snr", "Clp", "CH", "OH", "Rim", "Tom", "Prc")
val PAD_ROW_COLORS = listOf(
    Color(0xFFE84838), // Kick
    Color(0xFFFF8A24), // Snare
    Color(0xFFFFCA28), // Clap
    Color(0xFF29D3E8), // CH
    Color(0xFF5FA8FF), // OH
    Color(0xFF4BE08A), // Rim
    Color(0xFFC06BFF), // Tom
    Color(0xFFFF5C93)  // Perc
)
val SCENE_NAMES = listOf("A", "B", "C", "D")
val SCENE_LABELS = listOf("A: INTRO", "B: VERSE", "C: CHORUS", "D: BRIDGE")
val TRACK_NAMES = listOf("Drums", "Bass", "Lead", "Syn 3", "Syn 4")
val SYNTH_SHORT = listOf("BS", "LD", "S3", "S4")
val SYNTH_TAG = listOf("B", "L", "3", "4")
val SYNTH_GHOST_COLORS = listOf(
    Color(0xFFFF9526), // Bass = Vivid Amber
    Color(0xFF29D3E8), // Lead = Cyan
    Color(0xFFFF4DA6), // Syn 3 = Hot Magenta
    Color(0xFF5EE86F)  // Syn 4 = Acid Green
)
val INSTRUMENT_NAMES = listOf("Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead")
val FX_SLOT_NAMES = listOf("Off", "Drive", "Crush", "Chorus", "Phaser", "Filter", "Comp")
val DRUM_ENGINE_NAMES = listOf("808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break", "Hardcore Gabber", "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore")
val DSYN_TEMPLATES = listOf("Kick", "Snare", "Hat", "Gabber", "Zap", "Clang")
val NOTE_NAMES = listOf("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
fun noteName(midi: Int): String = NOTE_NAMES[((midi % 12) + 12) % 12] + (midi / 12 - 1)

/** Everything the screen needs to remember. The project itself lives in the C++ engine. */
class AppState {
    var rev by mutableIntStateOf(0)          // bumped after every change, so the screen re-reads the engine
    var tab by mutableIntStateOf(0)
    var scene by mutableIntStateOf(0)        // the scene on screen
    var selPad by mutableIntStateOf(0)
    var synth by mutableIntStateOf(1)        // 0 = bass, 1 = lead
    var fxTrack by mutableIntStateOf(0)
    var drumBar by mutableIntStateOf(0)
    var rollPage by mutableIntStateOf(0)     // half bars
    var rollMode by mutableStateOf(false)
    var accent by mutableStateOf(false)
    var slide by mutableStateOf(false)       // piano roll: taps set or clear a slide

    // Layered navigation & modular skins
    var skin by mutableStateOf(SkinConfig())
    var onSkinChanged: (SkinConfig) -> Unit = {}
    fun updateSkin(cfg: SkinConfig) { skin = cfg; onSkinChanged(cfg) }
    var showSkinStudio by mutableStateOf(false)
    var prefs by mutableStateOf(UiPrefs())
    var onPrefsChanged: (UiPrefs) -> Unit = {}
    fun updatePrefs(p: UiPrefs) { prefs = p; onPrefsChanged(p) }
    var drumLayer by mutableIntStateOf(0)    // 0 = GRID, 1 = PADS, 2 = SOUND
    var synthLayer by mutableIntStateOf(0)   // 0 = PLAY, 1 = SCULPT, 2 = DEEP

    // Piano roll ghost note overlay state (high-alpha default)
    var ghostDrums by mutableStateOf(true)
    var ghostSynthMask by mutableIntStateOf(0b1111)
    var ghostAlpha by mutableFloatStateOf(0.60f)

    // The library: presets and grooves that ship with the app, plus the user's own.
    val library = mutableStateListOf<LibItem>()
    var libOpen by mutableStateOf("")        // which kind is unfolded: "instrument", "drumkit", "groove" or none
    var libName by mutableStateOf("My preset")
    var libMsg by mutableStateOf("")
    var libStyle by mutableStateOf("")       // show only this style in the open library; empty = all
    var libTempoKey by mutableStateOf(true)  // a groove also sets tempo and key
    var onLibLoad: (LibItem) -> Unit = {}
    var onLibSave: (String, String) -> Unit = { _, _ -> }
    var onLibDelete: (LibItem) -> Unit = {}
    var onLibImport: () -> Unit = {}
    val projects = mutableStateListOf<String>()
    var projectName by mutableStateOf("My loop")
    var projectMsg by mutableStateOf("")
    var confirmDelete by mutableStateOf("")
    var onSaveProject: (String) -> Unit = {}
    var onOpenProject: (String) -> Unit = {}
    var onDeleteProject: (String) -> Unit = {}
    var rec by mutableStateOf(false)
    var songMode by mutableStateOf(false)
    var playing by mutableStateOf(false)
    var step by mutableIntStateOf(-1)
    var queued by mutableIntStateOf(-1)
    var chainPos by mutableIntStateOf(0)
    var peakL by mutableFloatStateOf(0f)
    var peakR by mutableFloatStateOf(0f)
    var audioOk by mutableStateOf(false)
    var info by mutableStateOf("")
    // Independent per-synth input octave (-2 .. +2). Shifts only played/edited notes without altering other scenes.
    val inputOctave = mutableStateListOf(0, 0, 0, 0)
    var selAutoTrack by mutableIntStateOf(2)
    var selAutoLane by mutableIntStateOf(0)
    var onFirstFrame: () -> Unit = {}
    var pickAuto by mutableStateOf(false)    // "tap a control": the next tap on a knob adds it to the song automation
    var autoMsg by mutableStateOf("")

    /** Adds one automation lane to the selected song slot: from where the control stands now to the far end of its range. */
    fun addAutomation(id: Int, index: Int) {
        val info = AUTO_PARAMS[id] ?: return
        val track = autoTrackOf(id, index)
        val pos = selChainPos.coerceIn(0, (Native.chain().size - 1).coerceAtLeast(0))
        val data = Native.chainAuto(pos, track)
        val ids = (0 until 3).map { data.getOrElse(it * 3) { -1f }.roundToInt() }
        pickAuto = false
        tab = 3
        selAutoTrack = track
        val have = ids.indexOf(id)
        if (have >= 0) { selAutoLane = have; autoMsg = "${info.label} is already automated here."; return }
        val lane = ids.indexOfFirst { it < 0 }
        if (lane < 0) { autoMsg = "All 3 lanes of ${TRACK_NAMES[track]} are in use in this slot. Clear one first."; return }
        val now = Native.getParam(id, index).coerceIn(info.lo, info.hi)
        val end = if (now - info.lo < (info.hi - info.lo) * 0.5f) info.hi else info.lo
        Native.setChainAuto(pos, track, lane, id, now, end)
        selAutoLane = lane
        autoMsg = "${info.label} added on ${TRACK_NAMES[track]}. Set where it starts and ends."
        rev++
    }
    var drumRollMode by mutableIntStateOf(0) // 0=normal, 3=2x, 4=3x, 5=4x ratchet roll
    var selChainPos by mutableIntStateOf(0)
    var wide by mutableStateOf(false)        // landscape: side rail and two panes
    var exporting by mutableStateOf(false)
    var exportMsg by mutableStateOf("")
    /** Set by the activity: 0 = MIDI, 1 = WAV mix, 2 = WAV stems. */
    var onExport: (Int) -> Unit = {}
    var drumMsg by mutableStateOf("")
    var showDrumSynth by mutableStateOf(false)
    var onLoadBreak: () -> Unit = {}
    private var frames = 0

    /** Runs once per screen frame. */
    fun frame() {
        if (Native.poll()) rev++
        playing = Native.isPlaying()
        step = Native.currentStep()
        queued = Native.queuedScene()
        chainPos = Native.chainPos()
        val sc = Native.currentScene()
        if (playing && sc != scene) scene = sc
        peakL = Native.peak(0)
        peakR = Native.peak(1)
        frames++
        if (frames % 180 == 0) Native.saveIfDirty()
        if (frames % 120 == 0) info = Native.info()
    }

    fun vel() = if (accent) 2 else 1

    fun scaleLen(): Int {
        val sc = Native.getParam(P.SCALE, 0).roundToInt()
        return if (sc == 3) 5 else 7 // Pentatonic = 5 notes/oct, others = 7
    }

    fun degreeOffset(synthIdx: Int): Int = inputOctave[synthIdx.coerceIn(0, 3)] * scaleLen()
}

@Composable
fun App(s: AppState) {
    LaunchedEffect(Unit) {
        withFrameNanos { }
        s.onFirstFrame()
        while (true) {
            withFrameNanos { }
            s.frame()
        }
    }
    val skin = remember(s.skin) { resolveSkin(s.skin) }
    val colorScheme = if (skin.isLight) {
        lightColorScheme(
            primary = skin.primary,
            background = skin.chassis,
            surface = skin.faceplate,
            onSurface = skin.silk,
            onBackground = skin.silk
        )
    } else {
        darkColorScheme(
            primary = skin.primary,
            background = skin.chassis,
            surface = skin.faceplate,
            onSurface = skin.silk,
            onBackground = skin.silk
        )
    }

    // SIZE scales everything at once, text included: the whole layout is measured in a coarser or finer unit.
    val baseDensity = LocalDensity.current
    val density = remember(baseDensity, s.prefs.scale) { Density(baseDensity.density * s.prefs.scaleFactor, baseDensity.fontScale) }
    CompositionLocalProvider(LocalSkin provides skin, LocalUiPrefs provides s.prefs, LocalDensity provides density) {
        MaterialTheme(colorScheme = colorScheme) {
            BoxWithConstraints(Modifier.fillMaxSize().background(skin.chassis).safeDrawingPadding()) {
                val wide = maxWidth > maxHeight && maxWidth >= 520.dp
                if (s.wide != wide) s.wide = wide
                val frame = if (wide) RoundedCornerShape(8.dp) else RoundedCornerShape(bottomStart = 8.dp, bottomEnd = 8.dp)
                val content: @Composable (Modifier) -> Unit = { m ->
                    Column(
                        m.border(2.dp, if (s.pickAuto) HwYellow else sceneColor(s.scene, skin), frame)
                            .padding(horizontal = 8.dp, vertical = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        if (s.pickAuto) {
                            Row(
                                Modifier.fillMaxWidth().clip(RoundedCornerShape(7.dp)).background(HwYellow).padding(horizontal = 10.dp, vertical = 5.dp),
                                horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically
                            ) {
                                SilkLabel("TAP A YELLOW CONTROL TO AUTOMATE IT IN SLOT #${s.selChainPos + 1}", color = HwPadInk, size = 10, modifier = Modifier.weight(1f))
                                HwButton("CANCEL", height = 26.dp, pad = 8.dp) { s.pickAuto = false; s.tab = 3 }
                            }
                        }
                        Box(Modifier.weight(1f).fillMaxWidth()) {
                            when (s.tab) {
                                0 -> DrumsTab(s)
                                1 -> SynthTab(s)
                                2 -> MixTab(s)
                                else -> SongTab(s)
                            }
                        }
                    }
                }
                Row(Modifier.fillMaxSize()) {
                    WalnutRail()
                    if (wide) {
                        // the transport rail sits on the side of the hand that is free
                        if (!s.prefs.leftHanded) SideRail(s)
                        content(Modifier.weight(1f).fillMaxHeight().padding(top = 6.dp, bottom = 6.dp, start = if (s.prefs.leftHanded) 6.dp else 0.dp, end = if (s.prefs.leftHanded) 0.dp else 6.dp))
                        if (s.prefs.leftHanded) SideRail(s)
                    } else {
                        Column(Modifier.weight(1f).fillMaxHeight()) {
                            TopBar(s)
                            SceneStrip(s)
                            content(Modifier.weight(1f).fillMaxWidth().padding(horizontal = 6.dp))
                            BottomNav(s)
                        }
                    }
                    WalnutRail()
                }
                if (s.showSkinStudio) {
                    SkinStudioModal(s) { s.showSkinStudio = false }
                }
            }
        }
    }
}

/** A control the song can move over time: its name and the range its Start and End controls cover. */
data class AutoInfo(val label: String, val lo: Float, val hi: Float)

val AUTO_PARAMS: Map<Int, AutoInfo> = mapOf(
    P.FX_LEVEL to AutoInfo("Level", 0f, 1f), P.FX_DRIVE to AutoInfo("Drive", 0f, 1f),
    P.FX_EQ_LOW to AutoInfo("Low EQ", -12f, 12f), P.FX_EQ_HIGH to AutoInfo("High EQ", -12f, 12f),
    P.FX_DELAY to AutoInfo("Delay send", 0f, 0.8f), P.FX_REVERB to AutoInfo("Reverb send", 0f, 0.8f),
    P.FX_SLOT1_AMOUNT to AutoInfo("FX1 amount", 0f, 1f), P.FX_SLOT1_AMOUNT + 1 to AutoInfo("FX1 rate", 0f, 1f),
    P.FX_SLOT1_AMOUNT + 2 to AutoInfo("FX2 amount", 0f, 1f), P.FX_SLOT1_AMOUNT + 3 to AutoInfo("FX2 rate", 0f, 1f),
    P.FX_SLOT1_AMOUNT + 4 to AutoInfo("FX3 amount", 0f, 1f), P.FX_SLOT1_AMOUNT + 5 to AutoInfo("FX3 rate", 0f, 1f),
    P.KIT_HARDNESS to AutoInfo("Hardness", 0f, 1f),
    P.SYN_CUTOFF to AutoInfo("Cutoff", 80f, 8000f), P.SYN_RES to AutoInfo("Resonance", 0.5f, 15f),
    P.SYN_DETUNE to AutoInfo("Detune", -30f, 30f),
    P.SYN_ATTACK to AutoInfo("Attack", 0.001f, 1f), P.SYN_DECAY to AutoInfo("Decay", 0.02f, 3f),
    P.SYN_SUSTAIN to AutoInfo("Sustain", 0f, 1f), P.SYN_RELEASE to AutoInfo("Release", 0.01f, 3f),
    P.SYN_LEVEL to AutoInfo("Synth level", 0f, 1f),
    P.SYN_LFO_RATE to AutoInfo("LFO rate", 0.05f, 20f), P.SYN_LFO_DEPTH to AutoInfo("LFO depth", 0f, 1f),
    P.SYN_FM_RATIO to AutoInfo("FM ratio", 0.5f, 8f), P.SYN_FM_INDEX to AutoInfo("FM index", 0f, 8f),
    P.SYN_FM_DECAY to AutoInfo("FM decay", 0.01f, 3f), P.SYN_FM_FB to AutoInfo("FM feedback", 0f, 1f),
    P.SYN_WARMTH to AutoInfo("Warmth", 0f, 1f), P.SYN_MOTION to AutoInfo("Motion", 0f, 1f),
    P.SYN_FOLD to AutoInfo("Fold", 0f, 1f), P.SYN_VEC_X to AutoInfo("Vector X", 0f, 1f), P.SYN_VEC_Y to AutoInfo("Vector Y", 0f, 1f),
    P.SYN_PLUCK_BRIGHT to AutoInfo("Pluck bright", 0f, 1f)
)
fun autoIsSynth(id: Int) = id in 20..39 || id in 50..61
fun autoTrackOf(id: Int, index: Int) = if (id == P.KIT_HARDNESS) 0 else if (autoIsSynth(id)) index + 1 else index
fun autoIndexOf(id: Int, track: Int) = if (id == P.KIT_HARDNESS) 0 else if (autoIsSynth(id)) track - 1 else track
val AUTO_QUICK_DRUMS = listOf(P.KIT_HARDNESS, P.FX_LEVEL, P.FX_REVERB, P.FX_DELAY, P.FX_EQ_HIGH)
val AUTO_QUICK_SYNTH = listOf(P.SYN_CUTOFF, P.SYN_RES, P.SYN_FOLD, P.FX_DRIVE, P.FX_REVERB, P.FX_DELAY, P.FX_LEVEL)

/** One preset or groove in the library. `user` ones are files the user saved or imported and can delete. */
data class LibItem(val kind: String, val name: String, val style: String, val user: Boolean, val path: String)

@Composable
fun LibraryPanel(s: AppState, kind: String, title: String, what: String, accent: Color) {
    val skin = LocalSkin.current
    val items = s.library.filter { it.kind == kind }
    if (s.libOpen != kind) {
        HwButton("$title (${items.size})", Modifier.fillMaxWidth(), ledColor = accent, height = 31.dp) {
            s.libOpen = kind; s.libMsg = ""; s.confirmDelete = ""; s.libStyle = ""
        }
        return
    }
    ModulePanel(
        title = title,
        accent = accent,
        trailing = { HwButton("CLOSE", ledColor = skin.primary, height = 26.dp, pad = 8.dp) { s.libOpen = "" } }
    ) {
        SilkLabel(what, color = skin.muted, size = 9, bold = false)
        if (kind == "groove") {
            HwButton(if (s.libTempoKey) "ALSO TAKES TEMPO AND KEY: ON" else "ALSO TAKES TEMPO AND KEY: OFF", Modifier.fillMaxWidth(), on = s.libTempoKey, ledColor = skin.tertiary, showLed = true, height = 28.dp) {
                s.libTempoKey = !s.libTempoKey
            }
        }
        val styles = items.map { if (it.user) "yours" else it.style }.filter { it.isNotEmpty() }.distinct()
        if (styles.size > 1) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                HwButton("ALL", on = s.libStyle.isEmpty(), ledColor = accent, height = 28.dp, pad = 8.dp) { s.libStyle = "" }
                for (st in styles) HwButton(st.uppercase(), on = s.libStyle == st, ledColor = accent, height = 28.dp, pad = 8.dp) { s.libStyle = st }
            }
        }
        for (item in items.filter { s.libStyle.isEmpty() || (if (it.user) "yours" else it.style) == s.libStyle }) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    SilkLabel(item.name, color = skin.silk, size = 12)
                    if (item.style.isNotEmpty() || item.user) SilkLabel(listOf(item.style, if (item.user) "yours" else "").filter { t -> t.isNotEmpty() }.joinToString(" - "), color = skin.muted, size = 9, bold = false)
                }
                HwButton("LOAD", ledColor = accent, height = 30.dp, pad = 10.dp) { s.onLibLoad(item) }
                if (item.user) {
                    val sure = s.confirmDelete == "lib:" + item.path
                    HwButton(if (sure) "SURE?" else "DEL", on = sure, ledColor = skin.danger, height = 30.dp, pad = 8.dp) {
                        if (sure) { s.confirmDelete = ""; s.onLibDelete(item) } else s.confirmDelete = "lib:" + item.path
                    }
                }
            }
        }
        if (items.isEmpty()) SilkLabel("Nothing here yet.", color = skin.muted, size = 10, bold = false)
        if (s.libMsg.isNotEmpty()) SilkLabel(s.libMsg, color = skin.tertiary, size = 10, bold = false)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            BasicTextField(
                value = s.libName,
                onValueChange = { v -> s.libName = v.take(40).filter { c -> c.isLetterOrDigit() || c in " -_" } },
                singleLine = true,
                textStyle = TextStyle(color = skin.silk, fontSize = 14.sp),
                cursorBrush = SolidColor(accent),
                modifier = Modifier.weight(1f).clip(RoundedCornerShape(6.dp)).background(skin.recess).border(1.dp, skin.line, RoundedCornerShape(6.dp)).padding(horizontal = 10.dp, vertical = 9.dp)
            )
            HwButton("SAVE MINE", ledColor = skin.positive, height = 34.dp, pad = 8.dp) { s.onLibSave(kind, s.libName) }
            HwButton("FILE", ledColor = skin.secondary, height = 34.dp, pad = 8.dp) { s.onLibImport() }
        }
        SilkLabel("SAVE MINE: keeps the current one under that name.", color = skin.muted, size = 9, bold = false)
        SilkLabel("FILE: adds a .json preset or groove from the phone.", color = skin.muted, size = 9, bold = false)
    }
}

@Composable
fun Panes(s: AppState, left: @Composable () -> Unit, right: @Composable () -> Unit) {
    if (s.wide) {
        Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) { left() }
            Column(Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) { right() }
        }
    } else {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            left()
            right()
        }
    }
}

// ------------------------------------------------------------------ helpers

fun sceneColor(i: Int, skin: SkinTheme? = null): Color = when (i) {
    0 -> skin?.primary ?: HwOrange
    1 -> skin?.secondary ?: HwCyan
    2 -> skin?.tertiary ?: HwYellow
    else -> skin?.positive ?: HwGreen
}

@Composable
fun Choice(s: AppState, label: String, id: Int, index: Int, options: List<String>, offset: Int = 0, accent: Color? = null) {
    val skin = LocalSkin.current
    val acc = accent ?: skin.primary
    val cur = remember(s.rev, id, index) { Native.getParam(id, index).roundToInt() }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        SilkLabel(label.uppercase(), size = 9, modifier = Modifier.width(56.dp))
        val scroll = rememberScrollState()
        val xs = remember(options.size) { IntArray(options.size) }
        LaunchedEffect(cur, options.size) {
            withFrameNanos { }                      // positions are known after the first layout
            val x = xs.getOrNull(cur - offset) ?: return@LaunchedEffect
            if (x < scroll.value || x > scroll.value + scroll.viewportSize * 2 / 3) scroll.animateScrollTo((x - scroll.viewportSize / 4).coerceAtLeast(0))
        }
        Row(Modifier.weight(1f).horizontalScroll(scroll), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            options.forEachIndexed { i, name ->
                HwButton(name, Modifier.onGloballyPositioned { c -> xs[i] = c.positionInParent().x.roundToInt() }, on = cur == i + offset, ledColor = acc, height = 30.dp, pad = 7.dp) {
                    Native.beginEdit(); Native.setParam(id, index, (i + offset).toFloat()); s.rev++
                }
            }
        }
    }
}

/**
 * The Vector engine's X/Y pad: one finger mixes the four sources in the corners.
 */
@Composable
fun VectorPad(s: AppState, synth: Int, corners: List<String>) {
    val skin = LocalSkin.current
    val px = remember { mutableFloatStateOf(0.5f) }
    val py = remember { mutableFloatStateOf(0.5f) }
    var dragging by remember { mutableStateOf(false) }
    val x0 = remember(s.rev, synth) { Native.getParam(P.SYN_VEC_X, synth) }
    val y0 = remember(s.rev, synth) { Native.getParam(P.SYN_VEC_Y, synth) }
    LaunchedEffect(x0, y0, synth) { if (!dragging) { px.floatValue = x0; py.floatValue = y0 } }
    val shape = RoundedCornerShape(8.dp)
    Box(
        Modifier.fillMaxWidth().height(164.dp).clip(shape).background(skin.scopeBg)
            .border(1.dp, if (dragging) skin.scopeTrace else skin.line, shape)
            .pointerInput(synth) {
                awaitEachGesture {
                    val down = awaitFirstDown(requireUnconsumed = false)
                    val put: (Offset) -> Unit = { o ->
                        val nx = (o.x / size.width).coerceIn(0f, 1f)
                        val ny = (o.y / size.height).coerceIn(0f, 1f)
                        px.floatValue = nx; py.floatValue = ny
                        Native.setParam(P.SYN_VEC_X, synth, nx); Native.setParam(P.SYN_VEC_Y, synth, ny)
                    }
                    dragging = true
                    Native.beginEdit()
                    put(down.position)
                    down.consume()
                    while (true) {
                        val ch = awaitPointerEvent().changes.firstOrNull { it.id == down.id } ?: break
                        if (!ch.pressed) break
                        put(ch.position)
                        ch.consume()
                    }
                    dragging = false
                    s.rev++
                }
            }
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val grid = skin.scopeTrace.copy(alpha = 0.14f)
            for (i in 1..3) {
                drawLine(grid, Offset(size.width * i / 4f, 0f), Offset(size.width * i / 4f, size.height), 1.dp.toPx())
                drawLine(grid, Offset(0f, size.height * i / 4f), Offset(size.width, size.height * i / 4f), 1.dp.toPx())
            }
            val c = Offset(px.floatValue * size.width, py.floatValue * size.height)
            val beam = skin.scopeTrace.copy(alpha = 0.55f)
            drawLine(beam, Offset(c.x, 0f), Offset(c.x, size.height), 1.dp.toPx())
            drawLine(beam, Offset(0f, c.y), Offset(size.width, c.y), 1.dp.toPx())
            drawCircle(skin.scopeTrace.copy(alpha = 0.25f), 16.dp.toPx(), c)
            drawCircle(skin.scopeTrace, 7.dp.toPx(), c)
            drawCircle(Color.White, 7.dp.toPx(), c, style = Stroke(1.5.dp.toPx()))
        }
        val al = listOf(Alignment.TopStart, Alignment.TopEnd, Alignment.BottomStart, Alignment.BottomEnd)
        for (i in 0 until 4) SilkLabel(corners[i], color = skin.scopeTrace.copy(alpha = 0.85f), size = 9, modifier = Modifier.align(al[i]).padding(6.dp))
    }
}

fun pct(v: Float) = "${(v * 100).roundToInt()}%"
fun signed(v: Float, unit: String) = (if (v >= 0) "+" else "") + v.roundToInt() + unit
fun secs(v: Float) = "${(v * 100).roundToInt() / 100f}s"

// ------------------------------------------------------------------ top hardware transport & nav

@Composable
fun TopBar(s: AppState) {
    val skin = LocalSkin.current
    val canUndo = remember(s.rev) { Native.canUndo() }
    val canRedo = remember(s.rev) { Native.canRedo() }
    val bpm = remember(s.rev) { Native.getParam(P.BPM, 0).roundToInt() }
    fun setBpm(v: Int) { Native.beginEdit(); Native.setParam(P.BPM, 0, v.coerceIn(40, 240).toFloat()); s.rev++ }

    Column(Modifier.fillMaxWidth().background(skin.faceplate).padding(horizontal = 8.dp, vertical = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            Column(Modifier.weight(1f)) {
                Logo()
                if (!s.audioOk) SilkLabel("NO AUDIO", color = skin.danger, size = 9)
            }
            HwButton("SETUP", on = s.showSkinStudio, ledColor = skin.tertiary, height = 28.dp, pad = 7.dp) {
                s.showSkinStudio = true
            }
            HwButton("UNDO", enabled = canUndo, height = 28.dp, pad = 6.dp) { if (Native.undo()) s.rev++ }
            HwButton("REDO", enabled = canRedo, height = 28.dp, pad = 6.dp) { if (Native.redo()) s.rev++ }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            HwButton(if (s.playing) "STOP" else "PLAY", Modifier.weight(1.1f), on = s.playing, ledColor = skin.positive, showLed = true, height = 38.dp) {
                if (s.playing) Native.stopTransport() else Native.play(s.scene)
            }
            HwButton("REC", on = s.rec, ledColor = skin.danger, showLed = true, height = 38.dp, pad = 8.dp) {
                s.rec = !s.rec; Native.setRecording(s.rec)
            }
            HwButton("-", height = 38.dp, pad = 10.dp) { setBpm(bpm - 1) }
            SevenSegmentBpm(bpm)
            HwButton("+", height = 38.dp, pad = 10.dp) { setBpm(bpm + 1) }
        }
    }
}

@Composable
fun SceneTab(s: AppState, i: Int, modifier: Modifier, shape: RoundedCornerShape) {
    val skin = LocalSkin.current
    val bars = remember(s.rev, i) { Native.sceneBars(i) }
    val active = s.scene == i
    val queued = s.queued == i
    val col = if (queued) skin.tertiary else sceneColor(i, skin)
    Box(
        modifier
            .height(34.dp)
            .clip(shape)
            .background(if (active || queued) col else skin.buttonOff)
            .border(1.dp, if (active || queued) col else skin.line, shape)
            .clickable {
                if (!(s.playing && s.songMode)) {
                    Native.queueScene(i)
                    if (!s.playing) { s.scene = i; s.drumBar = 0; s.rollPage = 0 }
                }
            },
        contentAlignment = Alignment.Center
    ) {
        SilkLabel(
            text = if (bars > 1) "${SCENE_NAMES[i]}  ${bars}B" else SCENE_NAMES[i],
            color = if (active || queued) skin.onAccentInk else skin.silk,
            size = 11
        )
    }
}

@Composable
fun SceneStrip(s: AppState) {
    val skin = LocalSkin.current
    val tab = RoundedCornerShape(topStart = 9.dp, topEnd = 9.dp)
    Row(
        Modifier.fillMaxWidth().background(skin.chassis).padding(start = 6.dp, end = 6.dp, top = 5.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        for (i in 0 until 4) SceneTab(s, i, Modifier.weight(1f), tab)
    }
}

@Composable
fun NavButton(s: AppState, i: Int, modifier: Modifier) {
    val skin = LocalSkin.current
    val label = listOf("DRUMS", "SYNTH", "FX", "SONG")[i]
    val col = listOf(skin.primary, skin.secondary, skin.tertiary, skin.positive)[i]
    HwButton(text = label, modifier = modifier, on = s.tab == i, ledColor = col, showLed = true, height = 40.dp, pad = 4.dp) { s.tab = i }
}

/** Landscape: logo, transport, tempo, undo, skin studio, scenes and screen buttons in one narrow column. */
@Composable
fun SideRail(s: AppState) {
    val skin = LocalSkin.current
    val canUndo = remember(s.rev) { Native.canUndo() }
    val canRedo = remember(s.rev) { Native.canRedo() }
    val bpm = remember(s.rev) { Native.getParam(P.BPM, 0).roundToInt() }
    fun setBpm(v: Int) { Native.beginEdit(); Native.setParam(P.BPM, 0, v.coerceIn(40, 240).toFloat()); s.rev++ }
    val gap = Arrangement.spacedBy(5.dp)
    val all = RoundedCornerShape(9.dp)
    Column(
        Modifier.width(210.dp).fillMaxHeight().background(skin.faceplate).verticalScroll(rememberScrollState()).padding(6.dp),
        verticalArrangement = gap
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween, modifier = Modifier.fillMaxWidth()) {
            Logo()
            HwButton("SETUP", on = s.showSkinStudio, ledColor = skin.tertiary, height = 26.dp, pad = 7.dp) { s.showSkinStudio = true }
        }
        if (!s.audioOk) SilkLabel("NO AUDIO", color = skin.danger, size = 9)
        Row(horizontalArrangement = gap) {
            HwButton(if (s.playing) "STOP" else "PLAY", Modifier.weight(1f), on = s.playing, ledColor = skin.positive, showLed = true, height = 38.dp) {
                if (s.playing) Native.stopTransport() else Native.play(s.scene)
            }
            HwButton("REC", on = s.rec, ledColor = skin.danger, showLed = true, height = 38.dp, pad = 8.dp) {
                s.rec = !s.rec; Native.setRecording(s.rec)
            }
        }
        Row(horizontalArrangement = gap, verticalAlignment = Alignment.CenterVertically) {
            HwButton("-", Modifier.weight(1f), height = 36.dp) { setBpm(bpm - 1) }
            SevenSegmentBpm(bpm)
            HwButton("+", Modifier.weight(1f), height = 36.dp) { setBpm(bpm + 1) }
        }
        Row(horizontalArrangement = gap) {
            HwButton("UNDO", Modifier.weight(1f), enabled = canUndo, height = 30.dp, pad = 4.dp) { if (Native.undo()) s.rev++ }
            HwButton("REDO", Modifier.weight(1f), enabled = canRedo, height = 30.dp, pad = 4.dp) { if (Native.redo()) s.rev++ }
        }
        for (row in 0 until 2) Row(horizontalArrangement = gap) {
            for (col in 0 until 2) SceneTab(s, row * 2 + col, Modifier.weight(1f), all)
        }
        for (row in 0 until 2) Row(horizontalArrangement = gap) {
            for (col in 0 until 2) NavButton(s, row * 2 + col, Modifier.weight(1f))
        }
    }
}

@Composable
fun BottomNav(s: AppState) {
    val skin = LocalSkin.current
    Row(
        Modifier
            .fillMaxWidth()
            .background(skin.faceplate)
            .border(1.dp, skin.line)
            .padding(horizontal = 6.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        for (i in 0 until 4) NavButton(s, i, Modifier.weight(1f))
    }
}

// ------------------------------------------------------------------ DRUMS TAB (8x16 GRID ASSEMBLER + LAYERED DEPTH)

@Composable
fun DrumPad(s: AppState, i: Int, label: String, modifier: Modifier) {
    val skin = LocalSkin.current
    val vel by rememberUpdatedState(s.vel())
    var hit by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(if (skin.config.button == ButtonSkin.ALU_SWITCH) 4.dp else 9.dp)
    val selected = s.selPad == i
    val padColor = PAD_ROW_COLORS[i % PAD_ROW_COLORS.size]
    Box(
        modifier
            .height(62.dp)
            .clip(shape)
            .background(if (hit) skin.padActive else skin.padSurface)
            .border(if (selected) 2.dp else 1.2.dp, if (hit) skin.positive else if (selected) padColor else skin.line, shape)
            .pointerInput(i) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)
                    Native.triggerPad(i, vel)
                    s.selPad = i
                    hit = true
                    waitForUpOrCancellation()
                    hit = false
                }
            }
            .padding(6.dp)
    ) {
        Box(
            Modifier
                .size(7.dp)
                .clip(CircleShape)
                .background(if (hit) skin.positive else if (selected) padColor else Color(0xFF6B4A4A))
                .align(Alignment.TopStart)
        )
        SilkLabel(label.uppercase(), color = skin.padInk, size = 11, modifier = Modifier.align(Alignment.BottomCenter))
    }
}

@Composable
fun BarPicker(bars: Int, current: Int, playingBar: Int, onPick: (Int) -> Unit) {
    val skin = LocalSkin.current
    if (bars <= 1) return
    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        for (b in 0 until bars) {
            HwButton(
                text = "B${b + 1}",
                modifier = Modifier.weight(1f),
                on = b == current,
                ledColor = if (b == playingBar) skin.positive else skin.primary,
                height = 28.dp,
                pad = 1.dp
            ) { onPick(b) }
        }
    }
}

/**
 * Full 8x16 Drum Grid Assembler: all 8 drum tracks x 16 steps visible simultaneously.
 * Tap a row label to audition & select that track; tap any cell to toggle hit / accent / ratchet roll.
 */
@Composable
fun DrumGridAssembler(
    s: AppState,
    grid: ByteArray,
    bars: Int,
    bar: Int,
    jungle: Boolean,
    accent: Color
) {
    val skin = LocalSkin.current
    val rollLabel = when (s.drumRollMode) { 3 -> "2X"; 4 -> "3X"; 5 -> "4X"; else -> "ROLL" }
    ModulePanel(
        title = "Drum Grid 8x16 • Bar ${bar + 1}/$bars",
        accent = accent,
        trailing = {
            HwButton(rollLabel, on = s.drumRollMode > 0, ledColor = skin.secondary, height = 26.dp, pad = 6.dp) {
                s.drumRollMode = when (s.drumRollMode) { 0 -> 3; 3 -> 4; 4 -> 5; else -> 0 }
            }
            HwButton("ACC", on = s.accent, ledColor = skin.tertiary, height = 26.dp, pad = 6.dp) { s.accent = !s.accent }
        }
    ) {
        BarPicker(bars, bar, if (s.playing) s.step / 16 else -1) { s.drumBar = it }

        // Beat ruler header (1..16)
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("TRK", color = skin.muted, size = 8, modifier = Modifier.width(38.dp))
            for (stInBar in 0 until 16) {
                val st = bar * 16 + stInBar
                val isHead = s.playing && s.step == st
                val isBeat = stInBar % 4 == 0
                if (stInBar > 0 && isBeat) Spacer(Modifier.width(3.dp))
                Box(Modifier.weight(1f), contentAlignment = Alignment.Center) {
                    if (isHead) {
                        Box(Modifier.size(6.dp).clip(CircleShape).background(skin.positive))
                    } else if (isBeat) {
                        SilkLabel("${stInBar / 4 + 1}", color = skin.silk, size = 8, mono = true)
                    } else {
                        Box(Modifier.size(2.5.dp).clip(CircleShape).background(skin.muted.copy(alpha = 0.4f)))
                    }
                }
            }
        }

        // 8 track rows x 16 steps
        Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
            for (pad in 0 until 8) {
                val rowCol = PAD_ROW_COLORS[pad]
                val selected = s.selPad == pad
                val label = if (jungle) "S${pad + 1}" else PAD_SHORT[pad]
                Row(
                    Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(5.dp))
                        .background(if (selected) rowCol.copy(alpha = 0.12f) else Color.Transparent)
                        .padding(vertical = 1.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    // Track label button: tap to audition & select
                    Row(
                        Modifier
                            .width(38.dp)
                            .height(26.dp)
                            .padding(end = 3.dp)
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (selected) rowCol else skin.buttonOff)
                            .border(1.dp, if (selected) Color.White.copy(alpha = 0.7f) else rowCol.copy(alpha = 0.45f), RoundedCornerShape(4.dp))
                            .clickable {
                                s.selPad = pad
                                Native.triggerPad(pad, s.vel())
                            }
                            .padding(horizontal = 3.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.Center
                    ) {
                        SilkLabel(label.uppercase(), color = if (selected) skin.onAccentInk else skin.silk, size = 8, mono = true)
                    }

                    // 16 step cells
                    for (stInBar in 0 until 16) {
                        val st = bar * 16 + stInBar
                        val isBeat = stInBar % 4 == 0
                        if (stInBar > 0 && isBeat) Spacer(Modifier.width(3.dp))
                        val curV = grid[pad * 128 + st].toInt()
                        val isHead = s.playing && s.step == st
                        val beatGroupOdd = ((stInBar / 4) % 2) == 1
                        val emptyBg = if (beatGroupOdd) skin.recess else skin.recess.copy(alpha = 0.62f)
                        val cellBg = when {
                            curV >= 3 -> skin.secondary
                            curV == 2 -> rowCol
                            curV == 1 -> rowCol.copy(alpha = 0.74f)
                            else -> emptyBg
                        }
                        val cellShape = RoundedCornerShape(3.dp)
                        Box(
                            Modifier
                                .weight(1f)
                                .height(26.dp)
                                .padding(horizontal = 0.7.dp)
                                .clip(cellShape)
                                .background(cellBg)
                                .border(
                                    width = if (isHead) 1.5.dp else 0.7.dp,
                                    color = when {
                                        isHead -> skin.positive
                                        curV == 2 -> Color.White.copy(alpha = 0.85f)
                                        curV > 0 -> rowCol
                                        else -> skin.line.copy(alpha = 0.65f)
                                    },
                                    shape = cellShape
                                )
                                .clickable {
                                    s.selPad = pad
                                    if (s.drumRollMode > 0) {
                                        Native.setDrumStep(s.scene, pad, st, if (curV == s.drumRollMode) 0 else s.drumRollMode)
                                    } else {
                                        Native.toggleStep(s.scene, pad, st, s.accent)
                                    }
                                    s.rev++
                                },
                            contentAlignment = Alignment.Center
                        ) {
                            if (curV >= 3) {
                                SilkLabel("${curV - 1}", color = skin.onAccentInk, size = 8, mono = true)
                            } else if (curV == 2) {
                                Box(Modifier.align(Alignment.TopCenter).fillMaxWidth(0.65f).height(2.5.dp).background(Color.White))
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun DrumsTab(s: AppState) {
    val skin = LocalSkin.current
    val grid = remember(s.rev, s.scene) { Native.drumGrid(s.scene) }
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }
    val bar = s.drumBar.coerceIn(0, bars - 1)
    val drumEng = remember(s.rev) { Native.getParam(P.DRUM_ENGINE, 0).roundToInt().coerceIn(0, DRUM_ENGINE_NAMES.size - 1) }
    val padSynth = remember(s.rev, s.selPad) { Native.padIsSynth(s.selPad) }
    val jungle = drumEng == 4
    val ownBreak = remember(s.rev) { Native.hasOwnBreak() }
    val revOn = remember(s.rev, s.selPad) { Native.getParam(P.PAD_REVERSE, s.selPad) > 0.5f }
    val peaks = remember(s.rev, s.selPad) { Native.padWave(s.selPad, 96) }
    val slice0 = remember(s.rev, s.selPad) { Native.getParam(P.PAD_SLICE_START, s.selPad) }
    val slice1 = remember(s.rev, s.selPad) { Native.getParam(P.PAD_SLICE_END, s.selPad) }
    val accent = if (jungle) skin.secondary else skin.primary
    val padLabel: (Int) -> String = { i -> if (jungle) "Slice ${i + 1}" else PAD_NAMES[i] }
    fun bounce() {
        s.drumMsg = if (Native.resampleSceneToJungle(s.scene)) "" else "Nothing to bounce: the scene needs drum steps and must be shorter than 24 s."
        s.rev++
    }
    val rollLabel = when (s.drumRollMode) { 3 -> "ROLL 2X"; 4 -> "ROLL 3X"; 5 -> "ROLL 4X"; else -> "ROLL" }
    // Ensure CI `dsyn` intent opens directly onto Sound/Drum Synth layer
    val effectiveLayer = if (s.showDrumSynth && !jungle) 2 else s.drumLayer

    val kitHeaderAndLibs: @Composable () -> Unit = {
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            Box(Modifier.weight(1f)) {
                LibraryPanel(s, "groove", "GROOVES", "Replaces the patterns of scene ${SCENE_NAMES[s.scene]}, brings its sounds.", skin.positive)
            }
            if (s.libOpen != "groove") {
                Box(Modifier.weight(1f)) {
                    LibraryPanel(s, "drumkit", "DRUM KITS", "Sets kit, pads and drum effects. Patterns stay.", skin.primary)
                }
            }
        }
        if (s.libOpen == "groove") {
            LibraryPanel(s, "drumkit", "DRUM KITS", "Sets kit, pads and drum effects. Patterns stay.", skin.primary)
        }
    }

    val kitSelectorStrip: @Composable () -> Unit = {
        // the row scrolls to the kit in use
        val kitScroll = rememberScrollState()
        val kitXs = remember { IntArray(DRUM_ENGINE_NAMES.size) }
        LaunchedEffect(drumEng) {
            withFrameNanos { }
            val x = kitXs.getOrNull(drumEng) ?: return@LaunchedEffect
            if (x < kitScroll.value || x > kitScroll.value + kitScroll.viewportSize * 2 / 3) kitScroll.animateScrollTo((x - kitScroll.viewportSize / 4).coerceAtLeast(0))
        }
        Row(Modifier.horizontalScroll(kitScroll), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("KIT", size = 9, modifier = Modifier.width(24.dp))
            DRUM_ENGINE_NAMES.forEachIndexed { idx, nm ->
                HwButton(nm, Modifier.onGloballyPositioned { c -> kitXs[idx] = c.positionInParent().x.roundToInt() }, on = drumEng == idx, ledColor = if (idx == 4) skin.secondary else skin.primary, height = 27.dp, pad = 6.dp) {
                    if (drumEng != idx) { Native.setDrumEngine(idx); s.drumMsg = ""; s.rev++ }
                }
            }
        }
    }

    val padsAnd808Section: @Composable () -> Unit = {
        ModulePanel(
            title = "Live Pads & Tone: ${padLabel(s.selPad)}",
            accent = accent
        ) {
            kitSelectorStrip()
            WaveformDisplay(peaks, slice0, slice1, accent, Modifier.fillMaxWidth())
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Vol", P.PAD_VOL, s.selPad, 0f, 1f, Modifier.weight(1f)) { pct(it) }
                RotaryKnob(s, "Pitch", P.PAD_PITCH, s.selPad, -12f, 12f, Modifier.weight(1f)) { signed(it, "st") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Decay", P.PAD_DECAY, s.selPad, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
                RotaryKnob(s, "Pan", P.PAD_PAN, s.selPad, -1f, 1f, Modifier.weight(1f)) {
                    if (it < -0.02f) "L${(it * -100).roundToInt()}" else if (it > 0.02f) "R${(it * 100).roundToInt()}" else "CTR"
                }
            }
            if (!jungle) Choice(s, "Choke", P.PAD_CHOKE, s.selPad, listOf("Off", "Grp 1", "Grp 2"), accent = skin.primary)
            for (row in 0 until 2) Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                for (col in 0 until 4) DrumPad(s, row * 4 + col, if (jungle) "S${row * 4 + col + 1}" else PAD_NAMES[row * 4 + col], Modifier.weight(1f))
            }
        }

        ModulePanel(
            title = "808 Step Strip: ${padLabel(s.selPad)}",
            accent = skin.primary,
            trailing = {
                HwButton(rollLabel, on = s.drumRollMode > 0, ledColor = skin.secondary, height = 26.dp, pad = 6.dp) {
                    s.drumRollMode = when (s.drumRollMode) { 0 -> 3; 3 -> 4; 4 -> 5; else -> 0 }
                }
                HwButton("ACCENT", on = s.accent, ledColor = skin.tertiary, height = 26.dp, pad = 6.dp) { s.accent = !s.accent }
            }
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                PAD_SHORT.forEachIndexed { i, name ->
                    val lbl = if (jungle) "S${i + 1}" else name
                    HwButton(lbl, Modifier.weight(1f), on = s.selPad == i, ledColor = accent, height = 30.dp, pad = 1.dp) { s.selPad = i }
                }
            }
            BarPicker(bars, bar, if (s.playing) s.step / 16 else -1) { s.drumBar = it }
            for (row in 0 until 2) Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (col in 0 until 8) {
                    val stInBar = row * 8 + col
                    val st = bar * 16 + stInBar
                    val curV = grid[s.selPad * 128 + st].toInt()
                    Tr808StepButton(
                        stepInBar = stInBar,
                        vel = curV,
                        playhead = s.playing && s.step == st,
                        modifier = Modifier.weight(1f)
                    ) {
                        if (s.drumRollMode > 0) {
                            Native.setDrumStep(s.scene, s.selPad, st, if (curV == s.drumRollMode) 0 else s.drumRollMode)
                        } else {
                            Native.toggleStep(s.scene, s.selPad, st, s.accent)
                        }
                        s.rev++
                    }
                }
            }
        }
    }

    val soundDesignSection: @Composable () -> Unit = {
        ModulePanel(
            title = if (jungle) "Break Slicer: ${padLabel(s.selPad)}" else "Kit Character & Slicer",
            accent = accent
        ) {
            kitSelectorStrip()
            WaveformDisplay(peaks, slice0, slice1, accent, Modifier.fillMaxWidth())
            if (jungle) {
                Row(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalAlignment = Alignment.CenterVertically) {
                    HwButton("BUILT-IN", Modifier.weight(1f), on = !ownBreak, ledColor = skin.secondary, height = 30.dp, pad = 2.dp) {
                        if (ownBreak) { Native.setDrumEngine(4); s.drumMsg = ""; s.rev++ }
                    }
                    HwButton("LOAD WAV", Modifier.weight(1f), ledColor = skin.secondary, height = 30.dp, pad = 2.dp) { s.onLoadBreak() }
                    HwButton("BOUNCE ${SCENE_NAMES[s.scene]}", Modifier.weight(1f), ledColor = skin.tertiary, height = 30.dp, pad = 2.dp) { bounce() }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Repitch", P.JUNGLE_PITCH, 0, -12f, 12f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { signed(it, "st") }
                    RotaryKnob(s, "Tight", P.JUNGLE_TIGHT, 0, 0.08f, 1.5f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { secs(it) }
                    RotaryKnob(s, "Crunch", P.JUNGLE_CRUNCH, 0, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    RotaryKnob(s, "Slice S", P.PAD_SLICE_START, s.selPad, 0f, 0.95f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                    RotaryKnob(s, "Slice E", P.PAD_SLICE_END, s.selPad, 0.05f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    HwButton(if (revOn) "REVERSE ON" else "REVERSE", Modifier.weight(1f), on = revOn, ledColor = skin.secondary, showLed = true, height = 34.dp) {
                        Native.beginEdit(); Native.setParam(P.PAD_REVERSE, s.selPad, if (revOn) 0f else 1f); s.rev++
                    }
                    HwButton("WRITE BREAK", Modifier.weight(1f), ledColor = skin.secondary, height = 34.dp) {
                        Native.writeBreakPattern(s.scene); s.rev++
                    }
                }
            } else {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    RotaryKnob(s, "Hardness", P.KIT_HARDNESS, 0, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
                    HwButton("BOUNCE LOOP TO SLICER", Modifier.weight(1f), ledColor = skin.secondary, height = 46.dp) { bounce() }
                }
            }
            if (s.drumMsg.isNotEmpty()) SilkLabel(s.drumMsg, color = skin.tertiary, size = 10, bold = false)
        }

        if (!jungle) {
            val pad = s.selPad
            ModulePanel(
                title = "Drum Synth: ${padLabel(pad)}",
                accent = skin.tertiary,
                trailing = {
                    HwButton(if (padSynth) "USE KIT SOUND" else "KIT SOUND", enabled = padSynth, ledColor = skin.primary, height = 26.dp, pad = 8.dp) {
                        Native.clearCustomPad(pad); s.rev++; Native.triggerPad(pad, 2)
                    }
                }
            ) {
                Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                    PAD_SHORT.forEachIndexed { i, name ->
                        HwButton(name, Modifier.weight(1f), on = s.selPad == i, ledColor = skin.tertiary, height = 26.dp, pad = 1.dp) {
                            s.selPad = i; Native.triggerPad(i, 2)
                        }
                    }
                }
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                    SilkLabel("START", size = 9, modifier = Modifier.width(38.dp))
                    DSYN_TEMPLATES.forEachIndexed { idx, nm ->
                        HwButton(nm, ledColor = skin.primary, height = 28.dp, pad = 8.dp) { Native.applyDrumSynthTemplate(pad, idx); s.rev++ }
                    }
                }
                val dk: @Composable (String, Int, Float, Float, Modifier, Color, (Float) -> String) -> Unit = { label, id, lo, hi, m, cap, f ->
                    val v = remember(s.rev, id, pad) { Native.getParam(id, pad) }
                    Knob(label, v, lo, hi, m, capColor = cap, fmt = f,
                        onStart = { Native.beginEdit() },
                        onChange = { Native.setParam(id, pad, it) },
                        onEnd = { s.rev++; Native.triggerPad(pad, 2) })
                }
                val grey = Color(0xFF323640); val blue = Color(0xFF1F3847); val brown = Color(0xFF422A1A)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    dk("Pitch", P.DSYN_FREQ, 25f, 1200f, Modifier.weight(1f), grey) { "${it.roundToInt()}Hz" }
                    dk("Drop", P.DSYN_DROP, 0f, 600f, Modifier.weight(1f), grey) { "${it.roundToInt()}Hz" }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    dk("Drop Time", P.DSYN_PDEC, 0.005f, 0.30f, Modifier.weight(1f), grey) { "${(it * 1000).roundToInt()}ms" }
                    dk("Length", P.DSYN_LEN, 0.05f, 1.5f, Modifier.weight(1f), grey) { secs(it) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    dk("Tone", P.DSYN_TONE, 0f, 1f, Modifier.weight(1f), grey) { pct(it) }
                    dk("Noise", P.DSYN_NOISE, 0f, 1f, Modifier.weight(1f), grey) { pct(it) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    dk("Noise Hz", P.DSYN_NFREQ, 300f, 10000f, Modifier.weight(1f), grey) { "${it.roundToInt()}" }
                    dk("Click", P.DSYN_CLICK, 0f, 1f, Modifier.weight(1f), grey) { pct(it) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    dk("FM", P.DSYN_FM, 0f, 8f, Modifier.weight(1f), blue) { "${(it * 10).roundToInt() / 10f}" }
                    dk("FM Ratio", P.DSYN_FMRATIO, 0.5f, 8f, Modifier.weight(1f), blue) { "${(it * 10).roundToInt() / 10f}x" }
                    dk("Drive", P.DSYN_DRIVE, 0f, 1f, Modifier.weight(1f), brown) { pct(it) }
                }
                SilkLabel(if (padSynth) "This pad plays its own synthesized drum. It stays when you change the kit." else "This pad plays the kit's sound. Pick a start or move a control to synthesize it.", color = skin.muted, size = 9, bold = false)
            }
        }
    }

    if (s.wide) {
        // Landscape: Left pane = Kit/Pads/Sound Design, Right pane = Always-live 8x16 Drum Grid Assembler
        Panes(s, left = {
            kitHeaderAndLibs()
            LayerSwitcher(
                layers = listOf("PADS & TONE", "SOUND & SYNTH"),
                selected = if (effectiveLayer == 2) 1 else 0,
                accent = accent,
                onSelect = { idx ->
                    s.showDrumSynth = (idx == 1)
                    s.drumLayer = if (idx == 1) 2 else 1
                }
            )
            if (effectiveLayer == 2) soundDesignSection() else padsAnd808Section()
        }, right = {
            DrumGridAssembler(s, grid, bars, bar, jungle, accent)
            ModulePanel("Quick Trim: ${padLabel(s.selPad)}", accent = accent) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Vol", P.PAD_VOL, s.selPad, 0f, 1f, Modifier.weight(1f)) { pct(it) }
                    RotaryKnob(s, "Pitch", P.PAD_PITCH, s.selPad, -12f, 12f, Modifier.weight(1f)) { signed(it, "st") }
                    RotaryKnob(s, "Decay", P.PAD_DECAY, s.selPad, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
                    if (!jungle) {
                        RotaryKnob(s, "Hardness", P.KIT_HARDNESS, 0, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
                    }
                }
            }
        })
    } else {
        // Portrait: Clean 3-layer flow (1. GRID -> 2. PADS -> 3. SOUND/SYNTH)
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            LayerSwitcher(
                layers = listOf("1. GRID 8x16", "2. PADS", if (jungle) "3. SLICER" else "3. SYNTH"),
                selected = effectiveLayer,
                accent = accent,
                onSelect = { idx ->
                    s.drumLayer = idx
                    s.showDrumSynth = (idx == 2)
                }
            )
            kitHeaderAndLibs()
            when (effectiveLayer) {
                0 -> {
                    ModulePanel(
                        title = if (jungle) "Kit: Break Slicer" else "Kit: ${DRUM_ENGINE_NAMES[drumEng]}",
                        accent = accent
                    ) {
                        kitSelectorStrip()
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                            if (jungle) {
                                HwButton("WRITE BREAK", Modifier.weight(1f), ledColor = skin.secondary, height = 34.dp) {
                                    Native.writeBreakPattern(s.scene); s.rev++
                                }
                                HwButton("BOUNCE ${SCENE_NAMES[s.scene]}", Modifier.weight(1f), ledColor = skin.tertiary, height = 34.dp) { bounce() }
                            } else {
                                RotaryKnob(s, "Hardness", P.KIT_HARDNESS, 0, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
                                RotaryKnob(s, "${padLabel(s.selPad)} Vol", P.PAD_VOL, s.selPad, 0f, 1f, Modifier.weight(1f)) { pct(it) }
                            }
                        }
                        if (s.drumMsg.isNotEmpty()) SilkLabel(s.drumMsg, color = skin.tertiary, size = 10, bold = false)
                    }
                    DrumGridAssembler(s, grid, bars, bar, jungle, accent)
                    // Compact quick trim for currently selected track right below the 8x16 grid
                    ModulePanel("Selected Track: ${padLabel(s.selPad)}", accent = PAD_ROW_COLORS[s.selPad % 8]) {
                        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            RotaryKnob(s, "Pitch", P.PAD_PITCH, s.selPad, -12f, 12f, Modifier.weight(1f)) { signed(it, "st") }
                            RotaryKnob(s, "Decay", P.PAD_DECAY, s.selPad, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
                            RotaryKnob(s, "Pan", P.PAD_PAN, s.selPad, -1f, 1f, Modifier.weight(1f)) {
                                if (it < -0.02f) "L${(it * -100).roundToInt()}" else if (it > 0.02f) "R${(it * 100).roundToInt()}" else "CTR"
                            }
                        }
                    }
                }
                1 -> padsAnd808Section()
                else -> soundDesignSection()
            }
        }
    }
}

// ------------------------------------------------------------------ SYNTH TAB (LAYERED DEPTH + HIGH-ALPHA GHOST PIANO ROLL)

@Composable
fun ScalePad(s: AppState, deg: Int, modifier: Modifier) {
    val skin = LocalSkin.current
    val vel by rememberUpdatedState(s.vel())
    val synth by rememberUpdatedState(s.synth)
    val degState by rememberUpdatedState(deg)
    val name = remember(s.rev, s.synth, deg) { noteName(Native.degreeMidi(s.synth, deg)) }
    var held by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(if (skin.config.button == ButtonSkin.ALU_SWITCH) 4.dp else 9.dp)
    Box(
        modifier
            .height(54.dp)
            .clip(shape)
            .background(if (held) skin.secondary else skin.padSurface)
            .border(1.5.dp, if (held) Color.White else skin.line, shape)
            .pointerInput(Unit) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)
                    val track = synth
                    val d = degState
                    Native.noteOn(track, d, vel)
                    held = true
                    waitForUpOrCancellation()
                    Native.noteOff(track, d)
                    held = false
                }
            },
        contentAlignment = Alignment.Center
    ) {
        SilkLabel(name, color = if (held) skin.onAccentInk else skin.padInk, size = 12, mono = true)
    }
}

private data class GhostCellHit(
    val color: Color,
    val tag: String,
    val exactOctave: Boolean,
    val octDir: Int, // -1 below, 0 exact, +1 above
    val isStart: Boolean
)

/**
 * Ghost-Layered Piano Roll: overlays active synth notes on top of high-alpha ghost notes
 * from all other enabled synths (with octave-offset indicators) AND drum Kick/Snare/Hat rhythm guides.
 */
@Composable
fun GhostPianoRoll(
    s: AppState,
    bars: Int,
    page: Int,
    degOffset: Int,
    activeSynths: Int
) {
    val skin = LocalSkin.current
    val notes = remember(s.rev, s.scene, s.synth) { Native.notes(s.scene, s.synth) }
    val drumGrid = remember(s.rev, s.scene) { Native.drumGrid(s.scene) }
    val otherNotes = remember(s.rev, s.scene, s.synth, activeSynths) {
        (0 until activeSynths).filter { it != s.synth }.associateWith { Native.notes(s.scene, it) }
    }
    val rowMidis = remember(s.rev, s.synth, degOffset) {
        (0..7).map { i -> Native.degreeMidi(s.synth, degOffset + i) }
    }
    val activeColor = SYNTH_GHOST_COLORS[s.synth % SYNTH_GHOST_COLORS.size]

    // Ghost Layer Controls Bar
    Row(
        Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        SilkLabel("GHOST", color = skin.muted, size = 8, mono = true)
        HwButton(
            text = "DRM",
            on = s.ghostDrums && s.ghostAlpha > 0.01f,
            ledColor = skin.primary,
            height = 24.dp,
            pad = 6.dp
        ) { s.ghostDrums = !s.ghostDrums }

        for (other in 0 until activeSynths) {
            if (other == s.synth) continue
            val bit = 1 shl other
            val on = (s.ghostSynthMask and bit) != 0 && s.ghostAlpha > 0.01f
            HwButton(
                text = SYNTH_SHORT[other],
                on = on,
                ledColor = SYNTH_GHOST_COLORS[other],
                height = 24.dp,
                pad = 6.dp
            ) { s.ghostSynthMask = s.ghostSynthMask xor bit }
        }

        val alphaText = if (s.ghostAlpha < 0.05f) "OFF" else "${(s.ghostAlpha * 100).roundToInt()}%"
        HwButton(
            text = "ALPHA $alphaText",
            on = s.ghostAlpha > 0.05f,
            ledColor = skin.tertiary,
            height = 24.dp,
            pad = 6.dp
        ) {
            s.ghostAlpha = when {
                s.ghostAlpha >= 0.75f -> 0.35f
                s.ghostAlpha >= 0.55f -> 0.80f
                s.ghostAlpha >= 0.25f -> 0f
                else -> 0.60f
            }
        }
    }

    // Half-bar page selector
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        for (pg in 0 until bars * 2) {
            val playingHere = s.playing && s.step / 8 == pg
            HwButton(
                "${pg / 2 + 1}" + (if (pg % 2 == 0) "a" else "b"),
                on = pg == page,
                ledColor = if (playingHere) skin.positive else skin.secondary,
                height = 27.dp,
                pad = 7.dp
            ) { s.rollPage = pg }
        }
    }

    // 8 rows x 8 steps
    Column(verticalArrangement = Arrangement.spacedBy(3.dp)) {
        for (i in 7 downTo 0) {
            val deg = degOffset + i
            val rowMidi = rowMidis[i]
            val name = noteName(rowMidi)
            Row(horizontalArrangement = Arrangement.spacedBy(3.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel(name, color = skin.silk, size = 9, mono = true, modifier = Modifier.width(30.dp))
                for (col in 0 until 8) {
                    val st = page * 8 + col
                    val isBeat = col % 4 == 0
                    val playhead = s.playing && s.step == st

                    // Active synth note at (st, deg)
                    var v = 0
                    var sl = false
                    var k = 0
                    while (k + 4 < notes.size) {
                        if (notes[k] == st && notes[k + 1] == deg) {
                            v = notes[k + 3]
                            sl = (notes[k + 4] and 1) != 0
                        }
                        k += 5
                    }

                    // High-alpha ghost note lookup across enabled other synths
                    var ghostHit: GhostCellHit? = null
                    if (s.ghostAlpha > 0.05f) {
                        for ((other, arr) in otherNotes) {
                            if ((s.ghostSynthMask and (1 shl other)) == 0) continue
                            var idx = 0
                            while (idx + 4 < arr.size) {
                                val nSt = arr[idx]
                                val nDeg = arr[idx + 1]
                                val nLen = arr[idx + 2].coerceAtLeast(1)
                                if (st >= nSt && st < nSt + nLen) {
                                    val otherMidi = Native.degreeMidi(other, nDeg)
                                    val exact = otherMidi == rowMidi
                                    val samePitchClass = ((otherMidi - rowMidi) % 12) == 0
                                    if (exact || (samePitchClass && ghostHit == null)) {
                                        val dir = if (exact) 0 else if (otherMidi > rowMidi) 1 else -1
                                        ghostHit = GhostCellHit(
                                            color = SYNTH_GHOST_COLORS[other],
                                            tag = SYNTH_TAG[other],
                                            exactOctave = exact,
                                            octDir = dir,
                                            isStart = (st == nSt)
                                        )
                                        if (exact) break
                                    }
                                }
                                idx += 5
                            }
                            if (ghostHit?.exactOctave == true) break
                        }
                    }

                    // Drum beat column background guide (Kick / Snare pocket)
                    val drumKick = s.ghostDrums && s.ghostAlpha > 0.05f && drumGrid[0 * 128 + st].toInt() > 0
                    val drumSnare = s.ghostDrums && s.ghostAlpha > 0.05f && (drumGrid[1 * 128 + st].toInt() > 0 || drumGrid[2 * 128 + st].toInt() > 0)
                    val baseBg = when {
                        drumKick -> PAD_ROW_COLORS[0].copy(alpha = (s.ghostAlpha * 0.24f).coerceIn(0.06f, 0.24f))
                        drumSnare -> PAD_ROW_COLORS[1].copy(alpha = (s.ghostAlpha * 0.20f).coerceIn(0.05f, 0.20f))
                        else -> skin.recess
                    }

                    val cellShape = RoundedCornerShape(5.dp)
                    Box(
                        Modifier
                            .weight(1f)
                            .height(30.dp)
                            .clip(cellShape)
                            .background(baseBg)
                            .border(
                                width = if (playhead) 2.dp else 1.dp,
                                color = when {
                                    playhead -> skin.positive
                                    v == 2 -> Color.White
                                    isBeat -> skin.line.copy(alpha = 0.95f)
                                    else -> skin.line.copy(alpha = 0.55f)
                                },
                                shape = cellShape
                            )
                            .clickable {
                                if (Native.toggleNote(s.scene, s.synth, st, deg, s.accent, s.slide) && !s.slide) {
                                    Native.noteOn(s.synth, deg, s.vel()); Native.noteOff(s.synth, deg)
                                }
                                s.rev++
                            }
                    ) {
                        // Ghost note layer (rendered underneath active note at high alpha!)
                        if (ghostHit != null) {
                            val g = ghostHit!!
                            val gAlpha = if (g.exactOctave) s.ghostAlpha else (s.ghostAlpha * 0.52f)
                            Box(
                                Modifier
                                    .fillMaxSize()
                                    .padding(if (g.exactOctave) 1.5.dp else 3.dp)
                                    .clip(RoundedCornerShape(3.5.dp))
                                    .background(g.color.copy(alpha = gAlpha))
                                    .border(1.dp, g.color.copy(alpha = (s.ghostAlpha + 0.2f).coerceAtMost(1f)), RoundedCornerShape(3.5.dp)),
                                contentAlignment = Alignment.Center
                            ) {
                                if (v == 0 && g.isStart) {
                                    val arrow = when {
                                        g.octDir > 0 -> "^"
                                        g.octDir < 0 -> "v"
                                        else -> ""
                                    }
                                    SilkLabel("${g.tag}$arrow", color = Color.White, size = 8, mono = true)
                                }
                            }
                        }

                        // Active track note layer
                        if (v > 0) {
                            Box(
                                Modifier
                                    .fillMaxSize()
                                    .clip(cellShape)
                                    .background(if (v == 2) activeColor else activeColor.copy(alpha = 0.80f))
                                    .border(1.2.dp, if (v == 2) Color.White else activeColor, cellShape)
                            ) {
                                if (sl) {
                                    Box(Modifier.align(Alignment.CenterStart).width(5.dp).fillMaxHeight().background(HwYellow))
                                }
                                // Tiny corner dot if a ghost note overlaps the exact same cell
                                if (ghostHit != null) {
                                    Box(
                                        Modifier
                                            .align(Alignment.TopEnd)
                                            .padding(2.dp)
                                            .size(6.dp)
                                            .clip(CircleShape)
                                            .background(ghostHit!!.color)
                                            .border(0.8.dp, Color.White, CircleShape)
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }

        // Synchronized Drum Rhythm Footer Strip (Kick / Snare / Hat pocket markers)
        if (s.ghostDrums && s.ghostAlpha > 0.05f) {
            Row(
                Modifier.fillMaxWidth().padding(top = 2.dp),
                horizontalArrangement = Arrangement.spacedBy(3.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                SilkLabel("DRM", color = skin.primary, size = 8, mono = true, modifier = Modifier.width(30.dp))
                for (col in 0 until 8) {
                    val st = page * 8 + col
                    val kick = drumGrid[0 * 128 + st].toInt() > 0
                    val snare = drumGrid[1 * 128 + st].toInt() > 0 || drumGrid[2 * 128 + st].toInt() > 0
                    val hat = drumGrid[3 * 128 + st].toInt() > 0 || drumGrid[4 * 128 + st].toInt() > 0
                    val playhead = s.playing && s.step == st
                    Row(
                        Modifier
                            .weight(1f)
                            .height(18.dp)
                            .clip(RoundedCornerShape(4.dp))
                            .background(skin.recess)
                            .border(1.dp, if (playhead) skin.positive else skin.line.copy(alpha = 0.6f), RoundedCornerShape(4.dp)),
                        horizontalArrangement = Arrangement.spacedBy(2.dp, Alignment.CenterHorizontally),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        if (kick) Box(Modifier.size(7.dp).clip(CircleShape).background(PAD_ROW_COLORS[0].copy(alpha = (s.ghostAlpha + 0.3f).coerceAtMost(1f))))
                        if (snare) Box(Modifier.size(7.dp).clip(CircleShape).background(PAD_ROW_COLORS[1].copy(alpha = (s.ghostAlpha + 0.3f).coerceAtMost(1f))))
                        if (hat) Box(Modifier.size(5.dp).clip(CircleShape).background(PAD_ROW_COLORS[3].copy(alpha = (s.ghostAlpha + 0.25f).coerceAtMost(1f))))
                    }
                }
            }
        }
    }
}

@Composable
fun SynthTab(s: AppState) {
    val skin = LocalSkin.current
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }
    val page = s.rollPage.coerceIn(0, bars * 2 - 1)
    val mode = remember(s.rev, s.synth) { Native.getParam(P.SYN_MODE, s.synth).roundToInt().coerceIn(0, 3) }
    val isFm = mode == 1
    val wave1 = remember(s.rev, s.synth) { Native.getParam(P.SYN_WAVE1, s.synth).roundToInt() }
    val cutoff = remember(s.rev, s.synth) { Native.getParam(P.SYN_CUTOFF, s.synth) }
    val fmIndex = remember(s.rev, s.synth) { Native.getParam(P.SYN_FM_INDEX, s.synth) }
    val fmRatio = remember(s.rev, s.synth) { Native.getParam(P.SYN_FM_RATIO, s.synth) }
    val lfoTarget = remember(s.rev, s.synth) { Native.getParam(P.SYN_LFO_TARGET, s.synth).roundToInt() }
    val degOffset = remember(s.rev, s.synth, s.inputOctave[s.synth]) { s.degreeOffset(s.synth) }
    val octVal = s.inputOctave[s.synth]

    val activeSynths = remember(s.rev) { Native.getParam(P.ACTIVE_SYNTHS, 0).roundToInt().coerceIn(2, 4) }
    if (s.synth >= activeSynths) s.synth = activeSynths - 1
    val curInst = remember(s.rev, s.synth) { Native.getParam(P.SYN_INSTRUMENT, s.synth).roundToInt() }
    val waves = listOf("Saw", "Sqr", "Tri", "Sin")

    val trackSelectorRow: @Composable () -> Unit = {
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
            for (idx in 0 until activeSynths) {
                HwButton(TRACK_NAMES[idx + 1].uppercase(), on = s.synth == idx, ledColor = SYNTH_GHOST_COLORS[idx], height = 28.dp, pad = 8.dp) { s.synth = idx }
            }
            if (activeSynths < 4) {
                HwButton("+TRK", ledColor = skin.positive, height = 28.dp, pad = 7.dp) {
                    Native.setActiveSynths(activeSynths + 1)
                    s.synth = activeSynths
                    s.rev++
                }
            }
        }
    }

    val playAndRollPanel: @Composable () -> Unit = {
        ModulePanel(
            title = "Play ${TRACK_NAMES[s.synth + 1]} • OCT ${if (octVal >= 0) "+$octVal" else "$octVal"}",
            accent = SYNTH_GHOST_COLORS[s.synth % 4],
            trailing = {
                HwButton("OCT-", enabled = octVal > -2, height = 26.dp, pad = 6.dp) {
                    s.inputOctave[s.synth] = (octVal - 1).coerceIn(-2, 2)
                }
                HwButton("OCT+", enabled = octVal < 2, height = 26.dp, pad = 6.dp) {
                    s.inputOctave[s.synth] = (octVal + 1).coerceIn(-2, 2)
                }
            }
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                val chord = remember(s.rev, s.synth) { Native.getParam(P.SYN_CHORD, s.synth) > 0.5f }
                HwButton(if (chord) "CHORD ON" else "CHORD", Modifier.weight(1f), on = chord, ledColor = skin.tertiary, height = 30.dp) {
                    Native.beginEdit(); Native.setParam(P.SYN_CHORD, s.synth, if (chord) 0f else 1f); s.rev++
                }
                HwButton("ACCENT", Modifier.weight(1f), on = s.accent, ledColor = skin.primary, height = 30.dp, pad = 2.dp) { s.accent = !s.accent }
                HwButton("SLIDE", Modifier.weight(1f), on = s.slide, ledColor = skin.tertiary, height = 30.dp, pad = 2.dp) { s.slide = !s.slide; if (s.slide) s.rollMode = true }
                HwButton(if (s.rollMode) "PADS" else "ROLL", Modifier.weight(1f), on = s.rollMode, ledColor = skin.secondary, height = 30.dp, pad = 2.dp) { s.rollMode = !s.rollMode }
            }
            if (!s.rollMode) {
                for (row in 1 downTo 0) Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (col in 0 until 4) ScalePad(s, degOffset + row * 4 + col, Modifier.weight(1f))
                }
            } else {
                GhostPianoRoll(s, bars, page, degOffset, activeSynths)
            }
            SilkLabel(
                if (s.slide) "SLIDE is on: tap a note to glide in (yellow bar). Tap again to clear."
                else if (s.rollMode) "Ghost layers show Drums + other Synths at ${(s.ghostAlpha * 100).roundToInt()}% alpha (^/v = octave offset)."
                else "OCT-/+ shifts only ${TRACK_NAMES[s.synth + 1]} without altering other scenes.",
                color = if (s.slide) skin.tertiary else skin.muted, size = 9, bold = false
            )
        }
    }

    val quickMacroPanel: @Composable () -> Unit = {
        ModulePanel(
            title = "Quick Tone: ${TRACK_NAMES[s.synth + 1]} (${listOf("SUB", "FM", "PLUCK", "VECTOR")[mode]})",
            accent = skin.secondary
        ) {
            trackSelectorRow()
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel("INST", size = 9, modifier = Modifier.width(32.dp))
                INSTRUMENT_NAMES.forEachIndexed { idx, nm ->
                    HwButton(nm, on = curInst == idx, ledColor = skin.tertiary, height = 26.dp, pad = 6.dp) {
                        Native.applyInstrument(s.synth, idx); s.rev++
                    }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                RotaryKnob(s, "Warmth", P.SYN_WARMTH, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
                RotaryKnob(s, "Motion", P.SYN_MOTION, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
            }
        }
    }

    val sculptPanel: @Composable () -> Unit = {
        ModulePanel(
            title = "Engine & Filter: ${TRACK_NAMES[s.synth + 1]}",
            accent = skin.secondary
        ) {
            trackSelectorRow()
            OledScopeDisplay(
                isFm = isFm,
                wave = wave1,
                cutoffNorm = ((cutoff - 80f) / 7920f).coerceIn(0.08f, 1f),
                fmIndex = fmIndex,
                fmRatio = fmRatio
            )
            Choice(s, "Engine", P.SYN_MODE, s.synth, listOf("Subtractive", "FM 2-Op", "Pluck", "Vector"), accent = skin.secondary)

            if (mode == 2) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Bright", P.SYN_PLUCK_BRIGHT, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                    RotaryKnob(s, "Ring", P.SYN_DECAY, s.synth, 0.03f, 1.5f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { secs(it * 4f) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                    RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
                }
            } else if (mode == 3) {
                val w1 = waves[wave1.coerceIn(0, 3)]
                val w2 = waves[remember(s.rev, s.synth) { Native.getParam(P.SYN_WAVE2, s.synth).roundToInt() }.coerceIn(0, 3)]
                VectorPad(s, s.synth, listOf("OSC 1 $w1", "OSC 2 $w2", "FM", "SUB"))
                Choice(s, "Osc 1", P.SYN_WAVE1, s.synth, waves, accent = skin.secondary)
                Choice(s, "Osc 2", P.SYN_WAVE2, s.synth, waves, accent = skin.secondary)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "FM Ratio", P.SYN_FM_RATIO, s.synth, 0.5f, 8f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { "${(it * 2).roundToInt() / 2f}x" }
                    RotaryKnob(s, "FM Index", P.SYN_FM_INDEX, s.synth, 0f, 8f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { "${(it * 10).roundToInt() / 10f}" }
                    RotaryKnob(s, "Detune", P.SYN_DETUNE, s.synth, -30f, 30f, Modifier.weight(1f), modulated = lfoTarget == 2) { signed(it, "c") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                    RotaryKnob(s, "Res", P.SYN_RES, s.synth, 0.5f, 15f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
                    RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
                }
            } else if (mode == 0) {
                Choice(s, "Osc 1", P.SYN_WAVE1, s.synth, waves, accent = skin.secondary)
                Choice(s, "Osc 2", P.SYN_WAVE2, s.synth, waves, accent = skin.secondary)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                    RotaryKnob(s, "Res", P.SYN_RES, s.synth, 0.5f, 15f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Detune", P.SYN_DETUNE, s.synth, -30f, 30f, Modifier.weight(1f), modulated = lfoTarget == 2) { signed(it, "c") }
                    RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
                }
            } else {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Ratio", P.SYN_FM_RATIO, s.synth, 0.5f, 8f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { "${(it * 2).roundToInt() / 2f}x" }
                    RotaryKnob(s, "Index", P.SYN_FM_INDEX, s.synth, 0f, 8f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { "${(it * 10).roundToInt() / 10f}" }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "ModDec", P.SYN_FM_DECAY, s.synth, 0.01f, 2f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { secs(it) }
                    RotaryKnob(s, "Fdbk", P.SYN_FM_FB, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                    RotaryKnob(s, "Res", P.SYN_RES, s.synth, 0.5f, 15f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
                    RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
                }
            }
        }
    }

    val deepModPanel: @Composable () -> Unit = {
        ModulePanel("Envelope, LFO & Wavefolder", accent = skin.secondary) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Attack", P.SYN_ATTACK, s.synth, 0.002f, 0.4f, Modifier.weight(1f)) { "${(it * 1000).roundToInt()}ms" }
                RotaryKnob(s, "Decay", P.SYN_DECAY, s.synth, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Sustain", P.SYN_SUSTAIN, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
                RotaryKnob(s, "Release", P.SYN_RELEASE, s.synth, 0.01f, 1.5f, Modifier.weight(1f)) { secs(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "LFO Hz", P.SYN_LFO_RATE, s.synth, 0.1f, 12f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
                RotaryKnob(s, "LFO Dep", P.SYN_LFO_DEPTH, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Fold", P.SYN_FOLD, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
                RotaryKnob(s, "Slide", P.SYN_SLIDE_TIME, s.synth, 0.01f, 0.4f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { "${(it * 1000).roundToInt()}ms" }
            }
            Choice(s, "LFO ->", P.SYN_LFO_TARGET, s.synth, listOf("Off", "Cutoff", "Pitch", "Amp"), accent = skin.secondary)
        }
    }

    if (s.wide) {
        // Landscape: Sound Design on Left, Play & Ghost Piano Roll on Right
        Panes(s, left = {
            LibraryPanel(s, "instrument", "INSTRUMENT LIBRARY", "Replaces the sound of ${TRACK_NAMES[s.synth + 1]}. Notes stay.", skin.secondary)
            quickMacroPanel()
            sculptPanel()
            deepModPanel()
        }, right = {
            playAndRollPanel()
        })
    } else {
        // Portrait: Clean 3-layer flow (1. PLAY & ROLL -> 2. SCULPT -> 3. MOD & DEEP)
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            LayerSwitcher(
                layers = listOf("1. PLAY & ROLL", "2. SCULPT", "3. MOD & DEEP"),
                selected = s.synthLayer,
                accent = skin.secondary,
                onSelect = { s.synthLayer = it }
            )
            LibraryPanel(s, "instrument", "INSTRUMENT LIBRARY", "Replaces the sound of ${TRACK_NAMES[s.synth + 1]}. Notes stay.", skin.secondary)
            when (s.synthLayer) {
                0 -> {
                    quickMacroPanel()
                    playAndRollPanel()
                }
                1 -> {
                    sculptPanel()
                }
                else -> {
                    deepModPanel()
                }
            }
        }
    }
}

// ------------------------------------------------------------------ FX & MASTER TAB (GUITAR PEDALBOARD CHAIN)

@Composable
fun MixTab(s: AppState) {
    val skin = LocalSkin.current
    val activeTracks = 1 + remember(s.rev) { Native.getParam(P.ACTIVE_SYNTHS, 0).roundToInt().coerceIn(2, 4) }
    val t = s.fxTrack.coerceIn(0, activeTracks - 1)
    val mute = remember(s.rev, t) { Native.getParam(P.FX_MUTE, t) > 0.5f }

    Panes(s, left = {
        // Instrument Rail + Guitar Pedalboard Chain
        ModulePanel(
            title = "Rig Track: ${TRACK_NAMES[t]}",
            accent = skin.tertiary
        ) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (i in 0 until activeTracks) {
                    val trkCol = if (i == 0) skin.primary else SYNTH_GHOST_COLORS[(i - 1) % 4]
                    HwButton(TRACK_NAMES[i].uppercase(), on = t == i, ledColor = trkCol, showLed = true, height = 30.dp, pad = 9.dp) { s.fxTrack = i }
                }
            }
        }

        PedalboardPanel(s, t)

    }, right = {
        ModulePanel(
            title = "Channel Amp & Sends: ${TRACK_NAMES[t]}",
            accent = skin.tertiary,
            trailing = {
                HwButton(if (mute) "MUTED" else "MUTE", on = mute, ledColor = skin.danger, showLed = true, height = 26.dp, pad = 8.dp) {
                    Native.beginEdit(); Native.setParam(P.FX_MUTE, t, if (mute) 0f else 1f); s.rev++
                }
            }
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Level", P.FX_LEVEL, t, 0f, 1f, Modifier.weight(1f)) { pct(it) }
                RotaryKnob(s, "Drive", P.FX_DRIVE, t, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Low EQ", P.FX_EQ_LOW, t, -12f, 12f, Modifier.weight(1f)) { signed(it, "dB") }
                RotaryKnob(s, "High EQ", P.FX_EQ_HIGH, t, -12f, 12f, Modifier.weight(1f)) { signed(it, "dB") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Dly Snd", P.FX_DELAY, t, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
                RotaryKnob(s, "Rev Snd", P.FX_REVERB, t, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
            }
        }

        ModulePanel("Master Bus & Analog VU", accent = skin.primary) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Master", P.MASTER, 0, 0f, 1f, Modifier.weight(1f)) { pct(it) }
                RotaryKnob(s, "Dly Fdbk", P.DELAY_FB, 0, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
                RotaryKnob(s, "Swing", P.SWING, 0, 0f, 0.5f, Modifier.weight(1f)) { if (it < 0.005f) "STR" else "${(50 + it * 50).roundToInt()}%" }
            }
            AnalogVuMeter(s.peakL, s.peakR)
        }
    })
}

// ------------------------------------------------------------------ SONG TAB

@Composable
fun SongTab(s: AppState) {
    val skin = LocalSkin.current
    val chain = remember(s.rev) { Native.chain() }
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }

    val activeTracks = 1 + remember(s.rev) { Native.getParam(P.ACTIVE_SYNTHS, 0).roundToInt().coerceIn(2, 4) }
    val selPos = s.selChainPos.coerceIn(0, (chain.size - 1).coerceAtLeast(0))
    val autoTrk = s.selAutoTrack.coerceIn(0, activeTracks - 1)
    val autoLn = s.selAutoLane.coerceIn(0, 2)
    val autoData = remember(s.rev, selPos, autoTrk) { Native.chainAuto(selPos, autoTrk) }
    val curPid = autoData.getOrElse(autoLn * 3) { -1f }.roundToInt()
    val curStart = autoData.getOrElse(autoLn * 3 + 1) { 0f }
    val curEnd = autoData.getOrElse(autoLn * 3 + 2) { 0f }

    val quick = if (autoTrk == 0) AUTO_QUICK_DRUMS else AUTO_QUICK_SYNTH
    val laneIds = (0 until 3).map { autoData.getOrElse(it * 3) { -1f }.roundToInt() }

    Panes(s, left = {
        LibraryPanel(s, "groove", "GROOVE LIBRARY", "Replaces the patterns of scene ${SCENE_NAMES[s.scene]}, brings its sounds.", skin.positive)
        ModulePanel(
            title = "Arrangement Chain",
            accent = skin.positive,
            trailing = {
                HwButton(if (s.songMode) "SONG ON" else "SONG OFF", on = s.songMode, ledColor = skin.positive, showLed = true, height = 28.dp) {
                    s.songMode = !s.songMode
                    Native.setSongMode(s.songMode)
                    if (s.playing) Native.play(s.scene)
                }
            }
        ) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                chain.forEachIndexed { pos, sc ->
                    val playingHere = s.playing && s.songMode && s.chainPos == pos
                    val selectedHere = pos == selPos
                    HwButton(
                        text = "${pos + 1}:${SCENE_NAMES[sc.coerceIn(0, 3)]}",
                        on = playingHere || selectedHere,
                        ledColor = if (playingHere) skin.positive else skin.secondary,
                        height = 34.dp
                    ) { s.selChainPos = pos }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel("ADD", size = 9)
                SCENE_NAMES.forEachIndexed { i, name ->
                    HwButton("+ $name", Modifier.weight(1f), height = 30.dp) { Native.chainAdd(i); s.rev++ }
                }
                HwButton("DEL", enabled = chain.size > 1, ledColor = skin.danger, height = 30.dp, pad = 8.dp) {
                    Native.chainRemove(selPos)
                    s.selChainPos = (selPos - 1).coerceAtLeast(0)
                    s.rev++
                }
            }
        }

        ModulePanel(
            title = "Slot #${selPos + 1}: moves over time",
            accent = skin.primary
        ) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (t in 0 until activeTracks) {
                    HwButton(TRACK_NAMES[t].uppercase(), on = autoTrk == t, ledColor = skin.secondary, height = 26.dp, pad = 6.dp) { s.selAutoTrack = t; s.autoMsg = "" }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                for (ln in 0 until 3) {
                    val name = AUTO_PARAMS[laneIds[ln]]?.label ?: if (laneIds[ln] >= 0) "Control" else "empty"
                    HwButton("${ln + 1}: $name", Modifier.weight(1f), on = autoLn == ln, ledColor = skin.primary, showLed = laneIds[ln] >= 0, height = 30.dp, pad = 2.dp) { s.selAutoLane = ln }
                }
            }
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel("ADD", size = 9, modifier = Modifier.width(28.dp))
                HwButton("+ ANY CONTROL", on = s.pickAuto, ledColor = skin.tertiary, showLed = true, height = 30.dp, pad = 8.dp) {
                    s.pickAuto = true; s.autoMsg = ""
                    s.fxTrack = autoTrk
                    if (autoTrk > 0) { s.synth = autoTrk - 1; s.tab = 2 } else s.tab = 0
                }
                for (id in quick) {
                    val info = AUTO_PARAMS[id] ?: continue
                    HwButton(info.label, on = id in laneIds, ledColor = skin.tertiary, height = 30.dp, pad = 7.dp) { s.addAutomation(id, autoIndexOf(id, autoTrk)) }
                }
            }
            if (s.autoMsg.isNotEmpty()) SilkLabel(s.autoMsg, color = skin.tertiary, size = 10, bold = false)
            if (curPid >= 0) {
                val info = AUTO_PARAMS[curPid] ?: AutoInfo("Control", minOf(curStart, curEnd, 0f), maxOf(curStart, curEnd, 1f))
                val show: (Float) -> String = { v -> if (info.hi - info.lo > 50f) "${v.roundToInt()}" else "%.2f".format(v) }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Knob("Start", curStart, info.lo, info.hi, Modifier.weight(1f), capColor = Color(0xFF1F3847), fmt = show,
                        onStart = { Native.beginEdit() },
                        onChange = { Native.setChainAutoLive(selPos, autoTrk, autoLn, curPid, it, curEnd) },
                        onEnd = { s.rev++ })
                    Knob("End", curEnd, info.lo, info.hi, Modifier.weight(1f), capColor = Color(0xFF422A1A), fmt = show,
                        onStart = { Native.beginEdit() },
                        onChange = { Native.setChainAutoLive(selPos, autoTrk, autoLn, curPid, curStart, it) },
                        onEnd = { s.rev++ })
                }
                Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    HwButton("START = NOW", Modifier.weight(1f), ledColor = skin.secondary, height = 28.dp, pad = 2.dp) {
                        val now = Native.getParam(curPid, autoIndexOf(curPid, autoTrk)).coerceIn(info.lo, info.hi)
                        Native.setChainAuto(selPos, autoTrk, autoLn, curPid, now, curEnd); s.rev++
                    }
                    HwButton("SWAP", Modifier.weight(0.7f), ledColor = skin.tertiary, height = 28.dp, pad = 2.dp) {
                        Native.setChainAuto(selPos, autoTrk, autoLn, curPid, curEnd, curStart); s.rev++
                    }
                    HwButton("REMOVE", Modifier.weight(0.8f), ledColor = skin.danger, height = 28.dp, pad = 2.dp) {
                        Native.setChainAuto(selPos, autoTrk, autoLn, -1, 0f, 0f); s.autoMsg = ""; s.rev++
                    }
                }
                SilkLabel("While slot #${selPos + 1} plays, ${info.label} of ${TRACK_NAMES[autoTrk]} moves from Start to End.", color = skin.muted, size = 9, bold = false)
            } else {
                SilkLabel("Lane ${autoLn + 1} of ${TRACK_NAMES[autoTrk]} is empty. Add a control above.", color = skin.muted, size = 9, bold = false)
            }
        }

    }, right = {
        ModulePanel("Mixer", accent = skin.positive) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for (t in 0 until activeTracks) {
                    Column(Modifier.width(96.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                        val mute = remember(s.rev, t) { Native.getParam(P.FX_MUTE, t) > 0.5f }
                        RotaryKnob(s, TRACK_NAMES[t], P.FX_LEVEL, t, 0f, 1f, Modifier.fillMaxWidth()) { pct(it) }
                        HwButton(if (mute) "MUTED" else "MUTE", Modifier.fillMaxWidth(), on = mute, ledColor = skin.danger, height = 28.dp, pad = 2.dp) {
                            Native.beginEdit(); Native.setParam(P.FX_MUTE, t, if (mute) 0f else 1f); s.rev++
                        }
                    }
                }
                RotaryKnob(s, "Master", P.MASTER, 0, 0f, 1f, Modifier.width(96.dp)) { pct(it) }
            }
        }

        ModulePanel("Projects", accent = skin.secondary) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                BasicTextField(
                    value = s.projectName,
                    onValueChange = { s.projectName = it.take(32).filter { c -> c.isLetterOrDigit() || c in " -_" } },
                    singleLine = true,
                    textStyle = TextStyle(color = skin.silk, fontSize = 14.sp),
                    cursorBrush = SolidColor(skin.secondary),
                    modifier = Modifier.weight(1f).clip(RoundedCornerShape(6.dp)).background(skin.recess).border(1.dp, skin.line, RoundedCornerShape(6.dp)).padding(horizontal = 10.dp, vertical = 9.dp)
                )
                HwButton("SAVE", ledColor = skin.positive, height = 34.dp, pad = 12.dp) { s.onSaveProject(s.projectName) }
            }
            if (s.projectMsg.isNotEmpty()) SilkLabel(s.projectMsg, color = skin.tertiary, size = 10, bold = false)
            if (s.projects.isEmpty()) SilkLabel("Nothing saved yet. The open project is also kept automatically when you leave the app.", color = skin.muted, size = 9, bold = false)
            for (nm in s.projects) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    SilkLabel(nm, color = skin.silk, size = 12, modifier = Modifier.weight(1f))
                    HwButton("OPEN", ledColor = skin.secondary, height = 30.dp, pad = 9.dp) { s.onOpenProject(nm) }
                    val sure = s.confirmDelete == nm
                    HwButton(if (sure) "SURE?" else "DEL", on = sure, ledColor = skin.danger, height = 30.dp, pad = 9.dp) {
                        if (sure) { s.confirmDelete = ""; s.onDeleteProject(nm) } else s.confirmDelete = nm
                    }
                }
            }
        }

        ModulePanel("Export for your DAW", accent = skin.silk) {
            SilkLabel(
                if (s.songMode) "Exports the whole song chain." else "Exports scene ${SCENE_NAMES[s.scene]}. Switch SONG on to export the chain.",
                color = skin.muted, size = 9, bold = false
            )
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                HwButton("EXPORT .MID", Modifier.weight(1f), enabled = !s.exporting, height = 38.dp, pad = 2.dp) { s.onExport(0) }
                HwButton("RENDER .WAV", Modifier.weight(1f), enabled = !s.exporting, height = 38.dp, pad = 2.dp) { s.onExport(1) }
                HwButton("STEMS", Modifier.weight(0.7f), enabled = !s.exporting, height = 38.dp, pad = 2.dp) { s.onExport(2) }
            }
            if (s.exportMsg.isNotEmpty()) SilkLabel(s.exportMsg, color = skin.tertiary, size = 10)
        }

        ModulePanel("Scene ${SCENE_LABELS[s.scene]}", accent = skin.secondary) {
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel("BARS", size = 9, modifier = Modifier.width(38.dp))
                for (b in 1..8) HwButton("$b", Modifier.weight(1f), on = bars == b, ledColor = skin.secondary, height = 30.dp, pad = 1.dp) {
                    Native.setSceneBars(s.scene, b); s.rev++
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel("COPY", size = 9, modifier = Modifier.width(38.dp))
                SCENE_NAMES.forEachIndexed { i, name ->
                    HwButton("-> $name", Modifier.weight(1f), enabled = i != s.scene, height = 30.dp) {
                        Native.copyScene(s.scene, i); s.rev++
                    }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                HwButton("CLEAR SCENE", Modifier.weight(1f), height = 32.dp) { Native.clearScene(s.scene); s.rev++ }
                HwButton("RESET DEMO", Modifier.weight(1f), height = 32.dp) { Native.resetToDemo(); s.rev++ }
            }
            HwButton("LOAD PRESET: WAREHOUSE 303", Modifier.fillMaxWidth(), ledColor = skin.positive, height = 32.dp) {
                Native.loadStartPreset(); s.scene = 0; s.drumBar = 0; s.rollPage = 0; s.rev++
            }
        }

        ModulePanel("Key & Scale", accent = skin.tertiary) {
            Choice(s, "Root", P.ROOT, 0, NOTE_NAMES, accent = skin.tertiary)
            Choice(s, "Scale", P.SCALE, 0, listOf("Minor", "Major", "Dorian", "Penta"), accent = skin.tertiary)
            Choice(s, "Base Oct", P.OCTAVE, 0, listOf("Low", "Mid", "High"), offset = 2, accent = skin.tertiary)
        }
    })
}
