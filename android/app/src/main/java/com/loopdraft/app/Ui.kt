package com.loopdraft.app

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
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.graphics.drawscope.Stroke
import kotlin.math.roundToInt

val PAD_NAMES = listOf("Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc")
val PAD_SHORT = listOf("Kik", "Snr", "Clp", "CH", "OH", "Rim", "Tom", "Prc")
val SCENE_NAMES = listOf("A", "B", "C", "D")
val SCENE_LABELS = listOf("A: INTRO", "B: VERSE", "C: CHORUS", "D: BRIDGE")
val TRACK_NAMES = listOf("Drums", "Bass", "Lead", "Syn 3", "Syn 4")
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
    var slide by mutableStateOf(false)     // piano roll: taps set or clear a slide
    // The library: presets and grooves that ship with the app, plus the user's own.
    val library = mutableStateListOf<LibItem>()
    var libOpen by mutableStateOf("")       // which kind is unfolded: "instrument", "drumkit", "groove" or none
    var libName by mutableStateOf("My preset")
    var libMsg by mutableStateOf("")
    var libTempoKey by mutableStateOf(true)   // a groove also sets tempo and key
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
    var drumRollMode by mutableIntStateOf(0) // 0=normal, 3=2x, 4=3x, 5=4x ratchet roll
    var selChainPos by mutableIntStateOf(0)
    var wide by mutableStateOf(false)      // landscape: side rail and two panes
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
        while (true) {
            withFrameNanos { }
            s.frame()
        }
    }
    MaterialTheme(
        colorScheme = darkColorScheme(
            primary = HwOrange,
            background = HwChassis,
            surface = HwFaceplate,
            onSurface = HwSilk,
            onBackground = HwSilk
        )
    ) {
        BoxWithConstraints(Modifier.fillMaxSize().background(HwChassis).safeDrawingPadding()) {
            val wide = maxWidth > maxHeight && maxWidth >= 520.dp
            if (s.wide != wide) s.wide = wide
            val frame = if (wide) RoundedCornerShape(8.dp) else RoundedCornerShape(bottomStart = 8.dp, bottomEnd = 8.dp)
            val content: @Composable (Modifier) -> Unit = { m ->
                Box(m.border(2.dp, sceneColor(s.scene), frame).padding(horizontal = 8.dp, vertical = 8.dp)) {
                    when (s.tab) {
                        0 -> DrumsTab(s)
                        1 -> SynthTab(s)
                        2 -> MixTab(s)
                        else -> SongTab(s)
                    }
                }
            }
            Row(Modifier.fillMaxSize()) {
                WalnutRail()
                if (wide) {
                    // Landscape: everything that is not the instrument itself moves into a rail on the left.
                    SideRail(s)
                    content(Modifier.weight(1f).fillMaxHeight().padding(top = 6.dp, bottom = 6.dp, end = 6.dp))
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
        }
    }
}

/** One preset or groove in the library. `user` ones are files the user saved or imported and can delete. */
data class LibItem(val kind: String, val name: String, val style: String, val user: Boolean, val path: String)

/**
 * The library for one kind of content. Folded up it is a single button. Loading is one undo step.
 * what: what a tap on LOAD changes, shown as a hint.
 */
@Composable
fun LibraryPanel(s: AppState, kind: String, title: String, what: String, accent: Color) {
    val items = s.library.filter { it.kind == kind }
    if (s.libOpen != kind) {
        HwButton("$title (${items.size})", Modifier.fillMaxWidth(), ledColor = accent, height = 32.dp) { s.libOpen = kind; s.libMsg = ""; s.confirmDelete = "" }
        return
    }
    ModulePanel(
        title = title,
        accent = accent,
        trailing = { HwButton("CLOSE", ledColor = HwOrange, height = 26.dp, pad = 8.dp) { s.libOpen = "" } }
    ) {
        SilkLabel(what, color = HwMuted, size = 9, bold = false)
        if (kind == "groove") {
            HwButton(if (s.libTempoKey) "ALSO TAKES TEMPO AND KEY: ON" else "ALSO TAKES TEMPO AND KEY: OFF", Modifier.fillMaxWidth(), on = s.libTempoKey, ledColor = HwYellow, showLed = true, height = 28.dp) {
                s.libTempoKey = !s.libTempoKey
            }
        }
        for (item in items) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    SilkLabel(item.name, color = HwSilk, size = 12)
                    if (item.style.isNotEmpty() || item.user) SilkLabel(listOf(item.style, if (item.user) "yours" else "").filter { t -> t.isNotEmpty() }.joinToString(" - "), color = HwMuted, size = 9, bold = false)
                }
                HwButton("LOAD", ledColor = accent, height = 30.dp, pad = 10.dp) { s.onLibLoad(item) }
                if (item.user) {
                    val sure = s.confirmDelete == "lib:" + item.path
                    HwButton(if (sure) "SURE?" else "DEL", on = sure, ledColor = HwRed, height = 30.dp, pad = 8.dp) {
                        if (sure) { s.confirmDelete = ""; s.onLibDelete(item) } else s.confirmDelete = "lib:" + item.path
                    }
                }
            }
        }
        if (items.isEmpty()) SilkLabel("Nothing here yet.", color = HwMuted, size = 10, bold = false)
        if (s.libMsg.isNotEmpty()) SilkLabel(s.libMsg, color = HwYellow, size = 10, bold = false)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            BasicTextField(
                value = s.libName,
                onValueChange = { v -> s.libName = v.take(40).filter { c -> c.isLetterOrDigit() || c in " -_" } },
                singleLine = true,
                textStyle = TextStyle(color = HwSilk, fontSize = 14.sp),
                cursorBrush = SolidColor(accent),
                modifier = Modifier.weight(1f).clip(RoundedCornerShape(6.dp)).background(HwRecess).border(1.dp, HwLine, RoundedCornerShape(6.dp)).padding(horizontal = 10.dp, vertical = 9.dp)
            )
            HwButton("SAVE MINE", ledColor = HwGreen, height = 34.dp, pad = 8.dp) { s.onLibSave(kind, s.libName) }
            HwButton("FILE", ledColor = HwCyan, height = 34.dp, pad = 8.dp) { s.onLibImport() }
        }
        SilkLabel("SAVE MINE: keeps the current one under that name.", color = HwMuted, size = 9, bold = false)
        SilkLabel("FILE: adds a .json preset or groove from the phone.", color = HwMuted, size = 9, bold = false)
    }
}

/**
 * A screen is two groups of panels. Portrait stacks them in one scrolling column.
 * Landscape puts them side by side, each scrolling on its own.
 */
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

fun sceneColor(i: Int): Color = when (i) { 0 -> HwOrange; 1 -> HwCyan; 2 -> HwYellow; else -> HwGreen }

@Composable
fun Choice(s: AppState, label: String, id: Int, index: Int, options: List<String>, offset: Int = 0, accent: Color = HwOrange) {
    val cur = remember(s.rev, id, index) { Native.getParam(id, index).roundToInt() }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        SilkLabel(label.uppercase(), size = 9, modifier = Modifier.width(56.dp))
        Row(Modifier.weight(1f).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            options.forEachIndexed { i, name ->
                HwButton(name, on = cur == i + offset, ledColor = accent, height = 30.dp, pad = 7.dp) {
                    Native.beginEdit(); Native.setParam(id, index, (i + offset).toFloat()); s.rev++
                }
            }
        }
    }
}

@Composable
fun RollCell(v: Int, playhead: Boolean, beat: Boolean, color: Color, modifier: Modifier, slide: Boolean = false, onTap: () -> Unit) {
    val shape = RoundedCornerShape(5.dp)
    Box(
        modifier
            .clip(shape)
            .background(when (v) { 2 -> color; 1 -> color.copy(alpha = 0.65f); else -> HwRecess })
            .border(
                if (playhead) 2.dp else 1.dp,
                if (playhead) HwGreen else if (v == 2) Color.White else if (beat) Color(0xFF4A5060) else HwLine,
                shape
            )
            .clickable(onClick = onTap)
    ) {
        // a slide note carries a bar on its left edge: it is glided into from the note before
        if (slide && v > 0) Box(Modifier.align(Alignment.CenterStart).width(5.dp).fillMaxHeight().background(HwYellow))
    }
}

/**
 * The Vector engine's X/Y pad: one finger mixes the four sources in the corners.
 * Touching it counts as one undo step. It swallows the drag, so the page does not scroll under the finger.
 */
@Composable
fun VectorPad(s: AppState, synth: Int, corners: List<String>) {
    val px = remember { mutableFloatStateOf(0.5f) }
    val py = remember { mutableFloatStateOf(0.5f) }
    var dragging by remember { mutableStateOf(false) }
    val x0 = remember(s.rev, synth) { Native.getParam(P.SYN_VEC_X, synth) }
    val y0 = remember(s.rev, synth) { Native.getParam(P.SYN_VEC_Y, synth) }
    LaunchedEffect(x0, y0, synth) { if (!dragging) { px.floatValue = x0; py.floatValue = y0 } }
    val shape = RoundedCornerShape(8.dp)
    Box(
        Modifier.fillMaxWidth().height(170.dp).clip(shape).background(Color(0xFF07130F))
            .border(1.dp, if (dragging) HwGreen else HwLine, shape)
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
            val grid = HwGreen.copy(alpha = 0.13f)
            for (i in 1..3) {
                drawLine(grid, Offset(size.width * i / 4f, 0f), Offset(size.width * i / 4f, size.height), 1.dp.toPx())
                drawLine(grid, Offset(0f, size.height * i / 4f), Offset(size.width, size.height * i / 4f), 1.dp.toPx())
            }
            val c = Offset(px.floatValue * size.width, py.floatValue * size.height)
            val beam = HwGreen.copy(alpha = 0.55f)
            drawLine(beam, Offset(c.x, 0f), Offset(c.x, size.height), 1.dp.toPx())
            drawLine(beam, Offset(0f, c.y), Offset(size.width, c.y), 1.dp.toPx())
            drawCircle(HwGreen.copy(alpha = 0.25f), 16.dp.toPx(), c)
            drawCircle(HwGreen, 7.dp.toPx(), c)
            drawCircle(Color.White, 7.dp.toPx(), c, style = Stroke(1.5.dp.toPx()))
        }
        val al = listOf(Alignment.TopStart, Alignment.TopEnd, Alignment.BottomStart, Alignment.BottomEnd)
        for (i in 0 until 4) SilkLabel(corners[i], color = HwGreen.copy(alpha = 0.8f), size = 9, modifier = Modifier.align(al[i]).padding(6.dp))
    }
}


fun pct(v: Float) = "${(v * 100).roundToInt()}%"
fun signed(v: Float, unit: String) = (if (v >= 0) "+" else "") + v.roundToInt() + unit
fun secs(v: Float) = "${(v * 100).roundToInt() / 100f}s"

// ------------------------------------------------------------------ top hardware transport & nav

@Composable
fun TopBar(s: AppState) {
    val canUndo = remember(s.rev) { Native.canUndo() }
    val canRedo = remember(s.rev) { Native.canRedo() }
    val bpm = remember(s.rev) { Native.getParam(P.BPM, 0).roundToInt() }
    fun setBpm(v: Int) { Native.beginEdit(); Native.setParam(P.BPM, 0, v.coerceIn(40, 240).toFloat()); s.rev++ }

    Column(Modifier.fillMaxWidth().background(HwFaceplate).padding(horizontal = 8.dp, vertical = 6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            Column(Modifier.weight(1f)) {
                Logo()
                SilkLabel(if (s.audioOk) s.info else "NO AUDIO", color = HwMuted, size = 8, bold = false)
            }
            HwButton("UNDO", enabled = canUndo, height = 28.dp, pad = 6.dp) { if (Native.undo()) s.rev++ }
            HwButton("REDO", enabled = canRedo, height = 28.dp, pad = 6.dp) { if (Native.redo()) s.rev++ }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            HwButton(if (s.playing) "STOP" else "PLAY", Modifier.weight(1.1f), on = s.playing, ledColor = HwGreen, showLed = true, height = 38.dp) {
                if (s.playing) Native.stopTransport() else Native.play(s.scene)
            }
            HwButton("REC", on = s.rec, ledColor = HwRed, showLed = true, height = 38.dp, pad = 8.dp) {
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
    val bars = remember(s.rev, i) { Native.sceneBars(i) }
    val active = s.scene == i
    val queued = s.queued == i
    val col = if (queued) HwYellow else sceneColor(i)
    Box(
        modifier
            .height(36.dp)
            .clip(shape)
            .background(if (active || queued) col else Color(0xFF2B2E35))
            .border(1.dp, if (active || queued) col else HwLine, shape)
            .clickable {
                if (!(s.playing && s.songMode)) {       // in song mode the chain decides
                    Native.queueScene(i)                  // while playing: switches on the next bar line
                    if (!s.playing) { s.scene = i; s.drumBar = 0; s.rollPage = 0 }
                }
            },
        contentAlignment = Alignment.Center
    ) {
        SilkLabel(
            text = if (bars > 1) "${SCENE_NAMES[i]}  $bars bars" else SCENE_NAMES[i],
            color = if (active || queued) HwPadInk else HwSilk,
            size = 12
        )
    }
}

@Composable
fun SceneStrip(s: AppState) {
    val tab = RoundedCornerShape(topStart = 9.dp, topEnd = 9.dp)
    Row(
        Modifier.fillMaxWidth().background(HwChassis).padding(start = 6.dp, end = 6.dp, top = 5.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        for (i in 0 until 4) SceneTab(s, i, Modifier.weight(1f), tab)
    }
}

val NAV_TABS = listOf("DRUMS" to HwOrange, "SYNTH" to HwCyan, "FX" to HwYellow, "SONG" to HwGreen)

@Composable
fun NavButton(s: AppState, i: Int, modifier: Modifier) {
    HwButton(text = NAV_TABS[i].first, modifier = modifier, on = s.tab == i, ledColor = NAV_TABS[i].second, showLed = true, height = 40.dp, pad = 4.dp) { s.tab = i }
}

/** Landscape: logo, transport, tempo, undo, scenes and screen buttons in one narrow column. */
@Composable
fun SideRail(s: AppState) {
    val canUndo = remember(s.rev) { Native.canUndo() }
    val canRedo = remember(s.rev) { Native.canRedo() }
    val bpm = remember(s.rev) { Native.getParam(P.BPM, 0).roundToInt() }
    fun setBpm(v: Int) { Native.beginEdit(); Native.setParam(P.BPM, 0, v.coerceIn(40, 240).toFloat()); s.rev++ }
    val gap = Arrangement.spacedBy(5.dp)
    val all = RoundedCornerShape(9.dp)
    Column(
        Modifier.width(210.dp).fillMaxHeight().background(HwFaceplate).verticalScroll(rememberScrollState()).padding(6.dp),
        verticalArrangement = gap
    ) {
        Logo()
        SilkLabel(if (s.audioOk) s.info else "NO AUDIO", color = HwMuted, size = 8, bold = false)
        Row(horizontalArrangement = gap) {
            HwButton(if (s.playing) "STOP" else "PLAY", Modifier.weight(1f), on = s.playing, ledColor = HwGreen, showLed = true, height = 38.dp) {
                if (s.playing) Native.stopTransport() else Native.play(s.scene)
            }
            HwButton("REC", on = s.rec, ledColor = HwRed, showLed = true, height = 38.dp, pad = 8.dp) {
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
    Row(
        Modifier
            .fillMaxWidth()
            .background(HwFaceplate)
            .border(1.dp, HwLine)
            .padding(horizontal = 6.dp, vertical = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        for (i in 0 until 4) NavButton(s, i, Modifier.weight(1f))
    }
}

// ------------------------------------------------------------------ DRUMS TAB

@Composable
fun DrumPad(s: AppState, i: Int, label: String, modifier: Modifier) {
    val vel by rememberUpdatedState(s.vel())
    var hit by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(9.dp)
    val selected = s.selPad == i
    Box(
        modifier
            .height(62.dp)
            .clip(shape)
            .background(if (hit) HwPadActive else HwPadSurface)
            .border(if (selected) 2.dp else 1.dp, if (hit) HwGreen else if (selected) HwOrange else Color(0xFF4A4840), shape)
            .pointerInput(i) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)      // sound on touch-down, not on release
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
                .background(if (hit) HwGreen else if (selected) HwRed else Color(0xFF6B4A4A))
                .align(Alignment.TopStart)
        )
        SilkLabel(label.uppercase(), color = HwPadInk, size = 11, modifier = Modifier.align(Alignment.BottomCenter))
    }
}

@Composable
fun BarPicker(bars: Int, current: Int, playingBar: Int, onPick: (Int) -> Unit) {
    if (bars <= 1) return
    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        for (b in 0 until bars) {
            HwButton(
                text = "B${b + 1}",
                modifier = Modifier.weight(1f),
                on = b == current,
                ledColor = if (b == playingBar) HwGreen else HwOrange,
                height = 28.dp,
                pad = 1.dp
            ) { onPick(b) }
        }
    }
}

@Composable
fun DrumsTab(s: AppState) {
    val grid = remember(s.rev, s.scene) { Native.drumGrid(s.scene) }
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }
    val bar = s.drumBar.coerceIn(0, bars - 1)
    // The drum engine is part of the project in the engine, so it survives restarts and follows undo.
    val drumEng = remember(s.rev) { Native.getParam(P.DRUM_ENGINE, 0).roundToInt().coerceIn(0, DRUM_ENGINE_NAMES.size - 1) }
    val padSynth = remember(s.rev, s.selPad) { Native.padIsSynth(s.selPad) }
    val jungle = drumEng == 4
    val ownBreak = remember(s.rev) { Native.hasOwnBreak() }
    val revOn = remember(s.rev, s.selPad) { Native.getParam(P.PAD_REVERSE, s.selPad) > 0.5f }
    val peaks = remember(s.rev, s.selPad) { Native.padWave(s.selPad, 96) }
    val slice0 = remember(s.rev, s.selPad) { Native.getParam(P.PAD_SLICE_START, s.selPad) }
    val slice1 = remember(s.rev, s.selPad) { Native.getParam(P.PAD_SLICE_END, s.selPad) }
    val accent = if (jungle) HwCyan else HwOrange
    val padLabel: (Int) -> String = { i -> if (jungle) "Slice ${i + 1}" else PAD_NAMES[i] }
    fun bounce() {
        s.drumMsg = if (Native.resampleSceneToJungle(s.scene)) "" else "Nothing to bounce: the scene needs drum steps and must be shorter than 24 s."
        s.rev++
    }

    val rollLabel = when (s.drumRollMode) { 3 -> "ROLL 2X"; 4 -> "ROLL 3X"; 5 -> "ROLL 4X"; else -> "ROLL" }
    Panes(s, left = {
    LibraryPanel(s, "groove", "GROOVE LIBRARY", "Replaces the patterns of scene ${SCENE_NAMES[s.scene]}, brings its sounds.", HwGreen)
    LibraryPanel(s, "drumkit", "DRUM KIT LIBRARY", "Sets kit, pads and drum effects. Patterns stay.", HwOrange)
    ModulePanel(
        title = if (jungle) "Break slicer: ${padLabel(s.selPad)}" else "${DRUM_ENGINE_NAMES[drumEng]}: ${padLabel(s.selPad)}",
        accent = accent
    ) {
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("KIT", size = 9, modifier = Modifier.width(26.dp))
            DRUM_ENGINE_NAMES.forEachIndexed { idx, nm ->
                HwButton(nm, on = drumEng == idx, ledColor = if (idx == 4) HwCyan else HwOrange, height = 28.dp, pad = 6.dp) {
                    if (drumEng != idx) { Native.setDrumEngine(idx); s.drumMsg = ""; s.rev++ }
                }
            }
        }
        WaveformDisplay(peaks, slice0, slice1, accent, Modifier.fillMaxWidth())
        if (jungle) {
            // The slicer plays the built-in break, a WAV of your own, or this scene's drums bounced with their effects.
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalAlignment = Alignment.CenterVertically) {
                HwButton("BUILT-IN", Modifier.weight(1f), on = !ownBreak, ledColor = HwCyan, height = 30.dp, pad = 2.dp) {
                    if (ownBreak) { Native.setDrumEngine(4); s.drumMsg = ""; s.rev++ }
                }
                HwButton("LOAD WAV", Modifier.weight(1f), ledColor = HwCyan, height = 30.dp, pad = 2.dp) { s.onLoadBreak() }
                HwButton("BOUNCE ${SCENE_NAMES[s.scene]}", Modifier.weight(1f), ledColor = HwYellow, height = 30.dp, pad = 2.dp) { bounce() }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Repitch", P.JUNGLE_PITCH, 0, -12f, 12f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { signed(it, "st") }
                RotaryKnob(s, "Tight", P.JUNGLE_TIGHT, 0, 0.08f, 1.5f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { secs(it) }
                RotaryKnob(s, "Crunch", P.JUNGLE_CRUNCH, 0, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
            }
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                // Hardness drives the whole drum track: clean on the left, brutal on the right.
                RotaryKnob(s, "Hardness", P.KIT_HARDNESS, 0, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
                Column(Modifier.weight(2f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    HwButton(if (padSynth) "DRUM SYNTH: ON" else "DRUM SYNTH", Modifier.fillMaxWidth(), on = s.showDrumSynth, ledColor = HwYellow, showLed = padSynth, height = 34.dp) {
                        s.showDrumSynth = !s.showDrumSynth
                    }
                    HwButton("BOUNCE LOOP TO SLICER", Modifier.fillMaxWidth(), ledColor = HwCyan, height = 34.dp) { bounce() }
                }
            }
        }
        if (s.drumMsg.isNotEmpty()) SilkLabel(s.drumMsg, color = HwYellow, size = 10, bold = false)
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            RotaryKnob(s, "Vol", P.PAD_VOL, s.selPad, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            RotaryKnob(s, "Pitch", P.PAD_PITCH, s.selPad, -12f, 12f, Modifier.weight(1f)) { signed(it, "st") }
            RotaryKnob(s, "Decay", P.PAD_DECAY, s.selPad, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
            RotaryKnob(s, "Pan", P.PAD_PAN, s.selPad, -1f, 1f, Modifier.weight(1f)) {
                if (it < -0.02f) "L${(it * -100).roundToInt()}" else if (it > 0.02f) "R${(it * 100).roundToInt()}" else "CTR"
            }
        }
        if (jungle) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                RotaryKnob(s, "Slice S", P.PAD_SLICE_START, s.selPad, 0f, 0.95f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                RotaryKnob(s, "Slice E", P.PAD_SLICE_END, s.selPad, 0.05f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                Column(Modifier.weight(2f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    HwButton(if (revOn) "REVERSE ON" else "REVERSE", Modifier.fillMaxWidth(), on = revOn, ledColor = HwCyan, showLed = true, height = 38.dp) {
                        Native.beginEdit(); Native.setParam(P.PAD_REVERSE, s.selPad, if (revOn) 0f else 1f); s.rev++
                    }
                    // Writes the eight slices in their original order into this scene: the break plays straight away.
                    HwButton("WRITE BREAK", Modifier.fillMaxWidth(), ledColor = HwCyan, height = 38.dp) {
                        Native.writeBreakPattern(s.scene); s.rev++
                    }
                }
            }
        } else {
            Choice(s, "Choke", P.PAD_CHOKE, s.selPad, listOf("Off", "Grp 1", "Grp 2"), accent = HwOrange)
        }
        for (row in 0 until 2) Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            for (col in 0 until 4) DrumPad(s, row * 4 + col, if (jungle) "S${row * 4 + col + 1}" else PAD_NAMES[row * 4 + col], Modifier.weight(1f))
        }
    }

    if (s.showDrumSynth && !jungle) {
        // One pad, synthesized from scratch. Turning any control switches the pad over; lifting the finger plays it.
        val pad = s.selPad
        ModulePanel(
            title = "Drum synth: ${padLabel(pad)}",
            accent = HwYellow,
            trailing = {
                HwButton(if (padSynth) "USE KIT SOUND" else "KIT SOUND", enabled = padSynth, ledColor = HwOrange, height = 26.dp, pad = 8.dp) {
                    Native.clearCustomPad(pad); s.rev++; Native.triggerPad(pad, 2)
                }
            }
        ) {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel("START", size = 9, modifier = Modifier.width(38.dp))
                DSYN_TEMPLATES.forEachIndexed { idx, nm ->
                    HwButton(nm, ledColor = HwOrange, height = 28.dp, pad = 8.dp) { Native.applyDrumSynthTemplate(pad, idx); s.rev++ }
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
                dk("Drop time", P.DSYN_PDEC, 0.005f, 0.30f, Modifier.weight(1f), grey) { "${(it * 1000).roundToInt()}ms" }
                dk("Length", P.DSYN_LEN, 0.05f, 1.5f, Modifier.weight(1f), grey) { secs(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                dk("Tone", P.DSYN_TONE, 0f, 1f, Modifier.weight(1f), grey) { pct(it) }
                dk("Noise", P.DSYN_NOISE, 0f, 1f, Modifier.weight(1f), grey) { pct(it) }
                dk("Noise Hz", P.DSYN_NFREQ, 300f, 10000f, Modifier.weight(1f), grey) { "${it.roundToInt()}" }
                dk("Click", P.DSYN_CLICK, 0f, 1f, Modifier.weight(1f), grey) { pct(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                dk("FM", P.DSYN_FM, 0f, 8f, Modifier.weight(1f), blue) { "${(it * 10).roundToInt() / 10f}" }
                dk("FM ratio", P.DSYN_FMRATIO, 0.5f, 8f, Modifier.weight(1f), blue) { "${(it * 10).roundToInt() / 10f}x" }
                dk("Drive", P.DSYN_DRIVE, 0f, 1f, Modifier.weight(1f), brown) { pct(it) }
                Box(Modifier.weight(1f))
            }
            SilkLabel(if (padSynth) "This pad plays its own synthesized drum. It stays when you change the kit." else "This pad plays the kit's sound. Pick a start or turn a control to synthesize it.", color = HwMuted, size = 9, bold = false)
        }
    }
    }, right = {
    ModulePanel(
        title = "Steps: ${padLabel(s.selPad)}",
        accent = HwOrange,
        trailing = {
            HwButton(rollLabel, on = s.drumRollMode > 0, ledColor = HwCyan, height = 26.dp, pad = 6.dp) {
                s.drumRollMode = when (s.drumRollMode) { 0 -> 3; 3 -> 4; 4 -> 5; else -> 0 }
            }
            HwButton("ACCENT", on = s.accent, ledColor = HwYellow, height = 26.dp, pad = 6.dp) { s.accent = !s.accent }
        }
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(3.dp)) {
            PAD_SHORT.forEachIndexed { i, name ->
                val lbl = if (jungle) "S${i + 1}" else name
                HwButton(lbl, Modifier.weight(1f), on = s.selPad == i, ledColor = accent, height = 32.dp, pad = 1.dp) { s.selPad = i }
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
    })
}

// ------------------------------------------------------------------ SYNTH TAB (SUB + FM + PER-SYNTH ISOLATED OCTAVE)

@Composable
fun ScalePad(s: AppState, deg: Int, modifier: Modifier) {
    val vel by rememberUpdatedState(s.vel())
    val synth by rememberUpdatedState(s.synth)
    val degState by rememberUpdatedState(deg)
    val name = remember(s.rev, s.synth, deg) { noteName(Native.degreeMidi(s.synth, deg)) }
    var held by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(9.dp)
    Box(
        modifier
            .height(54.dp)
            .clip(shape)
            .background(if (held) HwCyan else HwPadSurface)
            .border(1.5.dp, if (held) Color.White else Color(0xFF4A4840), shape)
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
        SilkLabel(name, color = HwPadInk, size = 12, mono = true)
    }
}

@Composable
fun SynthTab(s: AppState) {
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }
    val notes = remember(s.rev, s.scene, s.synth) { Native.notes(s.scene, s.synth) }
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

    Panes(s, left = {
    LibraryPanel(s, "instrument", "INSTRUMENT LIBRARY", "Replaces the sound of ${TRACK_NAMES[s.synth + 1]}. Notes stay.", HwCyan)
    ModulePanel(
        title = "Synth: ${TRACK_NAMES[s.synth + 1]} (${listOf("SUB", "FM", "PLUCK", "VECTOR")[mode]})",
        accent = HwCyan,
        trailing = {
            for (idx in 0 until activeSynths) {
                HwButton(TRACK_NAMES[idx + 1].uppercase(), on = s.synth == idx, ledColor = HwCyan, height = 26.dp, pad = 5.dp) { s.synth = idx }
            }
            if (activeSynths < 4) {
                HwButton("+TRK", ledColor = HwGreen, height = 26.dp, pad = 5.dp) {
                    Native.setActiveSynths(activeSynths + 1)
                    s.synth = activeSynths
                    s.rev++
                }
            }
        }
    ) {
        // Instrument Engine & Feel Shaping
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("INST", size = 9, modifier = Modifier.width(34.dp))
            INSTRUMENT_NAMES.forEachIndexed { idx, nm ->
                HwButton(nm, on = curInst == idx, ledColor = HwYellow, height = 26.dp, pad = 6.dp) {
                    Native.applyInstrument(s.synth, idx); s.rev++
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            RotaryKnob(s, "Warmth", P.SYN_WARMTH, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
            RotaryKnob(s, "Motion", P.SYN_MOTION, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
        }
        OledScopeDisplay(
            isFm = isFm,
            wave = wave1,
            cutoffNorm = ((cutoff - 80f) / 7920f).coerceIn(0.08f, 1f),
            fmIndex = fmIndex,
            fmRatio = fmRatio
        )

        Choice(s, "Engine", P.SYN_MODE, s.synth, listOf("Subtractive", "FM 2-Op", "Pluck", "Vector"), accent = HwCyan)

        val waves = listOf("Saw", "Sqr", "Tri", "Sin")
        if (mode == 2) {
            // a plucked string: Ring is how long it sounds, Bright how hard it is plucked
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Bright", P.SYN_PLUCK_BRIGHT, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
                RotaryKnob(s, "Ring", P.SYN_DECAY, s.synth, 0.03f, 1.5f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { secs(it * 4f) }
                RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
            }
        } else if (mode == 3) {
            val w1 = waves[wave1.coerceIn(0, 3)]
            val w2 = waves[remember(s.rev, s.synth) { Native.getParam(P.SYN_WAVE2, s.synth).roundToInt() }.coerceIn(0, 3)]
            VectorPad(s, s.synth, listOf("OSC 1 $w1", "OSC 2 $w2", "FM", "SUB"))
            Choice(s, "Osc 1", P.SYN_WAVE1, s.synth, waves, accent = HwCyan)
            Choice(s, "Osc 2", P.SYN_WAVE2, s.synth, waves, accent = HwCyan)
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
            Choice(s, "Osc 1", P.SYN_WAVE1, s.synth, waves, accent = HwCyan)
            Choice(s, "Osc 2", P.SYN_WAVE2, s.synth, waves, accent = HwCyan)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                RotaryKnob(s, "Res", P.SYN_RES, s.synth, 0.5f, 15f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
                RotaryKnob(s, "Detune", P.SYN_DETUNE, s.synth, -30f, 30f, Modifier.weight(1f), modulated = lfoTarget == 2) { signed(it, "c") }
                RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
            }
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Ratio", P.SYN_FM_RATIO, s.synth, 0.5f, 8f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { "${(it * 2).roundToInt() / 2f}x" }
                RotaryKnob(s, "Index", P.SYN_FM_INDEX, s.synth, 0f, 8f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { "${(it * 10).roundToInt() / 10f}" }
                RotaryKnob(s, "ModDec", P.SYN_FM_DECAY, s.synth, 0.01f, 2f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { secs(it) }
                RotaryKnob(s, "Fdbk", P.SYN_FM_FB, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847)) { pct(it) }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                RotaryKnob(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f), modulated = lfoTarget == 1) { "${it.roundToInt()}" }
                RotaryKnob(s, "Res", P.SYN_RES, s.synth, 0.5f, 15f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
                RotaryKnob(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f), modulated = lfoTarget == 3) { pct(it) }
            }
        }

        // Envelope
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            RotaryKnob(s, "Attack", P.SYN_ATTACK, s.synth, 0.002f, 0.4f, Modifier.weight(1f)) { "${(it * 1000).roundToInt()}ms" }
            RotaryKnob(s, "Decay", P.SYN_DECAY, s.synth, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
            RotaryKnob(s, "Sustain", P.SYN_SUSTAIN, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            RotaryKnob(s, "Release", P.SYN_RELEASE, s.synth, 0.01f, 1.5f, Modifier.weight(1f)) { secs(it) }
        }
        // LFO
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            RotaryKnob(s, "LFO Hz", P.SYN_LFO_RATE, s.synth, 0.1f, 12f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
            RotaryKnob(s, "LFO Dep", P.SYN_LFO_DEPTH, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            // Fold bends the wave back on itself before the filter. Slide is how long a slide note glides.
            RotaryKnob(s, "Fold", P.SYN_FOLD, s.synth, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { pct(it) }
            RotaryKnob(s, "Slide", P.SYN_SLIDE_TIME, s.synth, 0.01f, 0.4f, Modifier.weight(1f), capColor = Color(0xFF422A1A)) { "${(it * 1000).roundToInt()}ms" }
        }
        Choice(s, "LFO ->", P.SYN_LFO_TARGET, s.synth, listOf("Off", "Cutoff", "Pitch", "Amp"), accent = HwCyan)
    }

    }, right = {
    // Playable Scale Pads / Mini Piano Roll with ISOLATED per-synth octave switching
    ModulePanel(
        title = "Play ${TRACK_NAMES[s.synth + 1]} • OCT ${if (octVal >= 0) "+$octVal" else "$octVal"}",
        accent = HwCyan,
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
            HwButton(if (chord) "CHORD ON" else "CHORD", Modifier.weight(1f), on = chord, ledColor = HwYellow, height = 30.dp) {
                Native.beginEdit(); Native.setParam(P.SYN_CHORD, s.synth, if (chord) 0f else 1f); s.rev++
            }
            HwButton("ACCENT", Modifier.weight(1f), on = s.accent, ledColor = HwOrange, height = 30.dp, pad = 2.dp) { s.accent = !s.accent }
            HwButton("SLIDE", Modifier.weight(1f), on = s.slide, ledColor = HwYellow, height = 30.dp, pad = 2.dp) { s.slide = !s.slide; if (s.slide) s.rollMode = true }
            HwButton(if (s.rollMode) "PADS" else "ROLL", Modifier.weight(1f), on = s.rollMode, ledColor = HwCyan, height = 30.dp, pad = 2.dp) { s.rollMode = !s.rollMode }
        }
        if (!s.rollMode) {
            for (row in 1 downTo 0) Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for (col in 0 until 4) ScalePad(s, degOffset + row * 4 + col, Modifier.weight(1f))
            }
        } else {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (pg in 0 until bars * 2) {
                    val playingHere = s.playing && s.step / 8 == pg
                    HwButton("${pg / 2 + 1}" + (if (pg % 2 == 0) "a" else "b"), on = pg == page, ledColor = if (playingHere) HwGreen else HwCyan, height = 28.dp, pad = 7.dp) { s.rollPage = pg }
                }
            }
            for (i in 7 downTo 0) {
                val deg = degOffset + i
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                    val name = remember(s.rev, s.synth, deg) { noteName(Native.degreeMidi(s.synth, deg)) }
                    SilkLabel(name, color = HwSilk, size = 9, mono = true, modifier = Modifier.width(28.dp))
                    for (col in 0 until 8) {
                        val st = page * 8 + col
                        var v = 0
                        var sl = false
                        var k = 0
                        while (k + 4 < notes.size) { if (notes[k] == st && notes[k + 1] == deg) { v = notes[k + 3]; sl = (notes[k + 4] and 1) != 0 }; k += 5 }
                        RollCell(v, s.playing && s.step == st, col % 4 == 0, HwCyan, Modifier.weight(1f).height(28.dp), slide = sl) {
                            if (Native.toggleNote(s.scene, s.synth, st, deg, s.accent, s.slide) && !s.slide) {
                                Native.noteOn(s.synth, deg, s.vel()); Native.noteOff(s.synth, deg)
                            }
                            s.rev++
                        }
                    }
                }
            }
        }
        SilkLabel(if (s.slide) "SLIDE is on: tap a note to make it glide in from the note before (yellow bar). Tap again to undo it." else "OCT-/+ shifts only new/edited notes for ${TRACK_NAMES[s.synth + 1]} without changing other scenes.", color = if (s.slide) HwYellow else HwMuted, size = 9, bold = false)
    }
    })
}

// ------------------------------------------------------------------ FX & MASTER TAB

@Composable
fun MixTab(s: AppState) {
    val activeTracks = 1 + remember(s.rev) { Native.getParam(P.ACTIVE_SYNTHS, 0).roundToInt().coerceIn(2, 4) }
    val t = s.fxTrack.coerceIn(0, activeTracks - 1)
    val mute = remember(s.rev, t) { Native.getParam(P.FX_MUTE, t) > 0.5f }

    Panes(s, left = {
    ModulePanel(
        title = "Channel Strip: ${TRACK_NAMES[t]}",
        accent = HwYellow,
        trailing = {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (i in 0 until activeTracks) {
                    HwButton(TRACK_NAMES[i].uppercase(), on = t == i, ledColor = HwYellow, height = 26.dp, pad = 5.dp) { s.fxTrack = i }
                }
            }
        }
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            RotaryKnob(s, "Level", P.FX_LEVEL, t, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            RotaryKnob(s, "Drive", P.FX_DRIVE, t, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            RotaryKnob(s, "Low EQ", P.FX_EQ_LOW, t, -12f, 12f, Modifier.weight(1f)) { signed(it, "dB") }
            RotaryKnob(s, "High EQ", P.FX_EQ_HIGH, t, -12f, 12f, Modifier.weight(1f)) { signed(it, "dB") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            RotaryKnob(s, "Dly Snd", P.FX_DELAY, t, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
            RotaryKnob(s, "Rev Snd", P.FX_REVERB, t, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
            HwButton(if (mute) "MUTED" else "MUTE", Modifier.weight(1f), on = mute, ledColor = HwRed, showLed = true, height = 46.dp) {
                Native.beginEdit(); Native.setParam(P.FX_MUTE, t, if (mute) 0f else 1f); s.rev++
            }
        }
    }

    }, right = {
    // Vital-style 3-Slot Modular FX Chain per track
    ModulePanel("Modular FX Chain: ${TRACK_NAMES[t]} (Slot 1 → 2 → 3)", accent = HwCyan) {
        for (slot in 0 until 3) {
            val fd = remember(s.rev, t, slot) { Native.fxSlot(t, slot) }
            val fxType = fd.getOrElse(0) { 0f }.roundToInt()
            val fxAmt = fd.getOrElse(1) { 0.5f }
            val fxRate = fd.getOrElse(2) { 0.4f }
            val fxBypass = fd.getOrElse(3) { 0f } > 0.5f
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                SilkLabel("FX${slot + 1}", color = HwCyan, size = 9, modifier = Modifier.width(26.dp))
                Row(Modifier.weight(1f).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(3.dp)) {
                    FX_SLOT_NAMES.forEachIndexed { idx, nm ->
                        HwButton(nm, on = fxType == idx, ledColor = HwCyan, height = 26.dp, pad = 6.dp) {
                            Native.setFxSlot(t, slot, idx, fxAmt, fxRate, false); s.rev++
                        }
                    }
                }
            }
            if (fxType > 0) {
                val rateName = when (fxType) { 2 -> "Rate"; 3, 4 -> "Speed"; 5 -> "Reso"; else -> "" }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Knob(if (fxType == 5) "Cutoff" else "Amount", fxAmt, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847), fmt = { pct(it) },
                        onStart = { Native.beginEdit() },
                        onChange = { Native.setFxSlotLive(t, slot, fxType, it, fxRate, fxBypass) },
                        onEnd = { s.rev++ })
                    if (rateName.isNotEmpty()) {
                        Knob(rateName, fxRate, 0f, 1f, Modifier.weight(1f), capColor = Color(0xFF1F3847), fmt = { pct(it) },
                            onStart = { Native.beginEdit() },
                            onChange = { Native.setFxSlotLive(t, slot, fxType, fxAmt, it, fxBypass) },
                            onEnd = { s.rev++ })
                    } else {
                        Box(Modifier.weight(1f))
                    }
                    HwButton(if (fxBypass) "BYPASSED" else "ON", Modifier.weight(1f), on = !fxBypass, ledColor = HwGreen, showLed = true, height = 38.dp, pad = 2.dp) {
                        Native.setFxSlot(t, slot, fxType, fxAmt, fxRate, !fxBypass); s.rev++
                    }
                }
            }
        }
    }

    ModulePanel("Master Bus & Analog VU", accent = HwOrange) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
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

    data class AutoChoice(val id: Int, val label: String, val lo: Float, val hi: Float)
    val paramChoices = if (autoTrk == 0) {
        listOf(
            AutoChoice(-1, "Off", 0f, 0f),
            AutoChoice(P.FX_LEVEL, "Level", 0.1f, 1f),
            AutoChoice(P.FX_DRIVE, "Drive", 0f, 0.95f),
            AutoChoice(P.FX_EQ_LOW, "Low EQ", -10f, 10f),
            AutoChoice(P.FX_EQ_HIGH, "High EQ", -10f, 10f),
            AutoChoice(P.FX_DELAY, "Delay", 0f, 0.75f),
            AutoChoice(P.FX_REVERB, "Reverb", 0f, 0.75f)
        )
    } else {
        listOf(
            AutoChoice(-1, "Off", 0f, 0f),
            AutoChoice(P.SYN_CUTOFF, "Cutoff", 140f, 7500f),
            AutoChoice(P.SYN_RES, "Res", 0.8f, 12f),
            AutoChoice(P.SYN_FM_INDEX, "FM Idx", 0.2f, 7.5f),
            AutoChoice(P.SYN_WARMTH, "Warmth", 0f, 1f),
            AutoChoice(P.SYN_MOTION, "Motion", 0f, 1f),
            AutoChoice(P.SYN_LFO_DEPTH, "LFO Dep", 0f, 1f),
            AutoChoice(P.SYN_FOLD, "Fold", 0f, 1f),
            AutoChoice(P.SYN_VEC_X, "Vector X", 0f, 1f),
            AutoChoice(P.SYN_VEC_Y, "Vector Y", 0f, 1f),
            AutoChoice(P.SYN_PLUCK_BRIGHT, "Pluck Brt", 0f, 1f),
            AutoChoice(P.FX_DRIVE, "Drive", 0f, 0.95f),
            AutoChoice(P.FX_DELAY, "Delay", 0f, 0.75f),
            AutoChoice(P.FX_REVERB, "Reverb", 0f, 0.75f),
            AutoChoice(P.FX_LEVEL, "Level", 0.1f, 1f)
        )
    }

    // What the control is set to right now: synth controls belong to the synth, the rest to the track's channel.
    fun nowValue(ch: AutoChoice): Float =
        Native.getParam(ch.id, if (ch.id in 20..39 || ch.id >= 50) autoTrk - 1 else autoTrk).coerceIn(ch.lo, ch.hi)

    Panes(s, left = {
    LibraryPanel(s, "groove", "GROOVE LIBRARY", "Replaces the patterns of scene ${SCENE_NAMES[s.scene]}, brings its sounds.", HwGreen)
    ModulePanel(
        title = "Arrangement Chain",
        accent = HwGreen,
        trailing = {
            HwButton(if (s.songMode) "SONG ON" else "SONG OFF", on = s.songMode, ledColor = HwGreen, showLed = true, height = 28.dp) {
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
                    ledColor = if (playingHere) HwGreen else HwCyan,
                    height = 34.dp
                ) { s.selChainPos = pos }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("ADD", size = 9)
            SCENE_NAMES.forEachIndexed { i, name ->
                HwButton("+ $name", Modifier.weight(1f), height = 30.dp) { Native.chainAdd(i); s.rev++ }
            }
            HwButton("DEL", enabled = chain.size > 1, ledColor = HwRed, height = 30.dp, pad = 8.dp) {
                Native.chainRemove(selPos)
                s.selChainPos = (selPos - 1).coerceAtLeast(0)
                s.rev++
            }
        }
    }

    // Free-Choice Song-Menu Automation (up to 3 lanes per track!)
    ModulePanel(
        title = "Slot #${selPos + 1} Free Automation (3 / Track)",
        accent = HwOrange
    ) {
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            for (t in 0 until activeTracks) {
                HwButton(TRACK_NAMES[t].uppercase(), on = autoTrk == t, ledColor = HwCyan, height = 26.dp, pad = 6.dp) { s.selAutoTrack = t }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            for (ln in 0 until 3) {
                val activeLn = autoData.getOrElse(ln * 3) { -1f } >= 0f
                HwButton(
                    text = "LANE ${ln + 1}" + if (activeLn) " ●" else "",
                    modifier = Modifier.weight(1f),
                    on = autoLn == ln,
                    ledColor = HwOrange,
                    height = 28.dp
                ) { s.selAutoLane = ln }
            }
        }
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            paramChoices.forEach { ch ->
                HwButton(ch.label, on = curPid == ch.id, ledColor = HwYellow, height = 28.dp, pad = 7.dp) {
                    if (ch.id < 0) {
                        Native.setChainAuto(selPos, autoTrk, autoLn, -1, 0f, 0f)
                    } else {
                        // starts where the control stands now and moves to the far end of its range
                        val start = nowValue(ch)
                        val end = if (start - ch.lo < (ch.hi - ch.lo) * 0.5f) ch.hi else ch.lo
                        Native.setChainAuto(selPos, autoTrk, autoLn, ch.id, start, end)
                    }
                    s.rev++
                }
            }
        }
        val activeChoice = paramChoices.firstOrNull { it.id == curPid }
        if (activeChoice != null && curPid >= 0) {
            val show: (Float) -> String = { v -> if (activeChoice.hi - activeChoice.lo > 50f) "${v.roundToInt()}" else "%.2f".format(v) }
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                Knob("Start", curStart, activeChoice.lo, activeChoice.hi, Modifier.weight(1f), capColor = Color(0xFF1F3847), fmt = show,
                    onStart = { Native.beginEdit() },
                    onChange = { Native.setChainAutoLive(selPos, autoTrk, autoLn, curPid, it, curEnd) },
                    onEnd = { s.rev++ })
                Knob("End", curEnd, activeChoice.lo, activeChoice.hi, Modifier.weight(1f), capColor = Color(0xFF422A1A), fmt = show,
                    onStart = { Native.beginEdit() },
                    onChange = { Native.setChainAutoLive(selPos, autoTrk, autoLn, curPid, curStart, it) },
                    onEnd = { s.rev++ })
                Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    HwButton("START = NOW", Modifier.fillMaxWidth(), ledColor = HwCyan, height = 28.dp, pad = 2.dp) {
                        Native.setChainAuto(selPos, autoTrk, autoLn, curPid, nowValue(activeChoice), curEnd); s.rev++
                    }
                    HwButton("SWAP", Modifier.fillMaxWidth(), ledColor = HwYellow, height = 28.dp, pad = 2.dp) {
                        Native.setChainAuto(selPos, autoTrk, autoLn, curPid, curEnd, curStart); s.rev++
                    }
                    HwButton("CLEAR", Modifier.fillMaxWidth(), height = 28.dp, pad = 2.dp) {
                        Native.setChainAuto(selPos, autoTrk, autoLn, -1, 0f, 0f); s.rev++
                    }
                }
            }
            SilkLabel("While slot #${selPos + 1} plays, ${activeChoice.label} moves from Start to End.", color = HwMuted, size = 9, bold = false)
        } else {
            SilkLabel("Pick any parameter above to automate on ${TRACK_NAMES[autoTrk]} (Lane ${autoLn + 1}).", color = HwMuted, size = 9, bold = false)
        }
    }

    }, right = {
    ModulePanel("Mixer", accent = HwGreen) {
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            for (t in 0 until activeTracks) {
                Column(Modifier.width(74.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    val mute = remember(s.rev, t) { Native.getParam(P.FX_MUTE, t) > 0.5f }
                    RotaryKnob(s, TRACK_NAMES[t], P.FX_LEVEL, t, 0f, 1f, Modifier.fillMaxWidth()) { pct(it) }
                    HwButton(if (mute) "MUTED" else "MUTE", Modifier.fillMaxWidth(), on = mute, ledColor = HwRed, height = 28.dp, pad = 2.dp) {
                        Native.beginEdit(); Native.setParam(P.FX_MUTE, t, if (mute) 0f else 1f); s.rev++
                    }
                }
            }
            RotaryKnob(s, "Master", P.MASTER, 0, 0f, 1f, Modifier.width(74.dp)) { pct(it) }
        }
    }

    ModulePanel("Projects", accent = HwCyan) {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            BasicTextField(
                value = s.projectName,
                onValueChange = { s.projectName = it.take(32).filter { c -> c.isLetterOrDigit() || c in " -_" } },
                singleLine = true,
                textStyle = TextStyle(color = HwSilk, fontSize = 14.sp),
                cursorBrush = SolidColor(HwCyan),
                modifier = Modifier.weight(1f).clip(RoundedCornerShape(6.dp)).background(HwRecess).border(1.dp, HwLine, RoundedCornerShape(6.dp)).padding(horizontal = 10.dp, vertical = 9.dp)
            )
            HwButton("SAVE", ledColor = HwGreen, height = 34.dp, pad = 12.dp) { s.onSaveProject(s.projectName) }
        }
        if (s.projectMsg.isNotEmpty()) SilkLabel(s.projectMsg, color = HwYellow, size = 10, bold = false)
        if (s.projects.isEmpty()) SilkLabel("Nothing saved yet. The open project is also kept automatically when you leave the app.", color = HwMuted, size = 9, bold = false)
        for (nm in s.projects) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                SilkLabel(nm, color = HwSilk, size = 12, modifier = Modifier.weight(1f))
                HwButton("OPEN", ledColor = HwCyan, height = 30.dp, pad = 9.dp) { s.onOpenProject(nm) }
                val sure = s.confirmDelete == nm
                HwButton(if (sure) "SURE?" else "DEL", on = sure, ledColor = HwRed, height = 30.dp, pad = 9.dp) {
                    if (sure) { s.confirmDelete = ""; s.onDeleteProject(nm) } else s.confirmDelete = nm
                }
            }
        }
    }

    ModulePanel("Export for your DAW", accent = HwSilk) {
        SilkLabel(
            if (s.songMode) "Exports the whole song chain." else "Exports scene ${SCENE_NAMES[s.scene]}. Switch SONG on to export the chain.",
            color = HwMuted, size = 9, bold = false
        )
        Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            HwButton("EXPORT .MID", Modifier.weight(1f), enabled = !s.exporting, height = 38.dp, pad = 2.dp) { s.onExport(0) }
            HwButton("RENDER .WAV", Modifier.weight(1f), enabled = !s.exporting, height = 38.dp, pad = 2.dp) { s.onExport(1) }
            HwButton("STEMS", Modifier.weight(0.7f), enabled = !s.exporting, height = 38.dp, pad = 2.dp) { s.onExport(2) }
        }
        if (s.exportMsg.isNotEmpty()) SilkLabel(s.exportMsg, color = HwYellow, size = 10)
    }

    ModulePanel("Scene ${SCENE_LABELS[s.scene]}", accent = HwCyan) {
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("BARS", size = 9, modifier = Modifier.width(38.dp))
            for (b in 1..8) HwButton("$b", Modifier.weight(1f), on = bars == b, ledColor = HwCyan, height = 30.dp, pad = 1.dp) {
                Native.setSceneBars(s.scene, b); s.rev++
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
            SilkLabel("COPY", size = 9, modifier = Modifier.width(38.dp))
            SCENE_NAMES.forEachIndexed { i, name ->
                HwButton("→ $name", Modifier.weight(1f), enabled = i != s.scene, height = 30.dp) {
                    Native.copyScene(s.scene, i); s.rev++
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            HwButton("CLEAR SCENE", Modifier.weight(1f), height = 32.dp) { Native.clearScene(s.scene); s.rev++ }
            HwButton("RESET DEMO", Modifier.weight(1f), height = 32.dp) { Native.resetToDemo(); s.rev++ }
        }
        // Replaces the whole project with the built-in preset. UNDO brings the old project back.
        HwButton("LOAD PRESET: WAREHOUSE 303", Modifier.fillMaxWidth(), ledColor = HwGreen, height = 32.dp) {
            Native.loadStartPreset(); s.scene = 0; s.drumBar = 0; s.rollPage = 0; s.rev++
        }
    }

    ModulePanel("Key & Scale", accent = HwYellow) {
        Choice(s, "Root", P.ROOT, 0, NOTE_NAMES, accent = HwYellow)
        Choice(s, "Scale", P.SCALE, 0, listOf("Minor", "Major", "Dorian", "Penta"), accent = HwYellow)
        Choice(s, "Base Oct", P.OCTAVE, 0, listOf("Low", "Mid", "High"), offset = 2, accent = HwYellow)
    }
    })
}
