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
        state.onExport = { kind -> export(kind) }
        state.onLoadBreak = { try { pickBreak.launch("audio/*") } catch (e: Exception) { state.drumMsg = "No file picker on this device." } }

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
        for (i in 1..12) handler.postDelayed({
            loudest = maxOf(loudest, Native.peak(0), Native.peak(1))
            Log.i("LoopDraft", "selftest playing=${Native.isPlaying()} step=${Native.currentStep()} sound=${loudest > 0.01f}")
        }, i * 400L)
    }
}
