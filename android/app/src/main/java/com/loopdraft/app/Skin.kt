package com.loopdraft.app

import androidx.compose.foundation.layout.width
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt

// ---- Modular Mix-and-Match Skin System ----

enum class FrameSkin(val title: String) {
    WALNUT_80S("Walnut 80s"),
    STUDIO_RACK("Studio Rack"),
    POCKET_CREAM("Pocket Cream"),
    OLED_CYBER("OLED Cyber"),
    FIELD_OLIVE("Field Olive")
}

enum class KnobSkin(val title: String) {
    H_SLIDER("H-Slider (L-R)"),
    LED_BAR("LED Bar (L-R)"),
    SKIRTED_80S("Skirted 80s"),
    MOOG_SILVER("Silver Cap"),
    NEON_RING("Neon Ring"),
    BAKELITE("Bakelite")
}

enum class ButtonSkin(val title: String) {
    CAPS_808("808 Caps"),
    MPC_RUBBER("MPC Rubber"),
    GLASS_NEON("Glass Neon"),
    ALU_SWITCH("Alu Switch")
}

enum class AccentSkin(val title: String) {
    AMBER_808("Amber 808"),
    ACID_LIME("Acid Lime"),
    CYBER_PINK("Cyber Pink"),
    ICE_STEEL("Ice Steel"),
    RUBY_GOLD("Ruby Gold")
}

data class SkinConfig(
    val frame: FrameSkin = FrameSkin.WALNUT_80S,
    val knob: KnobSkin = KnobSkin.H_SLIDER,
    val button: ButtonSkin = ButtonSkin.CAPS_808,
    val accent: AccentSkin = AccentSkin.AMBER_808
) {
    fun encode(): String = "${frame.ordinal},${knob.ordinal},${button.ordinal},${accent.ordinal}"

    companion object {
        fun decode(raw: String?): SkinConfig {
            if (raw.isNullOrBlank()) return SkinConfig()
            val p = raw.split(",")
            fun <T : Enum<T>> pick(arr: Array<T>, idx: Int, def: T): T =
                p.getOrNull(idx)?.toIntOrNull()?.let { arr.getOrNull(it) } ?: def
            return SkinConfig(
                frame = pick(FrameSkin.values(), 0, FrameSkin.WALNUT_80S),
                knob = pick(KnobSkin.values(), 1, KnobSkin.H_SLIDER),
                button = pick(ButtonSkin.values(), 2, ButtonSkin.CAPS_808),
                accent = pick(AccentSkin.values(), 3, AccentSkin.AMBER_808)
            )
        }
    }
}

/**
 * How the app feels in the hand, apart from how it looks. Stored next to the skin.
 * scale: 0 compact, 1 normal, 2 large (everything, text included). speed: 0 fine, 1 normal, 2 fast (finger travel of controls).
 */
data class UiPrefs(
    val scale: Int = 1,
    val speed: Int = 1,
    val haptics: Boolean = true,
    val leftHanded: Boolean = false
) {
    val scaleFactor: Float get() = when (scale) { 0 -> 0.88f; 2 -> 1.16f; else -> 1f }
    /** Multiplies the finger travel for a control's whole range: more travel is finer. */
    val travelFactor: Float get() = when (speed) { 0 -> 1.8f; 2 -> 0.6f; else -> 1f }
    fun encode(): String = "$scale,$speed,${if (haptics) 1 else 0},${if (leftHanded) 1 else 0}"

    companion object {
        fun decode(raw: String?): UiPrefs {
            val p = (raw ?: "").split(",").map { it.toIntOrNull() }
            return UiPrefs(
                scale = (p.getOrNull(0) ?: 1).coerceIn(0, 2),
                speed = (p.getOrNull(1) ?: 1).coerceIn(0, 2),
                haptics = (p.getOrNull(2) ?: 1) != 0,
                leftHanded = (p.getOrNull(3) ?: 0) != 0
            )
        }
    }
}

val LocalUiPrefs = compositionLocalOf { UiPrefs() }

data class RigPreset(val name: String, val config: SkinConfig)

val SKIN_PRESETS = listOf(
    RigPreset("80s Slider", SkinConfig(FrameSkin.WALNUT_80S, KnobSkin.H_SLIDER, ButtonSkin.CAPS_808, AccentSkin.AMBER_808)),
    RigPreset("Studio Rack", SkinConfig(FrameSkin.STUDIO_RACK, KnobSkin.H_SLIDER, ButtonSkin.ALU_SWITCH, AccentSkin.ICE_STEEL)),
    RigPreset("Pocket Cream", SkinConfig(FrameSkin.POCKET_CREAM, KnobSkin.LED_BAR, ButtonSkin.MPC_RUBBER, AccentSkin.AMBER_808)),
    RigPreset("Cyber OLED", SkinConfig(FrameSkin.OLED_CYBER, KnobSkin.NEON_RING, ButtonSkin.GLASS_NEON, AccentSkin.CYBER_PINK)),
    RigPreset("Acid Field", SkinConfig(FrameSkin.FIELD_OLIVE, KnobSkin.BAKELITE, ButtonSkin.CAPS_808, AccentSkin.ACID_LIME)),
    RigPreset("Classic 808", SkinConfig(FrameSkin.WALNUT_80S, KnobSkin.SKIRTED_80S, ButtonSkin.CAPS_808, AccentSkin.AMBER_808))
)

/** Concrete colors & geometry resolved from a SkinConfig. */
data class SkinTheme(
    val config: SkinConfig,
    val isLight: Boolean,
    val chassis: Color,
    val faceplate: Color,
    val recess: Color,
    val line: Color,
    val buttonOff: Color,
    val railDark: Color,
    val railLight: Color,
    val railScrew: Color?,
    val silk: Color,
    val muted: Color,
    val padSurface: Color,
    val padActive: Color,
    val padInk: Color,
    val onAccentInk: Color,
    val primary: Color,
    val secondary: Color,
    val tertiary: Color,
    val positive: Color,
    val danger: Color,
    val ledOff: Color,
    val scopeBg: Color,
    val scopeGrid: Color,
    val scopeTrace: Color,
    val buttonRadius: Dp,
    val panelRadius: Dp
)

fun resolveSkin(cfg: SkinConfig): SkinTheme {
    val (primary, secondary, tertiary, positive) = when (cfg.accent) {
        AccentSkin.AMBER_808 -> listOf(Color(0xFFFF6D1F), Color(0xFF29D3E8), Color(0xFFFFCA28), Color(0xFF3BE083))
        AccentSkin.ACID_LIME -> listOf(Color(0xFFB4F02E), Color(0xFF18E2F2), Color(0xFFFFD83B), Color(0xFF3EF29B))
        AccentSkin.CYBER_PINK -> listOf(Color(0xFFFF3898), Color(0xFF2CE8F5), Color(0xFFFFC845), Color(0xFF42F5A7))
        AccentSkin.ICE_STEEL -> listOf(Color(0xFF4DA6FF), Color(0xFF38D9C0), Color(0xFFFFB74D), Color(0xFF4CE08A))
        AccentSkin.RUBY_GOLD -> listOf(Color(0xFFE83E4C), Color(0xFFF5B83D), Color(0xFFFF8A47), Color(0xFF3CD986))
    }

    val btnRadius = when (cfg.button) {
        ButtonSkin.CAPS_808 -> 7.dp
        ButtonSkin.MPC_RUBBER -> 10.dp
        ButtonSkin.GLASS_NEON -> 8.dp
        ButtonSkin.ALU_SWITCH -> 3.dp
    }
    val pnlRadius = when (cfg.frame) {
        FrameSkin.STUDIO_RACK -> 5.dp
        FrameSkin.POCKET_CREAM -> 12.dp
        else -> 10.dp
    }

    return when (cfg.frame) {
        FrameSkin.WALNUT_80S -> SkinTheme(
            config = cfg, isLight = false,
            chassis = Color(0xFF15161A), faceplate = Color(0xFF1F2127), recess = Color(0xFF121317),
            line = Color(0xFF353942), buttonOff = Color(0xFF2B2E35),
            railDark = Color(0xFF3A2111), railLight = Color(0xFF6B4226), railScrew = null,
            silk = Color(0xFFEBE5D5), muted = Color(0xFF969185),
            padSurface = Color(0xFFD5CEBD), padActive = Color(0xFFFFF4DC), padInk = Color(0xFF1B1C20),
            onAccentInk = Color(0xFF141518),
            primary = primary, secondary = secondary, tertiary = tertiary, positive = positive,
            danger = Color(0xFFE53935), ledOff = Color(0xFF332626),
            scopeBg = Color(0xFF07140D), scopeGrid = Color(0xFF143524), scopeTrace = positive,
            buttonRadius = btnRadius, panelRadius = pnlRadius
        )
        FrameSkin.STUDIO_RACK -> SkinTheme(
            config = cfg, isLight = false,
            chassis = Color(0xFF12151B), faceplate = Color(0xFF1B2029), recess = Color(0xFF0F1318),
            line = Color(0xFF364152), buttonOff = Color(0xFF262E3B),
            railDark = Color(0xFF252C38), railLight = Color(0xFF3D4759), railScrew = Color(0xFF8C9BB0),
            silk = Color(0xFFEDF2FA), muted = Color(0xFF8C98AB),
            padSurface = Color(0xFFCFD7E3), padActive = Color(0xFFFFFFFF), padInk = Color(0xFF12161E),
            onAccentInk = Color(0xFF0E1117),
            primary = primary, secondary = secondary, tertiary = tertiary, positive = positive,
            danger = Color(0xFFE84545), ledOff = Color(0xFF282F3B),
            scopeBg = Color(0xFF07111A), scopeGrid = Color(0xFF142D42), scopeTrace = secondary,
            buttonRadius = btnRadius, panelRadius = pnlRadius
        )
        FrameSkin.POCKET_CREAM -> SkinTheme(
            config = cfg, isLight = true,
            chassis = Color(0xFFE4DFD3), faceplate = Color(0xFFF3EFE6), recess = Color(0xFFDCD6C8),
            line = Color(0xFFB5AE9F), buttonOff = Color(0xFFE6E0D3),
            railDark = Color(0xFFB3ADA0), railLight = Color(0xFFD6D0C4), railScrew = Color(0xFF7D776C),
            silk = Color(0xFF1F2024), muted = Color(0xFF636058),
            padSurface = Color(0xFFFFFFFF), padActive = Color(0xFFFFF0D4), padInk = Color(0xFF1A1B1E),
            onAccentInk = Color(0xFF16171A),
            primary = primary, secondary = secondary, tertiary = tertiary, positive = positive,
            danger = Color(0xFFD9302C), ledOff = Color(0xFFB8B0A0),
            scopeBg = Color(0xFF141816), scopeGrid = Color(0xFF26332D), scopeTrace = positive,
            buttonRadius = btnRadius, panelRadius = pnlRadius
        )
        FrameSkin.OLED_CYBER -> SkinTheme(
            config = cfg, isLight = false,
            chassis = Color(0xFF050608), faceplate = Color(0xFF0C0E14), recess = Color(0xFF05060A),
            line = Color(0xFF272D3D), buttonOff = Color(0xFF141824),
            railDark = Color(0xFF080A12), railLight = primary.copy(alpha = 0.45f), railScrew = null,
            silk = Color(0xFFF4F6FF), muted = Color(0xFF838CA6),
            padSurface = Color(0xFF1A1F2E), padActive = primary, padInk = Color(0xFFF4F6FF),
            onAccentInk = Color(0xFF090A0E),
            primary = primary, secondary = secondary, tertiary = tertiary, positive = positive,
            danger = Color(0xFFFF3355), ledOff = Color(0xFF241B29),
            scopeBg = Color(0xFF030508), scopeGrid = primary.copy(alpha = 0.22f), scopeTrace = primary,
            buttonRadius = btnRadius, panelRadius = pnlRadius
        )
        FrameSkin.FIELD_OLIVE -> SkinTheme(
            config = cfg, isLight = false,
            chassis = Color(0xFF171A15), faceplate = Color(0xFF22271F), recess = Color(0xFF141712),
            line = Color(0xFF3E4738), buttonOff = Color(0xFF2D3329),
            railDark = Color(0xFF2C2618), railLight = Color(0xFF4F4429), railScrew = Color(0xFF8F8362),
            silk = Color(0xFFEBE7D6), muted = Color(0xFF999A86),
            padSurface = Color(0xFFD1CCB7), padActive = Color(0xFFF5F0D8), padInk = Color(0xFF1A1D16),
            onAccentInk = Color(0xFF131610),
            primary = primary, secondary = secondary, tertiary = tertiary, positive = positive,
            danger = Color(0xFFE04538), ledOff = Color(0xFF332C22),
            scopeBg = Color(0xFF0B140A), scopeGrid = Color(0xFF1F361C), scopeTrace = positive,
            buttonRadius = btnRadius, panelRadius = pnlRadius
        )
    }
}

val LocalSkin = compositionLocalOf { resolveSkin(SkinConfig()) }

/** Full-screen overlay dialog for mixing and matching Frame, Sliders/Knobs, Buttons, and Accents. */
@Composable
fun SkinStudioModal(s: AppState, onDismiss: () -> Unit) {
    val skin = LocalSkin.current
    var previewVal by remember { mutableFloatStateOf(0.68f) }
    var previewBipolar by remember { mutableFloatStateOf(0.3f) }

    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.72f))
            .clickable(onClick = onDismiss)
            .padding(12.dp),
        contentAlignment = Alignment.Center
    ) {
        val modalShape = RoundedCornerShape(14.dp)
        Column(
            Modifier
                .fillMaxWidth()
                .clip(modalShape)
                .background(skin.chassis)
                .border(1.5.dp, skin.primary, modalShape)
                .clickable(enabled = false) { }
                .padding(12.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Box(Modifier.size(width = 5.dp, height = 18.dp).clip(RoundedCornerShape(2.dp)).background(skin.primary))
                Column(Modifier.weight(1f)) {
                    SilkLabel("SETUP", color = skin.silk, size = 14)
                    SilkLabel("How it feels, and how it looks", color = skin.muted, size = 9, bold = false)
                }
                HwButton("DONE", on = true, ledColor = skin.positive, height = 30.dp, pad = 12.dp) { onDismiss() }
            }

            // How it feels in the hand
            ModulePanel("Feel & Layout", accent = skin.positive) {
                val pr = s.prefs
                val row: @Composable (String, List<String>, Int, (Int) -> Unit) -> Unit = { label, names, sel, pick ->
                    Row(horizontalArrangement = Arrangement.spacedBy(5.dp), verticalAlignment = Alignment.CenterVertically) {
                        SilkLabel(label, size = 9, modifier = Modifier.width(64.dp))
                        names.forEachIndexed { i, nm ->
                            HwButton(nm, Modifier.weight(1f), on = sel == i, ledColor = skin.positive, height = 30.dp, pad = 2.dp) { pick(i) }
                        }
                    }
                }
                row("SIZE", listOf("COMPACT", "NORMAL", "LARGE"), pr.scale) { s.updatePrefs(pr.copy(scale = it)) }
                row("CONTROLS", listOf("FINE", "NORMAL", "FAST"), pr.speed) { s.updatePrefs(pr.copy(speed = it)) }
                row("VIBRATE", listOf("OFF", "ON TAP"), if (pr.haptics) 1 else 0) { s.updatePrefs(pr.copy(haptics = it == 1)) }
                row("SIDEWAYS", listOf("RAIL LEFT", "RAIL RIGHT"), if (pr.leftHanded) 1 else 0) { s.updatePrefs(pr.copy(leftHanded = it == 1)) }
                SilkLabel("Double-tap any slider or knob to set it back to its default.", color = skin.muted, size = 9, bold = false)
                SilkLabel("SIDEWAYS: where the transport rail sits when the phone is turned.", color = skin.muted, size = 9, bold = false)
                SilkLabel(if (s.audioOk) "Audio: ${s.info}" else "Audio could not be started.", color = skin.muted, size = 9, bold = false)
            }

            // Live Interactive Preview
            ModulePanel("Live Control Preview (Drag Left <-> Right)", accent = skin.primary) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                    Knob(
                        label = "Cutoff",
                        value = previewVal,
                        min = 0f,
                        max = 1f,
                        modifier = Modifier.weight(1f),
                        fmt = { "${(it * 100).roundToInt()}%" },
                        onStart = {},
                        onChange = { previewVal = it },
                        onEnd = {}
                    )
                    Knob(
                        label = "Pan",
                        value = previewBipolar,
                        min = -1f,
                        max = 1f,
                        modifier = Modifier.weight(1f),
                        modulated = true,
                        fmt = { if (it < -0.02f) "L${(-it * 100).roundToInt()}" else if (it > 0.02f) "R${(it * 100).roundToInt()}" else "CTR" },
                        onStart = {},
                        onChange = { previewBipolar = it },
                        onEnd = {}
                    )
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    HwButton("PLAY", Modifier.weight(1f), on = true, ledColor = skin.positive, showLed = true, height = 32.dp) {}
                    HwButton("ACCENT", Modifier.weight(1f), on = true, ledColor = skin.primary, showLed = true, height = 32.dp) {}
                    HwButton("BYPASS", Modifier.weight(1f), on = false, ledColor = skin.secondary, showLed = true, height = 32.dp) {}
                }
            }

            // 1-Tap Rig Presets
            ModulePanel("Complete Rig Presets", accent = skin.secondary) {
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    for (preset in SKIN_PRESETS) {
                        val active = s.skin == preset.config
                        HwButton(preset.name.uppercase(), on = active, ledColor = skin.secondary, height = 30.dp, pad = 9.dp) {
                            s.updateSkin(preset.config)
                        }
                    }
                }
            }

            // 1. Controls: Sliders vs Rotary Knobs
            ModulePanel("1. Controls (Left-Right Sliders & Rotary Knobs)", accent = skin.primary) {
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    for (k in KnobSkin.values()) {
                        HwButton(k.title.uppercase(), on = s.skin.knob == k, ledColor = skin.primary, showLed = true, height = 32.dp, pad = 9.dp) {
                            s.updateSkin(s.skin.copy(knob = k))
                        }
                    }
                }
                SilkLabel(
                    if (s.skin.knob == KnobSkin.H_SLIDER || s.skin.knob == KnobSkin.LED_BAR)
                        "Horizontal slider mode active: drag left or right on any control track."
                    else
                        "Rotary knob mode active: drag left or right on any knob tile.",
                    color = skin.muted, size = 9, bold = false
                )
            }

            // 2. Main Frame & Rails
            ModulePanel("2. Main Frame & Side Rails", accent = skin.tertiary) {
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    for (f in FrameSkin.values()) {
                        HwButton(f.title.uppercase(), on = s.skin.frame == f, ledColor = skin.tertiary, height = 30.dp, pad = 9.dp) {
                            s.updateSkin(s.skin.copy(frame = f))
                        }
                    }
                }
            }

            // 3. Buttons & Pads
            ModulePanel("3. Buttons & Pads", accent = skin.secondary) {
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    for (b in ButtonSkin.values()) {
                        HwButton(b.title.uppercase(), on = s.skin.button == b, ledColor = skin.secondary, height = 30.dp, pad = 9.dp) {
                            s.updateSkin(s.skin.copy(button = b))
                        }
                    }
                }
            }

            // 4. Accent LED Palette
            ModulePanel("4. Accent & LED Palette", accent = skin.positive) {
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    for (a in AccentSkin.values()) {
                        HwButton(a.title.uppercase(), on = s.skin.accent == a, ledColor = skin.positive, height = 30.dp, pad = 9.dp) {
                            s.updateSkin(s.skin.copy(accent = a))
                        }
                    }
                }
            }
        }
    }
}
