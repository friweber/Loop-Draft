package com.loopdraft.app

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectVerticalDragGestures
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

// ---- 80s Analog Hardware Palette (TR-808 / Juno-106 inspired) ----
val HwChassis = Color(0xFF17181C)
val HwFaceplate = Color(0xFF202227)
val HwRecess = Color(0xFF131418)
val HwLine = Color(0xFF353942)
val HwWoodLight = Color(0xFF6B4226)
val HwWoodDark = Color(0xFF3A2111)
val HwSilk = Color(0xFFEBE5D5)
val HwMuted = Color(0xFF969185)

val HwRed = Color(0xFFE53935)
val HwOrange = Color(0xFFFF6D1F)
val HwYellow = Color(0xFFFFCA28)
val HwCream = Color(0xFFEDE6D4)
val HwCyan = Color(0xFF29D3E8)
val HwGreen = Color(0xFF3BE083)
val HwLedOff = Color(0xFF332626)

val HwPadSurface = Color(0xFFD5CEBD)
val HwPadActive = Color(0xFFFFF4DC)
val HwPadInk = Color(0xFF1B1C20)

/** Thin oiled-walnut side rails framing the hardware faceplate. */
@Composable
fun WalnutRail(modifier: Modifier = Modifier) {
    Box(
        modifier
            .width(7.dp)
            .fillMaxHeight()
            .background(
                Brush.horizontalGradient(
                    listOf(HwWoodDark, HwWoodLight, HwWoodDark)
                )
            )
    )
}

@Composable
fun SilkLabel(
    text: String,
    color: Color = HwMuted,
    size: Int = 10,
    bold: Boolean = true,
    mono: Boolean = false,
    modifier: Modifier = Modifier
) {
    Text(
        text = text,
        color = color,
        fontSize = size.sp,
        fontWeight = if (bold) FontWeight.Bold else FontWeight.Medium,
        fontFamily = if (mono) FontFamily.Monospace else FontFamily.SansSerif,
        letterSpacing = 0.6.sp,
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        modifier = modifier
    )
}

/** Hardware module enclosure with retro silkscreen accent stripe. */
@Composable
fun ModulePanel(
    title: String,
    accent: Color = HwOrange,
    trailing: @Composable RowScope.() -> Unit = {},
    content: @Composable ColumnScope.() -> Unit
) {
    val shape = RoundedCornerShape(10.dp)
    Column(
        Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(HwFaceplate)
            .border(1.2.dp, accent.copy(alpha = 0.55f), shape)
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Box(Modifier.size(width = 4.dp, height = 14.dp).clip(RoundedCornerShape(2.dp)).background(accent))
            SilkLabel(title.uppercase(), color = HwSilk, size = 11, modifier = Modifier.weight(1f))
            trailing()
        }
        Box(Modifier.fillMaxWidth().height(1.dp).background(accent.copy(alpha = 0.25f)))
        content()
    }
}

/** Tactile hardware push button with optional status LED dot. */
@Composable
fun HwButton(
    text: String,
    modifier: Modifier = Modifier,
    on: Boolean = false,
    ledColor: Color = HwOrange,
    enabled: Boolean = true,
    height: Dp = 36.dp,
    pad: Dp = 8.dp,
    showLed: Boolean = false,
    onClick: () -> Unit
) {
    val shape = RoundedCornerShape(7.dp)
    val bg = when {
        !enabled -> HwRecess
        on -> ledColor
        else -> Color(0xFF2B2E35)
    }
    Box(
        modifier
            .height(height)
            .clip(shape)
            .background(bg)
            .border(1.dp, if (on) ledColor else HwLine, shape)
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = pad),
        contentAlignment = Alignment.Center
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(5.dp)
        ) {
            if (showLed) {
                Box(
                    Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(if (on) Color.White else HwLedOff)
                )
            }
            SilkLabel(
                text = text,
                color = if (!enabled) HwMuted.copy(alpha = 0.4f) else if (on) HwPadInk else HwSilk,
                size = 11
            )
        }
    }
}

/** Glowing red retro 7-segment style digital BPM readout. */
@Composable
fun SevenSegmentBpm(bpm: Int, modifier: Modifier = Modifier) {
    val shape = RoundedCornerShape(6.dp)
    Box(
        modifier
            .clip(shape)
            .background(Color(0xFF0A0505))
            .border(1.dp, Color(0xFF421A1A), shape)
            .padding(horizontal = 10.dp, vertical = 4.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = bpm.toString().padStart(3, ' '),
            color = Color(0xFFFF3B30),
            fontSize = 18.sp,
            fontWeight = FontWeight.ExtraBold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 1.5.sp
        )
    }
}

/**
 * Tactile analog skirted rotary knob.
 * Drag vertically up/down to adjust. One drag gesture = one undo step.
 * Shows a glowing cyan modulation ring when modulated by the LFO.
 */
@Composable
fun RotaryKnob(
    s: AppState,
    label: String,
    id: Int,
    index: Int,
    min: Float,
    max: Float,
    modifier: Modifier = Modifier,
    capColor: Color = Color(0xFF323640),
    modulated: Boolean = false,
    fmt: (Float) -> String
) {
    var v by remember(s.rev, id, index) { mutableFloatStateOf(Native.getParam(id, index)) }
    var dragging by remember { mutableStateOf(false) }
    val norm = ((v - min) / (max - min)).coerceIn(0f, 1f)

    Column(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(HwRecess)
            .border(1.dp, if (modulated) HwCyan.copy(alpha = 0.7f) else HwLine, RoundedCornerShape(8.dp))
            .padding(vertical = 6.dp, horizontal = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(3.dp)
    ) {
        SilkLabel(label.uppercase(), color = if (modulated) HwCyan else HwMuted, size = 9)
        Canvas(
            modifier = Modifier
                .size(50.dp)
                .pointerInput(id, index, min, max) {
                    detectVerticalDragGestures(
                        onDragStart = {
                            dragging = true
                            Native.beginEdit()
                        },
                        onDragEnd = {
                            dragging = false
                            s.rev++
                        },
                        onDragCancel = {
                            dragging = false
                            s.rev++
                        },
                        onVerticalDrag = { _, dragAmount ->
                            val delta = -dragAmount / 180f * (max - min)
                            v = (v + delta).coerceIn(min, max)
                            Native.setParam(id, index, v)
                        }
                    )
                }
        ) {
            val r = size.minDimension / 2f - 4.dp.toPx()
            val c = Offset(size.width / 2f, size.height / 2f)

            // Outer track arc (135 deg to 405 deg = 270 deg sweep)
            drawArc(
                color = HwLine,
                startAngle = 135f,
                sweepAngle = 270f,
                useCenter = false,
                topLeft = Offset(c.x - r, c.y - r),
                size = Size(r * 2, r * 2),
                style = Stroke(width = 3.dp.toPx(), cap = StrokeCap.Round)
            )
            // Active value arc (glows cyan if LFO-modulated)
            drawArc(
                color = if (modulated) HwCyan else HwOrange,
                startAngle = 135f,
                sweepAngle = 270f * norm,
                useCenter = false,
                topLeft = Offset(c.x - r, c.y - r),
                size = Size(r * 2, r * 2),
                style = Stroke(width = 3.5.dp.toPx(), cap = StrokeCap.Round)
            )
            // Skirted knob body
            val bodyR = r * 0.74f
            drawCircle(color = Color(0xFF111216), radius = bodyR + 2f, center = c)
            drawCircle(color = capColor, radius = bodyR, center = c)
            drawCircle(
                color = Color.White.copy(alpha = 0.14f),
                radius = bodyR,
                center = c,
                style = Stroke(width = 1.dp.toPx())
            )
            // Crisp white pointer line
            val angleRad = (135.0 + 270.0 * norm) * PI / 180.0
            val pStart = Offset(c.x + (bodyR * 0.25f) * cos(angleRad).toFloat(), c.y + (bodyR * 0.25f) * sin(angleRad).toFloat())
            val pEnd = Offset(c.x + (bodyR * 0.88f) * cos(angleRad).toFloat(), c.y + (bodyR * 0.88f) * sin(angleRad).toFloat())
            drawLine(color = HwSilk, start = pStart, end = pEnd, strokeWidth = 2.5.dp.toPx(), cap = StrokeCap.Round)
        }
        SilkLabel(fmt(v), color = HwSilk, size = 10, mono = true)
    }
}

/** Iconic TR-808 4x4 color-coded step button with top LED dot. */
@Composable
fun Tr808StepButton(
    stepInBar: Int,
    vel: Int,
    playhead: Boolean,
    modifier: Modifier = Modifier,
    onTap: () -> Unit
) {
    val groupColor = when ((stepInBar / 4) % 4) {
        0 -> HwRed
        1 -> HwOrange
        2 -> HwYellow
        else -> HwCream
    }
    val shape = RoundedCornerShape(6.dp)
    val isRoll = vel >= 3
    val bg = when {
        isRoll -> HwCyan
        vel == 2 -> groupColor
        vel == 1 -> groupColor.copy(alpha = 0.72f)
        else -> Color(0xFF282B33)
    }
    val stepLabel = when (vel) {
        3 -> "2x"
        4 -> "3x"
        5 -> "4x"
        else -> "${stepInBar + 1}"
    }
    Column(
        modifier = modifier
            .height(48.dp)
            .clip(shape)
            .background(bg)
            .border(
                width = if (playhead) 2.dp else 1.dp,
                color = if (playhead) HwGreen else if (vel >= 2) Color.White else HwLine,
                shape = shape
            )
            .clickable(onClick = onTap)
            .padding(vertical = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween
    ) {
        Box(
            Modifier
                .size(6.dp)
                .clip(CircleShape)
                .background(
                    when {
                        playhead -> HwGreen
                        isRoll -> Color.White
                        vel == 2 -> Color.White
                        vel == 1 -> Color(0xFFFF3B30)
                        else -> HwLedOff
                    }
                )
        )
        SilkLabel(
            text = stepLabel,
            color = if (vel > 0) HwPadInk else HwMuted,
            size = 9,
            mono = true
        )
    }
}

/** Recessed vintage CRT green oscilloscope visualizing synth wave / FM character. */
@Composable
fun OledScopeDisplay(
    isFm: Boolean,
    wave: Int,
    cutoffNorm: Float,
    fmIndex: Float,
    fmRatio: Float,
    modifier: Modifier = Modifier
) {
    val shape = RoundedCornerShape(8.dp)
    Box(
        modifier
            .fillMaxWidth()
            .height(66.dp)
            .clip(shape)
            .background(Color(0xFF07140D))
            .border(1.5.dp, Color(0xFF254434), shape)
            .padding(6.dp)
    ) {
        Canvas(Modifier.fillMaxSize()) {
            // Subtle CRT graticule grid
            val gridColor = Color(0xFF143524)
            for (i in 1..3) {
                val y = size.height * i / 4f
                drawLine(gridColor, Offset(0f, y), Offset(size.width, y), 1f)
            }
            for (i in 1..7) {
                val x = size.width * i / 8f
                drawLine(gridColor, Offset(x, 0f), Offset(x, size.height), 1f)
            }
            val path = Path()
            val samples = 120
            val midY = size.height / 2f
            val amp = size.height * 0.38f
            val cycles = 2.0
            for (i in 0..samples) {
                val u = i.toFloat() / samples.toFloat()
                val t = (u * cycles) % 1.0
                val raw = if (isFm) {
                    val mod = sin(2.0 * PI * u * cycles * fmRatio)
                    sin(2.0 * PI * u * cycles + fmIndex * 0.65 * mod)
                } else {
                    when (wave) {
                        0 -> 2.0 * t - 1.0                                      // Saw
                        1 -> if (t < 0.5) 1.0 else -1.0                         // Square
                        2 -> 4.0 * kotlin.math.abs(t - 0.5) - 1.0               // Triangle
                        else -> sin(2.0 * PI * t)                               // Sine
                    }
                }
                // Visual lowpass softening based on cutoff
                val smooth = sin(2.0 * PI * t)
                val shaped = smooth * (1f - cutoffNorm) + raw.toFloat() * cutoffNorm
                val px = u * size.width
                val py = midY - shaped * amp
                if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
            }
            drawPath(path, color = HwGreen.copy(alpha = 0.28f), style = Stroke(width = 6f, cap = StrokeCap.Round))
            drawPath(path, color = HwGreen, style = Stroke(width = 2.2f, cap = StrokeCap.Round))
        }
        SilkLabel(
            text = if (isFm) "FM 2-OP" else listOf("SAW", "SQR", "TRI", "SIN").getOrElse(wave) { "SAW" },
            color = HwGreen.copy(alpha = 0.85f),
            size = 9,
            mono = true,
            modifier = Modifier.align(Alignment.TopEnd)
        )
    }
}

/** Backlit vintage stereo analog VU needle meter with Limiter Active LED. */
@Composable
fun AnalogVuMeter(peakL: Float, peakR: Float, modifier: Modifier = Modifier) {
    val limiting = peakL > 0.91f || peakR > 0.91f
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        SingleVuDial("VU L", peakL, Modifier.weight(1f))
        SingleVuDial("VU R", peakR, Modifier.weight(1f))
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(4.dp),
            modifier = Modifier.width(56.dp)
        ) {
            Box(
                Modifier
                    .size(14.dp)
                    .clip(CircleShape)
                    .background(if (limiting) HwRed else HwGreen)
                    .border(1.5.dp, Color.White.copy(alpha = 0.4f), CircleShape)
            )
            SilkLabel("LIMITER", color = HwSilk, size = 8)
            SilkLabel(if (limiting) "ACTIVE" else "READY", color = if (limiting) HwOrange else HwGreen, size = 8)
        }
    }
}

@Composable
private fun SingleVuDial(label: String, level: Float, modifier: Modifier = Modifier) {
    val f = (level / 0.944f).coerceIn(0f, 1f)
    val shape = RoundedCornerShape(8.dp)
    Box(
        modifier
            .height(64.dp)
            .clip(shape)
            .background(
                Brush.verticalGradient(listOf(Color(0xFFF3DFA8), Color(0xFFD8B76A)))
            )
            .border(1.5.dp, Color(0xFF4A3C22), shape)
            .padding(4.dp)
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val pivot = Offset(size.width / 2f, size.height * 1.15f)
            val rad = size.height * 0.95f
            // Scale arc: -42 deg to +42 deg around straight up (-90 deg)
            drawArc(
                color = Color(0xFF3E3018),
                startAngle = 228f,
                sweepAngle = 60f,
                useCenter = false,
                topLeft = Offset(pivot.x - rad, pivot.y - rad),
                size = Size(rad * 2, rad * 2),
                style = Stroke(width = 2.dp.toPx())
            )
            // Red peak zone
            drawArc(
                color = HwRed,
                startAngle = 288f,
                sweepAngle = 24f,
                useCenter = false,
                topLeft = Offset(pivot.x - rad, pivot.y - rad),
                size = Size(rad * 2, rad * 2),
                style = Stroke(width = 3.dp.toPx())
            )
            // Needle
            val deg = 228.0 + 84.0 * f
            val radAng = deg * PI / 180.0
            val tip = Offset(pivot.x + rad * 0.95f * cos(radAng).toFloat(), pivot.y + rad * 0.95f * sin(radAng).toFloat())
            drawLine(Color(0xFF1A1510), pivot, tip, strokeWidth = 2.dp.toPx(), cap = StrokeCap.Round)
        }
        SilkLabel(label, color = Color(0xFF3A2D16), size = 9, modifier = Modifier.align(Alignment.BottomCenter))
    }
}
