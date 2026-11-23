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
import androidx.core.content.FileProvider
import java.io.File

class MainActivity : ComponentActivity() {
    private val state = AppState()
    private var selfTest = false
    private var jungleShot = false

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
        state.onExport = { kind -> export(kind) }

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
        if (jungleShot) { jungleShot = false; Native.loadJungleKit(); Native.writeBreakPattern(0); state.rev++ }
        if (selfTest) runSelfTest()
    }

    /** kind: 0 = MIDI, 1 = WAV mix, 2 = one WAV per track. Renders off the main thread, then opens the share sheet. */
    private fun export(kind: Int) {
        if (state.exporting) return
        val song = state.songMode
        val scene = state.scene
        val base = "loopdraft_" + (if (song) "song" else "scene_" + SCENE_NAMES[scene])
        val dir = File(cacheDir, "exports").apply { mkdirs() }
        Native.snapshotForExport()
        state.exporting = true
        state.exportMsg = if (kind == 0) "" else "Rendering..."
        Thread {
            val files = ArrayList<File>()
            var ok = true
            when (kind) {
                0 -> { val f = File(dir, "$base.mid"); ok = Native.exportMidi(f.absolutePath, song, scene); files.add(f) }
                1 -> { val f = File(dir, "$base.wav"); ok = Native.exportWav(f.absolutePath, song, scene, -1); files.add(f) }
                else -> listOf("drums", "bass", "lead").forEachIndexed { i, n ->
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
