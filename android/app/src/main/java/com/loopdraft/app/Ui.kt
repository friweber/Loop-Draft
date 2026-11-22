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
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

// ---- colours (same palette as the HTML prototype) ----
val Bg = Color(0xFF0F1117)
val Panel = Color(0xFF181C26)
val Panel2 = Color(0xFF202636)
val Line = Color(0xFF2E364D)
val Ink = Color(0xFFE8ECF7)
val Muted = Color(0xFF8B95B0)
val Accent = Color(0xFF5B8CFF)
val Orange = Color(0xFFFF6B4A)
val Green = Color(0xFF2EE59D)
val Yellow = Color(0xFFFFD166)
val Purple = Color(0xFFB388FF)
val Danger = Color(0xFFFF4D6D)
val CellOff = Color(0xFF141824)

val PAD_NAMES = listOf("Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc")
val PAD_SHORT = listOf("Kik", "Snr", "Clp", "CH", "OH", "Rim", "Tom", "Prc")
val SCENE_NAMES = listOf("A", "B", "C", "D")
val TRACK_NAMES = listOf("Drums", "Bass", "Lead")
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
}

@Composable
fun App(s: AppState) {
    LaunchedEffect(Unit) {
        while (true) {
            withFrameNanos { }
            s.frame()
        }
    }
    MaterialTheme(colorScheme = darkColorScheme(primary = Accent, background = Bg, surface = Panel, onSurface = Ink, onBackground = Ink)) {
        Column(Modifier.fillMaxSize().background(Bg).safeDrawingPadding()) {
            TopBar(s)
            Column(
                Modifier.weight(1f).fillMaxWidth().verticalScroll(rememberScrollState()).padding(10.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                when (s.tab) {
                    0 -> DrumsTab(s)
                    1 -> SynthTab(s)
                    2 -> MixTab(s)
                    else -> SongTab(s)
                }
            }
            SceneStrip(s)
            Transport(s)
            BottomNav(s)
        }
    }
}

// ------------------------------------------------------------------ small parts

@Composable
fun Label(text: String, color: Color = Muted, size: Int = 11, bold: Boolean = true, modifier: Modifier = Modifier) {
    Text(text, color = color, fontSize = size.sp, fontWeight = if (bold) FontWeight.Bold else FontWeight.Normal,
        maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = modifier)
}

@Composable
fun Btn(text: String, modifier: Modifier = Modifier, on: Boolean = false, onColor: Color = Accent, enabled: Boolean = true,
        height: Dp = 38.dp, pad: Dp = 10.dp, onClick: () -> Unit) {
    val shape = RoundedCornerShape(10.dp)
    Box(
        modifier.height(height).clip(shape).background(if (on) onColor else Panel2)
            .border(1.dp, if (on) onColor else Line, shape)
            .clickable(enabled = enabled, onClick = onClick).padding(horizontal = pad),
        contentAlignment = Alignment.Center
    ) {
        Label(text, color = if (!enabled) Muted.copy(alpha = 0.4f) else if (on) Color(0xFF0B1020) else Ink, size = 12)
    }
}

@Composable
fun Card(title: String, trailing: @Composable RowScope.() -> Unit = {}, content: @Composable ColumnScope.() -> Unit) {
    val shape = RoundedCornerShape(14.dp)
    Column(
        Modifier.fillMaxWidth().clip(shape).background(Panel).border(1.dp, Line, shape).padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Label(title.uppercase(), modifier = Modifier.weight(1f))
            trailing()
        }
        content()
    }
}

/** A slider bound to one engine parameter. One drag is one undo step. */
@Composable
fun ParamSlider(s: AppState, label: String, id: Int, index: Int, min: Float, max: Float, modifier: Modifier = Modifier,
                fmt: (Float) -> String) {
    var v by remember(s.rev, id, index) { mutableFloatStateOf(Native.getParam(id, index)) }
    var dragging by remember { mutableStateOf(false) }
    Column(modifier.clip(RoundedCornerShape(10.dp)).background(Color(0xFF131722)).padding(horizontal = 8.dp, vertical = 4.dp)) {
        Row {
            Label(label, size = 10, modifier = Modifier.weight(1f))
            Label(fmt(v), color = Ink, size = 10)
        }
        Slider(
            value = v.coerceIn(min, max),
            onValueChange = {
                if (!dragging) { dragging = true; Native.beginEdit() }
                v = it
                Native.setParam(id, index, it)
            },
            onValueChangeFinished = { dragging = false; s.rev++ },
            valueRange = min..max,
            colors = SliderDefaults.colors(thumbColor = Accent, activeTrackColor = Accent, inactiveTrackColor = Line),
            modifier = Modifier.height(34.dp)
        )
    }
}

/** A row of choices bound to one whole-number parameter. */
@Composable
fun Choice(s: AppState, label: String, id: Int, index: Int, options: List<String>, offset: Int = 0) {
    val cur = remember(s.rev, id, index) { Native.getParam(id, index).roundToInt() }
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Label(label, size = 10, modifier = Modifier.width(64.dp))
        Row(Modifier.weight(1f).horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            options.forEachIndexed { i, name ->
                Btn(name, on = cur == i + offset, height = 32.dp) {
                    Native.beginEdit(); Native.setParam(id, index, (i + offset).toFloat()); s.rev++
                }
            }
        }
    }
}

@Composable
fun Cell(v: Int, playhead: Boolean, beat: Boolean, color: Color, modifier: Modifier, label: String = "", onTap: () -> Unit) {
    val shape = RoundedCornerShape(7.dp)
    Box(
        modifier.clip(shape)
            .background(when (v) { 2 -> color; 1 -> color.copy(alpha = 0.55f); else -> CellOff })
            .border(if (playhead) 2.dp else 1.dp, if (playhead) Green else if (v == 2) Ink else if (beat) Color(0xFF49577A) else Line, shape)
            .clickable(onClick = onTap),
        contentAlignment = Alignment.Center
    ) {
        if (label.isNotEmpty()) Label(label, color = if (v > 0) Ink else Muted, size = 10)
    }
}

fun pct(v: Float) = "${(v * 100).roundToInt()}%"
fun signed(v: Float, unit: String) = (if (v >= 0) "+" else "") + v.roundToInt() + " " + unit
fun secs(v: Float) = "${(v * 100).roundToInt() / 100f} s"

// ------------------------------------------------------------------ frame around the tabs

@Composable
fun TopBar(s: AppState) {
    val canUndo = remember(s.rev) { Native.canUndo() }
    val canRedo = remember(s.rev) { Native.canRedo() }
    Row(
        Modifier.fillMaxWidth().background(Panel).padding(horizontal = 12.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Column(Modifier.weight(1f)) {
            Label("LOOP DRAFT", color = Ink, size = 14)
            Label(if (s.audioOk) s.info else "No audio device", size = 9, bold = false)
        }
        Btn("Undo", enabled = canUndo, height = 34.dp) { if (Native.undo()) s.rev++ }
        Btn("Redo", enabled = canRedo, height = 34.dp) { if (Native.redo()) s.rev++ }
    }
}

@Composable
fun SceneStrip(s: AppState) {
    Row(Modifier.fillMaxWidth().background(Panel).padding(start = 10.dp, end = 10.dp, top = 8.dp), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        SCENE_NAMES.forEachIndexed { i, name ->
            val bars = remember(s.rev, i) { Native.sceneBars(i) }
            Btn("$name  $bars bar" + (if (bars > 1) "s" else ""), Modifier.weight(1f),
                on = s.scene == i || s.queued == i, onColor = if (s.queued == i) Yellow else Accent, height = 40.dp) {
                if (s.playing && s.songMode) return@Btn       // the song chain decides
                Native.queueScene(i)                              // while playing: switches on the next bar line
                if (!s.playing) { s.scene = i; s.drumBar = 0; s.rollPage = 0 }
            }
        }
    }
}

@Composable
fun Transport(s: AppState) {
    val bpm = remember(s.rev) { Native.getParam(P.BPM, 0).roundToInt() }
    fun setBpm(v: Int) { Native.beginEdit(); Native.setParam(P.BPM, 0, v.coerceIn(40, 240).toFloat()); s.rev++ }
    Row(Modifier.fillMaxWidth().background(Panel).padding(10.dp), verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        Btn(if (s.playing) "STOP" else "PLAY", Modifier.weight(1f), on = s.playing, onColor = Green, height = 48.dp) {
            if (s.playing) Native.stopTransport() else Native.play(s.scene)
        }
        Btn("REC", on = s.rec, onColor = Danger, height = 48.dp) { s.rec = !s.rec; Native.setRecording(s.rec) }
        Btn("-", height = 48.dp) { setBpm(bpm - 1) }
        Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.width(44.dp)) {
            Label("$bpm", color = Yellow, size = 16)
            Label("BPM", size = 9)
        }
        Btn("+", height = 48.dp) { setBpm(bpm + 1) }
    }
}

@Composable
fun BottomNav(s: AppState) {
    Row(Modifier.fillMaxWidth().background(Panel)) {
        listOf("Drums", "Synth", "Mix", "Song").forEachIndexed { i, name ->
            Box(
                Modifier.weight(1f).height(48.dp).background(if (s.tab == i) Accent.copy(alpha = 0.12f) else Panel).clickable { s.tab = i },
                contentAlignment = Alignment.Center
            ) { Label(name, color = if (s.tab == i) Accent else Muted, size = 13) }
        }
    }
}

// ------------------------------------------------------------------ Drums

@Composable
fun DrumPad(s: AppState, i: Int, modifier: Modifier) {
    val vel by rememberUpdatedState(s.vel())
    var hit by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(12.dp)
    Box(
        modifier.height(64.dp).clip(shape).background(if (hit) Color(0xFF3C5491) else Panel2)
            .border(1.5.dp, if (hit) Green else if (s.selPad == i) Accent else Line, shape)
            .pointerInput(i) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)      // sound on touch-down, not on release
                    Native.triggerPad(i, vel)
                    s.selPad = i
                    hit = true
                    waitForUpOrCancellation()
                    hit = false
                }
            },
        contentAlignment = Alignment.Center
    ) { Label(PAD_NAMES[i], color = Ink, size = 13) }
}

@Composable
fun BarPicker(bars: Int, current: Int, playingBar: Int, onPick: (Int) -> Unit) {
    if (bars <= 1) return
    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        for (b in 0 until bars) {
            Btn("${b + 1}", Modifier.weight(1f), on = b == current, onColor = if (b == playingBar) Green else Accent, height = 32.dp, pad = 1.dp) { onPick(b) }
        }
    }
}

@Composable
fun DrumsTab(s: AppState) {
    val grid = remember(s.rev, s.scene) { Native.drumGrid(s.scene) }
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }
    val bar = s.drumBar.coerceIn(0, bars - 1)

    Card("Drum pads") {
        for (row in 0 until 2) Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            for (col in 0 until 4) DrumPad(s, row * 4 + col, Modifier.weight(1f))
        }
    }
    Card("Steps: ${PAD_NAMES[s.selPad]}", trailing = { Btn("Accent", on = s.accent, onColor = Yellow, height = 30.dp) { s.accent = !s.accent } }) {
        Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {      // pick a lane without making a sound
            PAD_SHORT.forEachIndexed { i, name ->
                Btn(name, Modifier.weight(1f), on = s.selPad == i, height = 34.dp, pad = 1.dp) { s.selPad = i }
            }
        }
        BarPicker(bars, bar, if (s.playing) s.step / 16 else -1) { s.drumBar = it }
        for (row in 0 until 2) Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            for (col in 0 until 8) {
                val st = bar * 16 + row * 8 + col
                Cell(grid[s.selPad * 128 + st].toInt(), s.playing && s.step == st, col % 4 == 0, Orange,
                    Modifier.weight(1f).height(44.dp), label = "${row * 8 + col + 1}") {
                    Native.toggleStep(s.scene, s.selPad, st, s.accent); s.rev++
                }
            }
        }
    }
    Card("Pad: ${PAD_NAMES[s.selPad]}") {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Volume", P.PAD_VOL, s.selPad, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            ParamSlider(s, "Decay", P.PAD_DECAY, s.selPad, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Pitch", P.PAD_PITCH, s.selPad, -12f, 12f, Modifier.weight(1f)) { signed(it, "st") }
            ParamSlider(s, "Pan", P.PAD_PAN, s.selPad, -1f, 1f, Modifier.weight(1f)) { if (it < -0.02f) "L ${pct(-it)}" else if (it > 0.02f) "R ${pct(it)}" else "Centre" }
        }
        Choice(s, "Choke", P.PAD_CHOKE, s.selPad, listOf("Off", "Group 1", "Group 2"))
    }
}

// ------------------------------------------------------------------ Synth

@Composable
fun ScalePad(s: AppState, deg: Int, modifier: Modifier) {
    val vel by rememberUpdatedState(s.vel())
    val synth by rememberUpdatedState(s.synth)
    val name = remember(s.rev, s.synth, deg) { noteName(Native.degreeMidi(s.synth, deg)) }
    var held by remember { mutableStateOf(false) }
    val shape = RoundedCornerShape(10.dp)
    Box(
        modifier.height(60.dp).clip(shape).background(if (held) Accent else Panel2).border(1.dp, Line, shape)
            .pointerInput(deg) {
                awaitEachGesture {
                    awaitFirstDown(requireUnconsumed = false)
                    val track = synth
                    Native.noteOn(track, deg, vel)
                    held = true
                    waitForUpOrCancellation()
                    Native.noteOff(track, deg)                      // the note sounds for as long as the pad is held
                    held = false
                }
            },
        contentAlignment = Alignment.Center
    ) { Label(name, color = Ink, size = 13) }
}

@Composable
fun SynthTab(s: AppState) {
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }
    val notes = remember(s.rev, s.scene, s.synth) { Native.notes(s.scene, s.synth) }
    val page = s.rollPage.coerceIn(0, bars * 2 - 1)

    Card("Play: ${TRACK_NAMES[s.synth + 1]}", trailing = {
        Btn("Bass", on = s.synth == 0, height = 30.dp) { s.synth = 0 }
        Btn("Lead", on = s.synth == 1, height = 30.dp) { s.synth = 1 }
    }) {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            val chord = remember(s.rev, s.synth) { Native.getParam(P.SYN_CHORD, s.synth) > 0.5f }
            Btn(if (chord) "Chord: on" else "Chord", Modifier.weight(1f), on = chord, height = 32.dp) {
                Native.beginEdit(); Native.setParam(P.SYN_CHORD, s.synth, if (chord) 0f else 1f); s.rev++
            }
            Btn("Accent", Modifier.weight(1f), on = s.accent, onColor = Yellow, height = 32.dp) { s.accent = !s.accent }
            Btn(if (s.rollMode) "Show pads" else "Piano roll", Modifier.weight(1f), height = 32.dp) { s.rollMode = !s.rollMode }
        }
        if (!s.rollMode) {
            for (row in 1 downTo 0) Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                for (col in 0 until 4) ScalePad(s, row * 4 + col, Modifier.weight(1f))
            }
        } else {
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                for (pg in 0 until bars * 2) {
                    val playingHere = s.playing && s.step / 8 == pg
                    Btn("${pg / 2 + 1}" + (if (pg % 2 == 0) "a" else "b"), on = pg == page, onColor = if (playingHere) Green else Accent, height = 30.dp) { s.rollPage = pg }
                }
            }
            for (deg in 7 downTo 0) Row(horizontalArrangement = Arrangement.spacedBy(4.dp), verticalAlignment = Alignment.CenterVertically) {
                val name = remember(s.rev, s.synth, deg) { noteName(Native.degreeMidi(s.synth, deg)) }
                Label(name, size = 10, modifier = Modifier.width(30.dp))
                for (col in 0 until 8) {
                    val st = page * 8 + col
                    var v = 0
                    var k = 0
                    while (k + 3 < notes.size) { if (notes[k] == st && notes[k + 1] == deg) v = notes[k + 3]; k += 4 }
                    Cell(v, s.playing && s.step == st, col % 4 == 0, Purple, Modifier.weight(1f).height(32.dp)) {
                        if (Native.toggleNote(s.scene, s.synth, st, deg, s.accent)) {     // let the new note be heard
                            Native.noteOn(s.synth, deg, s.vel()); Native.noteOff(s.synth, deg)
                        }
                        s.rev++
                    }
                }
            }
        }
    }
    Card("Sound: ${TRACK_NAMES[s.synth + 1]}") {
        val waves = listOf("Saw", "Square", "Triangle", "Sine")
        Choice(s, "Osc 1", P.SYN_WAVE1, s.synth, waves)
        Choice(s, "Osc 2", P.SYN_WAVE2, s.synth, waves)
        Choice(s, "Octave", P.SYN_OCTAVE, s.synth, listOf("-2", "-1", "0", "+1", "+2"), offset = -2)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Cutoff", P.SYN_CUTOFF, s.synth, 80f, 8000f, Modifier.weight(1f)) { "${it.roundToInt()} Hz" }
            ParamSlider(s, "Resonance", P.SYN_RES, s.synth, 0.5f, 15f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f}" }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Attack", P.SYN_ATTACK, s.synth, 0.002f, 0.4f, Modifier.weight(1f)) { "${(it * 1000).roundToInt()} ms" }
            ParamSlider(s, "Decay", P.SYN_DECAY, s.synth, 0.03f, 1.5f, Modifier.weight(1f)) { secs(it) }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Sustain", P.SYN_SUSTAIN, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            ParamSlider(s, "Release", P.SYN_RELEASE, s.synth, 0.01f, 1.5f, Modifier.weight(1f)) { secs(it) }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Detune", P.SYN_DETUNE, s.synth, -30f, 30f, Modifier.weight(1f)) { signed(it, "ct") }
            ParamSlider(s, "Level", P.SYN_LEVEL, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
        }
        Choice(s, "LFO to", P.SYN_LFO_TARGET, s.synth, listOf("Off", "Cutoff", "Pitch", "Amp"))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "LFO rate", P.SYN_LFO_RATE, s.synth, 0.1f, 12f, Modifier.weight(1f)) { "${(it * 10).roundToInt() / 10f} Hz" }
            ParamSlider(s, "LFO depth", P.SYN_LFO_DEPTH, s.synth, 0f, 1f, Modifier.weight(1f)) { pct(it) }
        }
    }
}

// ------------------------------------------------------------------ Mix

@Composable
fun Meter(level: Float) {
    val shape = RoundedCornerShape(6.dp)
    Box(Modifier.fillMaxWidth().height(12.dp).clip(shape).background(Color(0xFF0D1018)).border(1.dp, Line, shape)) {
        val f = (level / 0.944f).coerceIn(0f, 1f)
        Box(Modifier.fillMaxHeight().fillMaxWidth(f).background(if (f > 0.97f) Danger else if (f > 0.7f) Yellow else Green))
    }
}

@Composable
fun MixTab(s: AppState) {
    Card("Track", trailing = {
        TRACK_NAMES.forEachIndexed { i, name -> Btn(name, on = s.fxTrack == i, height = 30.dp) { s.fxTrack = i } }
    }) {
        val t = s.fxTrack
        val mute = remember(s.rev, t) { Native.getParam(P.FX_MUTE, t) > 0.5f }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
            ParamSlider(s, "Level", P.FX_LEVEL, t, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            Btn(if (mute) "Muted" else "Mute", on = mute, onColor = Danger, height = 44.dp) {
                Native.beginEdit(); Native.setParam(P.FX_MUTE, t, if (mute) 0f else 1f); s.rev++
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Drive", P.FX_DRIVE, t, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            ParamSlider(s, "Delay send", P.FX_DELAY, t, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Low EQ", P.FX_EQ_LOW, t, -12f, 12f, Modifier.weight(1f)) { signed(it, "dB") }
            ParamSlider(s, "High EQ", P.FX_EQ_HIGH, t, -12f, 12f, Modifier.weight(1f)) { signed(it, "dB") }
        }
        ParamSlider(s, "Reverb send", P.FX_REVERB, t, 0f, 0.8f) { pct(it) }
    }
    Card("Master") {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ParamSlider(s, "Volume", P.MASTER, 0, 0f, 1f, Modifier.weight(1f)) { pct(it) }
            ParamSlider(s, "Delay feedback", P.DELAY_FB, 0, 0f, 0.8f, Modifier.weight(1f)) { pct(it) }
        }
        Meter(s.peakL)
        Meter(s.peakR)
        Label("A limiter holds the output just below full scale. Red means it is working hard.", size = 10, bold = false)
    }
    Card("Audio") {
        Text(if (s.audioOk) s.info else "No audio device could be opened.", color = Ink, fontSize = 12.sp)
        Label("Wired headphones or the speaker respond fastest. Bluetooth always adds delay.", size = 10, bold = false)
    }
}

// ------------------------------------------------------------------ Song

@Composable
fun SongTab(s: AppState) {
    val chain = remember(s.rev) { Native.chain() }
    val bars = remember(s.rev, s.scene) { Native.sceneBars(s.scene) }

    Card("Song chain", trailing = {
        Btn(if (s.songMode) "Song mode: on" else "Song mode", on = s.songMode, onColor = Green, height = 30.dp) {
            s.songMode = !s.songMode
            Native.setSongMode(s.songMode)
            if (s.playing) Native.play(s.scene)                   // restart from the top in the new mode
        }
    }) {
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            chain.forEachIndexed { pos, sc ->
                Btn(SCENE_NAMES[sc.coerceIn(0, 3)] + "  x", on = s.playing && s.songMode && s.chainPos == pos, onColor = Green, height = 34.dp) {
                    Native.chainRemove(pos); s.rev++
                }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            Label("Add", size = 10)
            SCENE_NAMES.forEachIndexed { i, name -> Btn("+ $name", Modifier.weight(1f), height = 34.dp) { Native.chainAdd(i); s.rev++ } }
        }
        Label("Tap an entry to remove it. Song mode plays the chain from the top.", size = 10, bold = false)
    }
    Card("Scene ${SCENE_NAMES[s.scene]}") {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            Label("Length", size = 10, modifier = Modifier.width(48.dp))
            for (b in 1..8) Btn("$b", Modifier.weight(1f), on = bars == b, height = 34.dp, pad = 1.dp) { Native.setSceneBars(s.scene, b); s.rev++ }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
            Label("Copy to", size = 10, modifier = Modifier.width(48.dp))
            SCENE_NAMES.forEachIndexed { i, name ->
                Btn(name, Modifier.weight(1f), enabled = i != s.scene, height = 34.dp) { Native.copyScene(s.scene, i); s.rev++ }
            }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            Btn("Clear scene", Modifier.weight(1f), height = 34.dp) { Native.clearScene(s.scene); s.rev++ }
            Btn("Reset project", Modifier.weight(1f), height = 34.dp) { Native.resetToDemo(); s.rev++ }
        }
        Label("Everything here can be undone.", size = 10, bold = false)
    }
    Card("Groove and key") {
        ParamSlider(s, "Swing", P.SWING, 0, 0f, 0.5f) { if (it < 0.005f) "Straight" else "${(50 + it * 50).roundToInt()}%" }
        Choice(s, "Root", P.ROOT, 0, NOTE_NAMES)
        Choice(s, "Scale", P.SCALE, 0, listOf("Minor", "Major", "Dorian", "Pentatonic"))
        Choice(s, "Octave", P.OCTAVE, 0, listOf("Low", "Mid", "High"), offset = 2)
    }
}
