package com.loopdraft.app

import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
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

// ---- Base Hardware Palette Constants (fallback & semantic accents) ----
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

/** Side rails framing the hardware faceplate (Walnut wood, Rack ears with screws, Cyber edge, etc.). */
@Composable
fun WalnutRail(modifier: Modifier = Modifier) {
    val skin = LocalSkin.current
    Canvas(
        modifier
            .width(8.dp)
            .fillMaxHeight()
    ) {
        drawRect(
            brush = Brush.horizontalGradient(listOf(skin.railDark, skin.railLight, skin.railDark)),
            size = size
        )
        val screw = skin.railScrew
        if (screw != null && size.height > 80f) {
            val positions = listOf(0.06f, 0.22f, 0.50f, 0.78f, 0.94f)
            val r = 2.1.dp.toPx()
            for (p in positions) {
                val cy = size.height * p
                drawCircle(Color.Black.copy(alpha = 0.45f), r + 0.8f, Offset(size.width / 2f, cy))
                drawCircle(screw, r, Offset(size.width / 2f, cy))
            }
        }
    }
}

@Composable
fun SilkLabel(
    text: String,
    color: Color? = null,
    size: Int = 10,
    bold: Boolean = true,
    mono: Boolean = false,
    modifier: Modifier = Modifier
) {
    val skin = LocalSkin.current
    Text(
        text = text,
        color = color ?: skin.muted,
        fontSize = size.sp,
        fontWeight = if (bold) FontWeight.Bold else FontWeight.Medium,
        fontFamily = if (mono) FontFamily.Monospace else FontFamily.SansSerif,
        letterSpacing = 0.5.sp,
        maxLines = 1,
        overflow = TextOverflow.Ellipsis,
        modifier = modifier
    )
}

/** Clean 3-layer segmented bar (Quick Start -> Dig Deeper). */
@Composable
fun LayerSwitcher(
    layers: List<String>,
    selected: Int,
    accent: Color,
    onSelect: (Int) -> Unit,
    trailing: @Composable RowScope.() -> Unit = {}
) {
    val skin = LocalSkin.current
    val outer = RoundedCornerShape(skin.panelRadius)
    Row(
        Modifier
            .fillMaxWidth()
            .clip(outer)
            .background(skin.recess)
            .border(1.dp, skin.line, outer)
            .padding(3.dp),
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        layers.forEachIndexed { idx, title ->
            val on = selected == idx
            val pill = RoundedCornerShape((skin.buttonRadius - 1.dp).coerceAtLeast(3.dp))
            Box(
                Modifier
                    .weight(1f)
                    .height(31.dp)
                    .clip(pill)
                    .background(if (on) accent else Color.Transparent)
                    .clickable { onSelect(idx) },
                contentAlignment = Alignment.Center
            ) {
                SilkLabel(
                    text = title,
                    color = if (on) skin.onAccentInk else skin.muted,
                    size = 10
                )
            }
        }
        trailing()
    }
}

/** Hardware module enclosure with retro silkscreen accent stripe. */
@Composable
fun ModulePanel(
    title: String,
    accent: Color? = null,
    trailing: @Composable RowScope.() -> Unit = {},
    content: @Composable ColumnScope.() -> Unit
) {
    val skin = LocalSkin.current
    val acc = accent ?: skin.primary
    val shape = RoundedCornerShape(skin.panelRadius)
    Column(
        Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(skin.faceplate)
            .border(1.2.dp, acc.copy(alpha = if (skin.isLight) 0.7f else 0.52f), shape)
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Box(Modifier.size(width = 4.dp, height = 14.dp).clip(RoundedCornerShape(2.dp)).background(acc))
            SilkLabel(title.uppercase(), color = skin.silk, size = 11, modifier = Modifier.weight(1f))
            trailing()
        }
        Box(Modifier.fillMaxWidth().height(1.dp).background(acc.copy(alpha = 0.25f)))
        content()
    }
}

/** Tactile hardware push button styled by active ButtonSkin & FrameSkin. */
@Composable
fun HwButton(
    text: String,
    modifier: Modifier = Modifier,
    on: Boolean = false,
    ledColor: Color? = null,
    enabled: Boolean = true,
    height: Dp = 36.dp,
    pad: Dp = 8.dp,
    showLed: Boolean = false,
    onClick: () -> Unit
) {
    val skin = LocalSkin.current
    val prefs = LocalUiPrefs.current
    val haptic = LocalHapticFeedback.current
    val activeCol = ledColor ?: skin.primary
    val shape = RoundedCornerShape(skin.buttonRadius)
    val btnSkin = skin.config.button

    val bg = when {
        !enabled -> skin.recess
        btnSkin == ButtonSkin.GLASS_NEON -> if (on) activeCol.copy(alpha = 0.22f) else skin.recess.copy(alpha = 0.85f)
        btnSkin == ButtonSkin.MPC_RUBBER -> if (on) activeCol else skin.buttonOff
        btnSkin == ButtonSkin.ALU_SWITCH -> if (on) activeCol else skin.buttonOff
        else -> if (on) activeCol else skin.buttonOff
    }
    val borderColor = when {
        !enabled -> skin.line.copy(alpha = 0.5f)
        btnSkin == ButtonSkin.GLASS_NEON -> if (on) activeCol else skin.line
        btnSkin == ButtonSkin.MPC_RUBBER -> if (on) Color.White.copy(alpha = 0.7f) else skin.line
        else -> if (on) activeCol else skin.line
    }
    val borderWidth = if (btnSkin == ButtonSkin.GLASS_NEON && on) 1.6.dp else if (btnSkin == ButtonSkin.MPC_RUBBER) 1.5.dp else 1.dp
    val textColor = when {
        !enabled -> skin.muted.copy(alpha = 0.4f)
        btnSkin == ButtonSkin.GLASS_NEON && on -> activeCol
        on -> skin.onAccentInk
        else -> skin.silk
    }

    Box(
        modifier
            .height(height)
            .clip(shape)
            .background(bg)
            .border(borderWidth, borderColor, shape)
            .clickable(enabled = enabled, onClick = { if (prefs.haptics) haptic.performHapticFeedback(HapticFeedbackType.TextHandleMove); onClick() })
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
                        .size(if (btnSkin == ButtonSkin.MPC_RUBBER) 7.dp else 6.dp)
                        .clip(if (btnSkin == ButtonSkin.ALU_SWITCH) RoundedCornerShape(1.5.dp) else CircleShape)
                        .background(
                            if (on) (if (btnSkin == ButtonSkin.GLASS_NEON) activeCol else Color.White)
                            else skin.ledOff
                        )
                )
            }
            SilkLabel(text = text, color = textColor, size = 11)
        }
    }
}

/** Glowing red retro 7-segment style digital BPM readout. */
@Composable
fun SevenSegmentBpm(bpm: Int, modifier: Modifier = Modifier) {
    val skin = LocalSkin.current
    val shape = RoundedCornerShape(6.dp)
    val glow = if (skin.config.accent == AccentSkin.CYBER_PINK) skin.primary
    else if (skin.config.accent == AccentSkin.ACID_LIME) skin.primary
    else Color(0xFFFF3B30)
    Box(
        modifier
            .clip(shape)
            .background(Color(0xFF0A0505))
            .border(1.dp, glow.copy(alpha = 0.45f), shape)
            .padding(horizontal = 10.dp, vertical = 4.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = bpm.toString().padStart(3, ' '),
            color = glow,
            fontSize = 18.sp,
            fontWeight = FontWeight.ExtraBold,
            fontFamily = FontFamily.Monospace,
            letterSpacing = 1.5.sp
        )
    }
}

/**
 * Modular hardware control: renders as Left-Right Horizontal Slider (`H_SLIDER`),
 * 12-Segment LED Bar (`LED_BAR`), or one of 4 Rotary Knob skins (`SKIRTED_80S`, `MOOG_SILVER`,
 * `NEON_RING`, `BAKELITE`) based on active `LocalSkin`.
 * ALL modes respond smoothly to Left <-> Right horizontal drag so vertical scrolling stays effortless.
 */
@Composable
fun Knob(
    label: String,
    value: Float,
    min: Float,
    max: Float,
    modifier: Modifier = Modifier,
    capColor: Color = Color(0xFF323640),
    modulated: Boolean = false,
    fmt: (Float) -> String,
    onStart: () -> Unit,
    onChange: (Float) -> Unit,
    onEnd: () -> Unit,
    onReset: (() -> Unit)? = null      // double-tap: back to the default
) {
    val skin = LocalSkin.current
    val prefs = LocalUiPrefs.current
    val haptic = LocalHapticFeedback.current
    val reset by rememberUpdatedState(onReset)
    val knobStyle = skin.config.knob
    val cur = remember { mutableFloatStateOf(value) }
    var dragging by remember { mutableStateOf(false) }
    LaunchedEffect(value) { if (!dragging) cur.floatValue = value }
    val start by rememberUpdatedState(onStart)
    val change by rememberUpdatedState(onChange)
    val end by rememberUpdatedState(onEnd)
    val v = cur.floatValue
    val span = (max - min).coerceAtLeast(1e-6f)
    val norm = ((v - min) / span).coerceIn(0f, 1f)
    val isBipolar = min < -1e-4f && max > 1e-4f
    val zeroNorm = if (isBipolar) ((-min) / span).coerceIn(0f, 1f) else 0f
    val ring = if (modulated) skin.secondary else skin.primary
    val isSlider = knobStyle == KnobSkin.H_SLIDER || knobStyle == KnobSkin.LED_BAR

    val tileShape = RoundedCornerShape(skin.buttonRadius)
    val borderCol = when {
        dragging -> skin.silk
        modulated -> skin.secondary.copy(alpha = 0.75f)
        else -> skin.line
    }

    Column(
        modifier = modifier
            .clip(tileShape)
            .background(skin.recess)
            .border(if (dragging) 1.5.dp else 1.dp, borderCol, tileShape)
            .pointerInput(onReset != null, prefs.haptics) {
                detectTapGestures(onDoubleTap = {
                    val r = reset
                    if (r != null) {
                        if (prefs.haptics) haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                        r()
                    }
                })
            }
            .pointerInput(min, max, isSlider, prefs.speed) {
                // Horizontal drag: left decreases, right increases. Sliders feel crisp & direct; rotary knobs fine & smooth.
                // CONTROLS in the setup stretches or shortens the finger travel.
                val fullTravel = (if (isSlider) maxOf(size.width.toFloat() * 0.95f, 120.dp.toPx()) else 200.dp.toPx()) * prefs.travelFactor
                detectHorizontalDragGestures(
                    onDragStart = {
                        dragging = true
                        start()
                    },
                    onDragEnd = {
                        dragging = false
                        end()
                    },
                    onDragCancel = {
                        dragging = false
                        end()
                    },
                    onHorizontalDrag = { ch, dragAmount ->
                        ch.consume()
                        val nv = (cur.floatValue + dragAmount / fullTravel * span).coerceIn(min, max)
                        cur.floatValue = nv
                        change(nv)
                    }
                )
            }
            .padding(vertical = if (isSlider) 6.dp else 6.dp, horizontal = if (isSlider) 6.dp else 2.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(if (isSlider) 5.dp else 3.dp)
    ) {
        if (isSlider) {
            // Compact label + live value header for Left-Right Sliders
            Row(
                Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                SilkLabel(label.uppercase(), color = if (modulated) skin.secondary else skin.muted, size = 8, modifier = Modifier.weight(1f))
                SilkLabel(fmt(v), color = if (dragging) ring else skin.silk, size = 9, mono = true)
            }
            if (knobStyle == KnobSkin.H_SLIDER) {
                // Tactile horizontal fader slider (Left <-> Right)
                Canvas(modifier = Modifier.fillMaxWidth().height(26.dp)) {
                    val trackH = 7.dp.toPx()
                    val cy = size.height / 2f
                    val padX = 8.dp.toPx()
                    val trackW = (size.width - padX * 2f).coerceAtLeast(8f)
                    val trackTop = cy - trackH / 2f

                    // Subtle scale ticks along the bottom edge
                    for (t in 0..4) {
                        val tx = padX + trackW * (t / 4f)
                        val tickH = if (t == 2 && isBipolar) 4.dp.toPx() else 2.5.dp.toPx()
                        drawLine(
                            color = skin.muted.copy(alpha = 0.35f),
                            start = Offset(tx, cy + trackH / 2f + 2.dp.toPx()),
                            end = Offset(tx, cy + trackH / 2f + 2.dp.toPx() + tickH),
                            strokeWidth = 1.dp.toPx()
                        )
                    }

                    // Recessed slot groove
                    drawRoundRect(
                        color = if (skin.isLight) Color(0xFFBEB7A8) else Color(0xFF090A0D),
                        topLeft = Offset(padX, trackTop),
                        size = Size(trackW, trackH),
                        cornerRadius = CornerRadius(trackH / 2f, trackH / 2f)
                    )
                    drawRoundRect(
                        color = skin.line.copy(alpha = 0.7f),
                        topLeft = Offset(padX, trackTop),
                        size = Size(trackW, trackH),
                        cornerRadius = CornerRadius(trackH / 2f, trackH / 2f),
                        style = Stroke(width = 1.dp.toPx())
                    )

                    // Active glowing fill (center-origin for bipolar, left-origin for unipolar)
                    val thumbX = padX + trackW * norm
                    val fillStart = if (isBipolar) padX + trackW * minOf(zeroNorm, norm) else padX
                    val fillEnd = if (isBipolar) padX + trackW * maxOf(zeroNorm, norm) else thumbX
                    if (fillEnd - fillStart > 1f) {
                        drawRoundRect(
                            color = ring,
                            topLeft = Offset(fillStart, trackTop + 1f),
                            size = Size(fillEnd - fillStart, trackH - 2f),
                            cornerRadius = CornerRadius(trackH / 2f, trackH / 2f)
                        )
                    }

                    // Hardware fader thumb cap with vertical center notch
                    val thumbW = 13.dp.toPx()
                    val thumbH = 20.dp.toPx()
                    val thumbLeft = thumbX - thumbW / 2f
                    val thumbTop = cy - thumbH / 2f
                    drawRoundRect(
                        color = Color.Black.copy(alpha = 0.45f),
                        topLeft = Offset(thumbLeft + 1f, thumbTop + 1.5f),
                        size = Size(thumbW, thumbH),
                        cornerRadius = CornerRadius(3.5.dp.toPx(), 3.5.dp.toPx())
                    )
                    val capFill = if (dragging) skin.silk else if (skin.isLight) Color(0xFF32343A) else Color(0xFFDCD6C6)
                    drawRoundRect(
                        color = capFill,
                        topLeft = Offset(thumbLeft, thumbTop),
                        size = Size(thumbW, thumbH),
                        cornerRadius = CornerRadius(3.5.dp.toPx(), 3.5.dp.toPx())
                    )
                    drawRoundRect(
                        color = if (dragging) ring else Color.Black.copy(alpha = 0.35f),
                        topLeft = Offset(thumbLeft, thumbTop),
                        size = Size(thumbW, thumbH),
                        cornerRadius = CornerRadius(3.5.dp.toPx(), 3.5.dp.toPx()),
                        style = Stroke(width = 1.dp.toPx())
                    )
                    // Center hairline indicator
                    drawLine(
                        color = if (dragging) ring else if (skin.isLight) Color.White else Color(0xFF1A1B20),
                        start = Offset(thumbX, thumbTop + 3.dp.toPx()),
                        end = Offset(thumbX, thumbTop + thumbH - 3.dp.toPx()),
                        strokeWidth = 2.dp.toPx(),
                        cap = StrokeCap.Round
                    )
                }
            } else {
                // LED_BAR: 12-segment illuminated left-right bar slider
                Canvas(modifier = Modifier.fillMaxWidth().height(26.dp)) {
                    val segs = 12
                    val gap = 2.2.dp.toPx()
                    val segW = ((size.width - gap * (segs - 1)) / segs).coerceAtLeast(2f)
                    val barH = 14.dp.toPx()
                    val topY = (size.height - barH) / 2f
                    val activeIdx = (norm * segs).roundToInt().coerceIn(0, segs)
                    val zeroIdx = (zeroNorm * segs).roundToInt().coerceIn(0, segs)
                    for (i in 0 until segs) {
                        val lit = if (isBipolar) {
                            if (activeIdx >= zeroIdx) i in zeroIdx until activeIdx else i in activeIdx until zeroIdx
                        } else {
                            i < activeIdx
                        }
                        val isTip = lit && (if (isBipolar) (i == activeIdx - 1 || i == activeIdx) else i == activeIdx - 1)
                        val col = when {
                            isTip && dragging -> Color.White
                            lit -> ring.copy(alpha = if (isTip) 1f else 0.78f)
                            else -> if (skin.isLight) Color(0xFFC5BFB0) else Color(0xFF1D2028)
                        }
                        drawRoundRect(
                            color = col,
                            topLeft = Offset(i * (segW + gap), topY),
                            size = Size(segW, barH),
                            cornerRadius = CornerRadius(2.dp.toPx(), 2.dp.toPx())
                        )
                    }
                }
            }
        } else {
            // Rotary Knob Skins (SKIRTED_80S, MOOG_SILVER, NEON_RING, BAKELITE) — also dragged left <-> right
            SilkLabel(label.uppercase(), color = if (modulated) skin.secondary else skin.muted, size = 9)
            Canvas(modifier = Modifier.fillMaxWidth().height(50.dp)) {
                val r = minOf(size.height / 2f - 4.dp.toPx(), size.width / 2f - 14.dp.toPx()).coerceAtLeast(10.dp.toPx())
                val c = Offset(size.width / 2f, size.height / 2f)
                val room = size.width / 2f - r
                if (room > 11.dp.toPx()) {
                    val hint = if (dragging) ring else skin.silk.copy(alpha = 0.22f)
                    val ax = r + maxOf(room * 0.55f, 8.dp.toPx())
                    val ah = 4.dp.toPx()
                    val aw = 3.dp.toPx()
                    val sw = 1.5.dp.toPx()
                    drawLine(hint, Offset(c.x - ax + aw, c.y - ah), Offset(c.x - ax, c.y), strokeWidth = sw, cap = StrokeCap.Round)
                    drawLine(hint, Offset(c.x - ax, c.y), Offset(c.x - ax + aw, c.y + ah), strokeWidth = sw, cap = StrokeCap.Round)
                    drawLine(hint, Offset(c.x + ax - aw, c.y - ah), Offset(c.x + ax, c.y), strokeWidth = sw, cap = StrokeCap.Round)
                    drawLine(hint, Offset(c.x + ax, c.y), Offset(c.x + ax - aw, c.y + ah), strokeWidth = sw, cap = StrokeCap.Round)
                }

                // Value arc
                drawArc(
                    color = skin.line, startAngle = 135f, sweepAngle = 270f, useCenter = false,
                    topLeft = Offset(c.x - r, c.y - r), size = Size(r * 2, r * 2),
                    style = Stroke(width = 3.dp.toPx(), cap = StrokeCap.Round)
                )
                if (knobStyle == KnobSkin.NEON_RING) {
                    drawArc(
                        color = ring.copy(alpha = 0.28f), startAngle = 135f, sweepAngle = 270f * norm, useCenter = false,
                        topLeft = Offset(c.x - r, c.y - r), size = Size(r * 2, r * 2),
                        style = Stroke(width = 7.dp.toPx(), cap = StrokeCap.Round)
                    )
                }
                drawArc(
                    color = ring, startAngle = 135f, sweepAngle = 270f * norm, useCenter = false,
                    topLeft = Offset(c.x - r, c.y - r), size = Size(r * 2, r * 2),
                    style = Stroke(width = 3.5.dp.toPx(), cap = StrokeCap.Round)
                )

                val bodyR = r * 0.74f
                val angleRad = (135.0 + 270.0 * norm) * PI / 180.0
                val ca = cos(angleRad).toFloat()
                val sa = sin(angleRad).toFloat()

                when (knobStyle) {
                    KnobSkin.MOOG_SILVER -> {
                        drawCircle(Color(0xFF101216), bodyR + 1.5f, c)
                        drawCircle(Color(0xFF23262E), bodyR, c)
                        drawCircle(
                            brush = Brush.linearGradient(listOf(Color(0xFFE6ECF5), Color(0xFF96A1B2), Color(0xFFD5DDE8))),
                            radius = bodyR * 0.68f,
                            center = c
                        )
                        drawLine(
                            color = Color(0xFF14161A),
                            start = Offset(c.x + bodyR * 0.15f * ca, c.y + bodyR * 0.15f * sa),
                            end = Offset(c.x + bodyR * 0.92f * ca, c.y + bodyR * 0.92f * sa),
                            strokeWidth = 2.6.dp.toPx(), cap = StrokeCap.Round
                        )
                    }
                    KnobSkin.NEON_RING -> {
                        drawCircle(Color(0xFF080A10), bodyR, c)
                        drawCircle(ring.copy(alpha = 0.6f), bodyR, c, style = Stroke(1.3.dp.toPx()))
                        drawLine(
                            color = ring,
                            start = Offset(c.x + bodyR * 0.2f * ca, c.y + bodyR * 0.2f * sa),
                            end = Offset(c.x + bodyR * 0.9f * ca, c.y + bodyR * 0.9f * sa),
                            strokeWidth = 2.6.dp.toPx(), cap = StrokeCap.Round
                        )
                    }
                    KnobSkin.BAKELITE -> {
                        drawCircle(Color(0xFF1C1410), bodyR + 1.5f, c)
                        drawCircle(Color(0xFF3A281E), bodyR, c)
                        drawCircle(Color(0xFF5A3F30), bodyR * 0.78f, c, style = Stroke(1.dp.toPx()))
                        drawLine(
                            color = Color(0xFFF5ECD8),
                            start = Offset(c.x - bodyR * 0.15f * ca, c.y - bodyR * 0.15f * sa),
                            end = Offset(c.x + bodyR * 0.94f * ca, c.y + bodyR * 0.94f * sa),
                            strokeWidth = 3.dp.toPx(), cap = StrokeCap.Round
                        )
                    }
                    else -> {
                        // SKIRTED_80S
                        drawCircle(color = Color(0xFF111216), radius = bodyR + 2f, center = c)
                        drawCircle(color = capColor, radius = bodyR, center = c)
                        drawCircle(color = Color.White.copy(alpha = 0.14f), radius = bodyR, center = c, style = Stroke(width = 1.dp.toPx()))
                        drawLine(
                            color = Color(0xFFEBE5D5),
                            start = Offset(c.x + bodyR * 0.25f * ca, c.y + bodyR * 0.25f * sa),
                            end = Offset(c.x + bodyR * 0.88f * ca, c.y + bodyR * 0.88f * sa),
                            strokeWidth = 2.5.dp.toPx(), cap = StrokeCap.Round
                        )
                    }
                }
            }
            SilkLabel(fmt(v), color = skin.silk, size = 10, mono = true)
        }
    }
}

/**
 * A control bound to one engine parameter (renders as Left-Right Slider or Rotary Knob per Skin).
 * Supports Song Automation Picking (`s.pickAuto`): automatable controls frame yellow.
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
    val skin = LocalSkin.current
    val value = remember(s.rev, id, index) { Native.getParam(id, index) }
    Box(modifier) {
        Knob(
            label, value, min, max, Modifier.fillMaxWidth(), capColor, modulated, fmt,
            onStart = { Native.beginEdit() },
            onChange = { Native.setParam(id, index, it) },
            onEnd = { s.rev++ },
            onReset = { Native.beginEdit(); Native.setParam(id, index, Native.defaultParam(id, index)); s.rev++ }
        )
        if (s.pickAuto) {
            val shape = RoundedCornerShape(skin.buttonRadius)
            if (AUTO_PARAMS.containsKey(id)) {
                Box(
                    Modifier.matchParentSize().clip(shape)
                        .background(HwYellow.copy(alpha = 0.16f))
                        .border(2.dp, HwYellow, shape)
                        .clickable { s.addAutomation(id, index) }
                )
            } else {
                Box(Modifier.matchParentSize().clip(shape).background(Color.Black.copy(alpha = 0.5f)).clickable { })
            }
        }
    }
}

/** The two-colour logo. */
@Composable
fun Logo() {
    val skin = LocalSkin.current
    Row {
        SilkLabel("LOOP", color = skin.silk, size = 17)
        SilkLabel("DRAFT", color = skin.primary, size = 17)
    }
}

/** Sample display: the outline of the selected pad's sound, with the played slice lit up. */
@Composable
fun WaveformDisplay(peaks: FloatArray, start: Float, end: Float, accent: Color, modifier: Modifier = Modifier) {
    val skin = LocalSkin.current
    val shape = RoundedCornerShape(6.dp)
    Box(
        modifier
            .height(44.dp)
            .clip(shape)
            .background(Color(0xFF0B0C0F))
            .border(1.dp, skin.line, shape)
            .padding(horizontal = 4.dp, vertical = 3.dp)
    ) {
        Canvas(Modifier.fillMaxSize()) {
            val n = peaks.size
            if (n == 0) return@Canvas
            val w = size.width / n
            val mid = size.height / 2f
            for (i in 0 until n) {
                val u = (i + 0.5f) / n
                val inside = u >= start && u <= end
                val h = (peaks[i] * mid).coerceAtLeast(1f)
                val x = i * w + w / 2f
                drawLine(
                    color = if (inside) Color(0xFFEBE5D5) else Color(0xFF7A766E).copy(alpha = 0.35f),
                    start = Offset(x, mid - h), end = Offset(x, mid + h),
                    strokeWidth = (w * 0.7f).coerceAtLeast(1f)
                )
            }
            if (start > 0.001f || end < 0.999f) {
                drawLine(accent, Offset(start * size.width, 0f), Offset(start * size.width, size.height), 2.dp.toPx())
                drawLine(accent, Offset(end * size.width, 0f), Offset(end * size.width, size.height), 2.dp.toPx())
            }
        }
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
    val skin = LocalSkin.current
    val groupColor = when ((stepInBar / 4) % 4) {
        0 -> HwRed
        1 -> skin.primary
        2 -> skin.tertiary
        else -> HwCream
    }
    val shape = RoundedCornerShape(skin.buttonRadius)
    val isRoll = vel >= 3
    val bg = when {
        isRoll -> skin.secondary
        vel == 2 -> groupColor
        vel == 1 -> groupColor.copy(alpha = 0.75f)
        else -> skin.buttonOff
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
                color = if (playhead) skin.positive else if (vel >= 2) Color.White else skin.line,
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
                        playhead -> skin.positive
                        isRoll -> Color.White
                        vel == 2 -> Color.White
                        vel == 1 -> Color(0xFFFF3B30)
                        else -> skin.ledOff
                    }
                )
        )
        SilkLabel(
            text = stepLabel,
            color = if (vel > 0) skin.onAccentInk else skin.muted,
            size = 9,
            mono = true
        )
    }
}

/** Skin-aware recessed CRT/OLED oscilloscope visualizing synth wave / FM character. */
@Composable
fun OledScopeDisplay(
    isFm: Boolean,
    wave: Int,
    cutoffNorm: Float,
    fmIndex: Float,
    fmRatio: Float,
    modifier: Modifier = Modifier
) {
    val skin = LocalSkin.current
    val shape = RoundedCornerShape(8.dp)
    Box(
        modifier
            .fillMaxWidth()
            .height(64.dp)
            .clip(shape)
            .background(skin.scopeBg)
            .border(1.5.dp, skin.scopeTrace.copy(alpha = 0.35f), shape)
            .padding(6.dp)
    ) {
        Canvas(Modifier.fillMaxSize()) {
            for (i in 1..3) {
                val y = size.height * i / 4f
                drawLine(skin.scopeGrid, Offset(0f, y), Offset(size.width, y), 1f)
            }
            for (i in 1..7) {
                val x = size.width * i / 8f
                drawLine(skin.scopeGrid, Offset(x, 0f), Offset(x, size.height), 1f)
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
                        0 -> 2.0 * t - 1.0
                        1 -> if (t < 0.5) 1.0 else -1.0
                        2 -> 4.0 * kotlin.math.abs(t - 0.5) - 1.0
                        else -> sin(2.0 * PI * t)
                    }
                }
                val smooth = sin(2.0 * PI * t)
                val shaped = smooth * (1f - cutoffNorm) + raw.toFloat() * cutoffNorm
                val px = u * size.width
                val py = midY - (shaped * amp).toFloat()
                if (i == 0) path.moveTo(px, py) else path.lineTo(px, py)
            }
            drawPath(path, color = skin.scopeTrace.copy(alpha = 0.28f), style = Stroke(width = 6f, cap = StrokeCap.Round))
            drawPath(path, color = skin.scopeTrace, style = Stroke(width = 2.2f, cap = StrokeCap.Round))
        }
        SilkLabel(
            text = if (isFm) "FM 2-OP" else listOf("SAW", "SQR", "TRI", "SIN").getOrElse(wave) { "SAW" },
            color = skin.scopeTrace.copy(alpha = 0.88f),
            size = 9,
            mono = true,
            modifier = Modifier.align(Alignment.TopEnd)
        )
    }
}

/** Backlit vintage stereo analog VU needle meter with Limiter Active LED. */
@Composable
fun AnalogVuMeter(peakL: Float, peakR: Float, modifier: Modifier = Modifier) {
    val skin = LocalSkin.current
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
                    .background(if (limiting) skin.danger else skin.positive)
                    .border(1.5.dp, Color.White.copy(alpha = 0.4f), CircleShape)
            )
            SilkLabel("LIMITER", color = skin.silk, size = 8)
            SilkLabel(if (limiting) "ACTIVE" else "READY", color = if (limiting) skin.primary else skin.positive, size = 8)
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
            drawArc(
                color = Color(0xFF3E3018),
                startAngle = 228f,
                sweepAngle = 60f,
                useCenter = false,
                topLeft = Offset(pivot.x - rad, pivot.y - rad),
                size = Size(rad * 2, rad * 2),
                style = Stroke(width = 2.dp.toPx())
            )
            drawArc(
                color = HwRed,
                startAngle = 288f,
                sweepAngle = 24f,
                useCenter = false,
                topLeft = Offset(pivot.x - rad, pivot.y - rad),
                size = Size(rad * 2, rad * 2),
                style = Stroke(width = 3.dp.toPx())
            )
            val deg = 228.0 + 84.0 * f
            val radAng = deg * PI / 180.0
            val tip = Offset(pivot.x + rad * 0.95f * cos(radAng).toFloat(), pivot.y + rad * 0.95f * sin(radAng).toFloat())
            drawLine(Color(0xFF1A1510), pivot, tip, strokeWidth = 2.dp.toPx(), cap = StrokeCap.Round)
        }
        SilkLabel(label, color = Color(0xFF3A2D16), size = 9, modifier = Modifier.align(Alignment.BottomCenter))
    }
}
