package com.loopdraft.app

import android.content.Intent
import android.media.AudioManager
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.FileProvider
import java.io.File

class MainActivity : ComponentActivity() {
    private val state = AppState()
    private var selfTest = false
    private var jungleShot = false
    private var fourShot = false

    // "LOAD WAV" in the break slicer: the system file picker, then a private copy the engine can open by path.
    private val pickBreak = registerForActivityResult(ActivityResultContracts.GetContent()) { uri -> if (uri != null) loadBreak(uri) }

    // ---------------------------------------------------------------- library of presets and grooves
    // Built-in ones are assets (the repository's content/library folder), the user's own are files in the app's folder.
    private val pickContent = registerForActivityResult(ActivityResultContracts.GetContent()) { uri -> if (uri != null) importContentFile(uri) }
    private fun libraryDir() = File(filesDir, "library").apply { mkdirs() }

    private fun header(text: String): Triple<String, String, String>? = try {
        val o = org.json.JSONObject(text)
        Triple(o.getString("kind").lowercase(), o.getString("name"), o.optString("style", ""))
    } catch (e: Exception) { null }

    private fun assetJsons(dir: String, out: MutableList<String>) {
        for (n in assets.list(dir) ?: emptyArray()) {
            val p = "$dir/$n"
            if (n.endsWith(".json")) out.add(p) else if (!n.contains('.')) assetJsons(p, out)
        }
    }

    // Reading and parsing well over a hundred small files is kept off the screen's thread, so it never delays the first frame.
    private fun refreshLibrary(done: () -> Unit = {}) {
        Thread {
            val items = mutableListOf<LibItem>()
            val builtIn = mutableListOf<String>()
            try { assetJsons("library", builtIn) } catch (e: Exception) { }
            for (p in builtIn.sorted()) {
                val text = try { assets.open(p).bufferedReader().use { it.readText() } } catch (e: Exception) { continue }
                val h = header(text) ?: continue
                items.add(LibItem(h.first, h.second, h.third, false, p))
            }
            for (f in (libraryDir().listFiles() ?: emptyArray()).filter { it.isFile && it.name.endsWith(".json") }.sortedBy { it.name }) {
                val text = try { f.readText() } catch (e: Exception) { continue }
                val h = header(text) ?: continue
                items.add(LibItem(h.first, h.second, h.third, true, f.absolutePath))
            }
            val sorted = items.sortedWith(compareBy({ it.kind }, { !it.user }, { it.style }, { it.name }))
            runOnUiThread {
                state.library.clear()
                state.library.addAll(sorted)
                done()
            }
        }.start()
    }

    private fun libText(item: LibItem): String? = try {
        if (item.user) File(item.path).readText() else assets.open(item.path).bufferedReader().use { it.readText() }
    } catch (e: Exception) { null }

    private fun loadLibItem(item: LibItem) {
        val text = libText(item)
        val err = if (text == null) "could not be read" else Native.importContent(text, state.scene, state.synth, state.libTempoKey)
        state.libMsg = if (err.isEmpty()) "Loaded \"${item.name}\". UNDO takes it back." else "\"${item.name}\" has an error: $err"
        if (err.isEmpty() && item.kind == "groove") { state.drumBar = 0; state.rollPage = 0 }
        state.rev++
    }

    private fun userFile(kind: String, name: String) = File(libraryDir(), "$kind-${cleanName(name).replace(' ', '_')}.json")

    private fun saveLibItem(kind: String, raw: String) {
        val name = cleanName(raw)
        if (name.isEmpty()) { state.libMsg = "Give it a name first."; return }
        val text = Native.exportContent(when (kind) { "instrument" -> 0; "drumkit" -> 1; else -> 2 }, state.scene, state.synth, name)
        val ok = text.isNotEmpty() && try { userFile(kind, name).writeText(text); true } catch (e: Exception) { false }
        state.libMsg = if (ok) "Saved \"$name\"." else "Could not save."
        refreshLibrary()
    }

    private fun deleteLibItem(item: LibItem) {
        if (item.user) File(item.path).delete()
        state.libMsg = "Deleted \"${item.name}\"."
        refreshLibrary()
    }

    private fun readLimited(input: java.io.InputStream, max: Int): ByteArray {
        val out = java.io.ByteArrayOutputStream()
        val buf = ByteArray(1 shl 14)
        while (out.size() <= max) {
            val n = input.read(buf)
            if (n < 0) break
            out.write(buf, 0, n)
        }
        return out.toByteArray()
    }

    private fun importContentFile(uri: Uri) {
        val limit = 2 * 1024 * 1024
        val text = try {
            contentResolver.openInputStream(uri)?.use { input ->
                val bytes = readLimited(input, limit)
                if (bytes.size > limit) null else String(bytes, Charsets.UTF_8)
            }
        } catch (e: Exception) { null }
        if (text == null) { state.libMsg = "Could not read this file (it must be a .json of at most 2 MB)."; return }
        val err = Native.checkContent(text)
        val h = header(text)
        if (err.isNotEmpty() || h == null) { state.libMsg = "This file has an error: ${if (err.isNotEmpty()) err else "kind or name is missing"}"; return }
        val ok = try { userFile(h.first, h.second).writeText(text); true } catch (e: Exception) { false }
        state.libMsg = if (ok) "Added ${h.first} \"${h.second}\" to the library." else "Could not save the file."
        if (ok) state.libOpen = h.first
        refreshLibrary()
    }

    // Named projects live as files in the app's own folder: <name>.ldp, plus <name>.ldp.break.wav for an own break.
    private fun projectDir() = File(filesDir, "projects").apply { mkdirs() }
    private fun projectFile(name: String) = File(projectDir(), "$name.ldp")
    private fun cleanName(name: String) = name.trim().filter { it.isLetterOrDigit() || it in " -_" }.take(32)

    private fun refreshProjects() {
        val names = (projectDir().listFiles() ?: emptyArray()).filter { it.isFile && it.name.endsWith(".ldp") }
            .sortedByDescending { it.lastModified() }.map { it.name.removeSuffix(".ldp") }
        state.projects.clear()
        state.projects.addAll(names)
    }

    private fun saveProject(raw: String) {
        val name = cleanName(raw)
        if (name.isEmpty()) { state.projectMsg = "Give the project a name first."; return }
        val existed = projectFile(name).exists()
        val ok = Native.saveProjectCopy(projectFile(name).absolutePath)
        state.projectMsg = if (!ok) "Could not save." else if (existed) "Saved over \"$name\"." else "Saved as \"$name\"."
        refreshProjects()
    }

    private fun openProject(name: String) {
        val ok = Native.openProject(projectFile(name).absolutePath)
        state.projectMsg = if (ok) "Opened \"$name\". UNDO goes back to what was open before." else "Could not open \"$name\"."
        if (ok) { state.projectName = name; state.scene = 0; state.drumBar = 0; state.rollPage = 0; state.selChainPos = 0 }
        state.rev++
    }

    private fun deleteProject(name: String) {
        projectFile(name).delete()
        File(projectDir(), "$name.ldp.break.wav").delete()
        state.projectMsg = "Deleted \"$name\"."
        refreshProjects()
    }

    private fun loadBreak(uri: Uri) {
        val tmp = File(cacheDir, "break_in.wav")
        val ok = try {
            var total = 0L
            contentResolver.openInputStream(uri)?.use { input ->
                tmp.outputStream().use { out ->
                    val buf = ByteArray(1 shl 16)
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        total += n
                        if (total > 40L * 1024 * 1024) throw java.io.IOException("too large")
                        out.write(buf, 0, n)
                    }
                }
            } != null && Native.loadJungleBreakWav(tmp.absolutePath)
        } catch (e: Exception) {
            false
        }
        tmp.delete()
        state.drumMsg = if (ok) "" else "Could not use this file. It has to be a WAV of at most 24 seconds."
        state.rev++
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        volumeControlStream = AudioManager.STREAM_MUSIC
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Options used by the automated test on GitHub (android/ci/smoke.sh). Normal starts have none of them.
        val tab = intent?.getIntExtra("tab", -1) ?: -1
        if (tab in 0..3) state.tab = tab
        if (intent?.getBooleanExtra("roll", false) == true) state.rollMode = true
        selfTest = intent?.getBooleanExtra("selftest", false) == true
        jungleShot = intent?.getBooleanExtra("jungle", false) == true
        fourShot = intent?.getBooleanExtra("four", false) == true
        val createdAt = android.os.SystemClock.uptimeMillis()
        state.onFirstFrame = { Log.i("LoopDraft", "first frame after ${android.os.SystemClock.uptimeMillis() - createdAt} ms") }
        state.onExport = { kind -> export(kind) }
        state.onSaveProject = { name -> saveProject(name) }
        state.onOpenProject = { name -> openProject(name) }
        state.onDeleteProject = { name -> deleteProject(name) }
        refreshProjects()
        state.onLibLoad = { item -> loadLibItem(item) }
        state.onLibSave = { kind, name -> saveLibItem(kind, name) }
        state.onLibDelete = { item -> deleteLibItem(item) }
        state.onLibImport = { try { pickContent.launch("*/*") } catch (e: Exception) { state.libMsg = "No file picker on this device." } }
        refreshLibrary()
        state.onLoadBreak = { try { pickBreak.launch("audio/*") } catch (e: Exception) { state.drumMsg = "No file picker on this device." } }

        val skinPrefs = getSharedPreferences("loopdraft_skin", MODE_PRIVATE)
        state.skin = SkinConfig.decode(skinPrefs.getString("config", null))
        state.onSkinChanged = { cfg -> skinPrefs.edit().putString("config", cfg.encode()).apply() }
        state.prefs = UiPrefs.decode(skinPrefs.getString("feel", null))
        state.onPrefsChanged = { p -> skinPrefs.edit().putString("feel", p.encode()).apply() }
        // the app opens on the screen it was left on
        if ((intent?.getIntExtra("tab", -1) ?: -1) < 0) state.tab = skinPrefs.getInt("tab", 0).coerceIn(0, 3)
        // test options: the setup dialog, a size, a layer of the drum and synth screens
        if (intent?.getBooleanExtra("setup", false) == true) state.showSkinStudio = true
        val uiSize = intent?.getIntExtra("uisize", -1) ?: -1
        if (uiSize >= 0) state.prefs = state.prefs.copy(scale = uiSize.coerceIn(0, 2))
        if (intent?.getBooleanExtra("lefthand", false) == true) state.prefs = state.prefs.copy(leftHanded = true)
        val rig = intent?.getIntExtra("rig", -1) ?: -1
        if (rig >= 0) state.skin = SKIN_PRESETS[rig.coerceIn(0, SKIN_PRESETS.size - 1)].config
        val layer = intent?.getIntExtra("layer", -1) ?: -1
        if (layer >= 0) { state.drumLayer = layer.coerceIn(0, 2); state.synthLayer = layer.coerceIn(0, 2) }
        setContent { App(state) }
    }

    override fun onStart() {
        super.onStart()
        state.audioOk = Native.start(filesDir.absolutePath)
        state.info = Native.info()
        Native.setSongMode(state.songMode)
        Native.setRecording(state.rec)
        state.rev++
        Log.i("LoopDraft", "boot audio=${state.audioOk} bpm=${Native.getParam(P.BPM, 0).toInt()} ${state.info}")
        Log.i("LoopDraft", "kit engine=${Native.getParam(P.DRUM_ENGINE, 0).toInt()} ownBreak=${Native.hasOwnBreak()}")
        when (intent?.getStringExtra("kit")) {     // test options: bounce the loop into the slicer, leave the slicer, come back
            "bounce" -> Log.i("LoopDraft", "kit bounce ok=${Native.resampleSceneToJungle(0)}")
            "909" -> Native.setDrumEngine(1)
            "project" -> {
                Native.setParam(P.SYN_MODE, 0, 3f); Native.setParam(P.SYN_VEC_X, 0, 0.8f); Native.setParam(P.SYN_VEC_Y, 0, 0.3f)
                Native.toggleNote(0, 0, 4, 2, false, true)
                saveProject("CI test")
                Native.resetToDemo()
                openProject("CI test")
                Log.i("LoopDraft", "project reopened mode=${Native.getParam(P.SYN_MODE, 0).toInt()} list=${state.projects.size} msg=${state.projectMsg}")
                state.synth = 0; state.rollMode = true
            }
            "autopick" -> {
                // test option: automate an effect-slot control and the drum hardness the way the knobs do it, then show pick mode
                Native.setFxSlot(1, 0, 5, 0.6f, 0.3f, false)
                state.selChainPos = 0
                state.addAutomation(P.FX_SLOT1_AMOUNT, 1)
                state.addAutomation(P.KIT_HARDNESS, 0)
                state.addAutomation(P.SYN_CUTOFF, 0)
                val a = Native.chainAuto(0, 1); val d = Native.chainAuto(0, 0)
                Log.i("LoopDraft", "autopick bass=${a[0].toInt()},${a[3].toInt()} drums=${d[0].toInt()} msg=${state.autoMsg}")
                state.selAutoTrack = 1; state.selAutoLane = 0
                if (intent?.getBooleanExtra("picking", false) == true) { state.pickAuto = true; state.fxTrack = 1; state.tab = 2 } else state.tab = 3
            }
            "lib" -> refreshLibrary {
                // test option: load a built-in groove, save the bass as an own instrument and load that onto the lead
                val g = state.library.firstOrNull { it.kind == "groove" && !it.user }
                if (g != null) loadLibItem(g)
                val grooveMsg = state.libMsg
                val text = Native.exportContent(0, state.scene, 0, "CI bass")
                userFile("instrument", "CI bass").writeText(text)
                refreshLibrary {
                    state.synth = 1
                    val mine = state.library.firstOrNull { it.user && it.kind == "instrument" }
                    if (mine != null) loadLibItem(mine)
                    var bad = 0
                    for (it in state.library) { val t = libText(it); if (t == null || Native.checkContent(t).isNotEmpty()) bad++ }
                    Log.i("LoopDraft", "lib items=${state.library.size} bad=$bad groove=[$grooveMsg] own=[${state.libMsg}] bpm=${Native.getParam(P.BPM, 0).toInt()}")
                    state.libOpen = "groove"
                }
            }
            "dsyncheck" -> Log.i("LoopDraft", "dsyn after restart engine=${Native.getParam(P.DRUM_ENGINE, 0).toInt()} pad0=${Native.padIsSynth(0)}")
            "dsyn" -> {
                Native.setDrumEngine(5); Native.applyDrumSynthTemplate(0, 3); Native.setParam(P.KIT_HARDNESS, 0, 0.4f)
                state.selPad = 0; state.showDrumSynth = true
                Log.i("LoopDraft", "dsyn engine=${Native.getParam(P.DRUM_ENGINE, 0).toInt()} pad0=${Native.padIsSynth(0)}")
            }
            "slicer" -> { Native.setDrumEngine(4); Log.i("LoopDraft", "kit back in slicer ownBreak=${Native.hasOwnBreak()}") }
        }
        intent?.removeExtra("kit")
        if (jungleShot) { jungleShot = false; Native.loadJungleKit(); Native.writeBreakPattern(0); state.rev++ }
        if (fourShot) {                             // test option: four synth tracks with instruments and an FX chain
            fourShot = false
            Native.setActiveSynths(4); Native.applyInstrument(2, 6); Native.applyInstrument(3, 4)
            Native.setFxSlot(2, 0, 3, 0.7f, 0.5f, false); Native.setFxSlot(2, 1, 5, 0.6f, 0.3f, false); Native.setFxSlot(2, 2, 6, 0.5f, 0f, false)
            state.fxTrack = 2; state.synth = 2; state.rev++
        }
        if (selfTest) runSelfTest()
    }

    /** kind: 0 = MIDI, 1 = WAV mix, 2 = one WAV per track. Renders off the main thread, then opens the share sheet. */
    private fun export(kind: Int) {
        if (state.exporting) return
        val song = state.songMode
        val scene = state.scene
        val base = "loopdraft_" + (if (song) "song" else "scene_" + SCENE_NAMES[scene])
        val dir = File(cacheDir, "exports").apply { mkdirs() }
        val tracks = 1 + Native.getParam(P.ACTIVE_SYNTHS, 0).toInt().coerceIn(2, 4)
        val stemNames = listOf("drums", "bass", "lead", "synth3", "synth4").take(tracks)
        Native.snapshotForExport()
        state.exporting = true
        state.exportMsg = if (kind == 0) "" else "Rendering..."
        Thread {
            val files = ArrayList<File>()
            var ok = true
            when (kind) {
                0 -> { val f = File(dir, "$base.mid"); ok = Native.exportMidi(f.absolutePath, song, scene); files.add(f) }
                1 -> { val f = File(dir, "$base.wav"); ok = Native.exportWav(f.absolutePath, song, scene, -1); files.add(f) }
                else -> stemNames.forEachIndexed { i, n ->
                    val f = File(dir, "${base}_$n.wav")
                    ok = Native.exportWav(f.absolutePath, song, scene, i) && ok
                    files.add(f)
                }
            }
            runOnUiThread {
                state.exporting = false
                state.exportMsg = if (ok) "" else "Export failed"
                if (ok) shareFiles(files, if (kind == 0) "audio/midi" else "audio/wav")
            }
        }.start()
    }

    private fun shareFiles(files: List<File>, mime: String) {
        val uris = ArrayList<Uri>()
        for (f in files) uris.add(FileProvider.getUriForFile(this, "$packageName.files", f))
        val intent = if (uris.size == 1) Intent(Intent.ACTION_SEND).putExtra(Intent.EXTRA_STREAM, uris[0])
        else Intent(Intent.ACTION_SEND_MULTIPLE).putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris)
        intent.type = mime
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        startActivity(Intent.createChooser(intent, "Export"))
    }

    override fun onStop() {
        getSharedPreferences("loopdraft_skin", MODE_PRIVATE).edit().putInt("tab", state.tab).apply()
        Native.stop()   // stops the sound and saves the project
        super.onStop()
    }

    /** Plays the demo for a few seconds and writes what happens to the log. */
    private fun runSelfTest() {
        selfTest = false
        val handler = Handler(Looper.getMainLooper())
        // Change the tempo and save. The test then kills the app and checks the next start reports this tempo.
        Native.beginEdit(); Native.setParam(P.BPM, 0, 131f)
        Native.saveIfDirty()
        // Export: render scene A and the song's MIDI the way the Export buttons do, and check the share link can be made.
        Native.snapshotForExport()
        Thread {
            val dir = File(cacheDir, "exports").apply { mkdirs() }
            val mid = File(dir, "selftest.mid")
            val wav = File(dir, "selftest.wav")
            val okMid = Native.exportMidi(mid.absolutePath, true, 0) && mid.length() > 200
            val okWav = Native.exportWav(wav.absolutePath, false, 0, -1) && wav.length() > 200000
            val okShare = try { FileProvider.getUriForFile(this, "$packageName.files", wav).toString().startsWith("content://") } catch (e: Exception) { false }
            Log.i("LoopDraft", "selftest export midi=$okMid wav=$okWav share=$okShare")
        }.start()
        Native.play(0)
        var loudest = 0f
        // Sampled from its own thread: this checks the audio engine, whatever the screen is busy with while the app starts.
        Thread {
            for (i in 1..12) {
                Thread.sleep(400)
                loudest = maxOf(loudest, Native.peak(0), Native.peak(1))
                Log.i("LoopDraft", "selftest playing=${Native.isPlaying()} step=${Native.currentStep()} sound=${loudest > 0.01f}")
            }
        }.start()
    }
}
