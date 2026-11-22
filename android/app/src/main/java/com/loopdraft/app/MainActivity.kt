package com.loopdraft.app

import android.media.AudioManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge

class MainActivity : ComponentActivity() {
    private val state = AppState()
    private var selfTest = false

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

        setContent { App(state) }
    }

    override fun onStart() {
        super.onStart()
        state.audioOk = Native.start(filesDir.absolutePath)
        state.info = Native.info()
        Native.setSongMode(state.songMode)
        Native.setRecording(state.rec)
        state.rev++
        if (selfTest) runSelfTest()
    }

    override fun onStop() {
        Native.stop()   // stops the sound and saves the project
        super.onStop()
    }

    /** Plays the demo for a few seconds and writes what happens to the log. */
    private fun runSelfTest() {
        selfTest = false
        val handler = Handler(Looper.getMainLooper())
        Log.i("LoopDraft", "selftest audio=${state.audioOk} ${Native.info()}")
        // Save and restore: change the tempo, stop (saves), start again (loads) and compare.
        Native.beginEdit(); Native.setParam(P.BPM, 0, 131f)
        Native.stop()
        Native.start(filesDir.absolutePath)
        Log.i("LoopDraft", "selftest saved=${Native.getParam(P.BPM, 0) == 131f}")
        Native.undo()
        Native.play(0)
        var loudest = 0f
        for (i in 1..12) handler.postDelayed({
            loudest = maxOf(loudest, Native.peak(0), Native.peak(1))
            Log.i("LoopDraft", "selftest playing=${Native.isPlaying()} step=${Native.currentStep()} sound=${loudest > 0.01f}")
        }, i * 400L)
    }
}
