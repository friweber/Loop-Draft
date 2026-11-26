package com.loopdraft.app

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.unit.dp
import kotlin.math.roundToInt

/** Stompbox metadata mapped 1:1 to engine FX slot types 1..6. */
data class PedalSpec(
    val typeId: Int,
    val code: String,
    val name: String,
    val bodyTop: Color,
    val bodyBottom: Color,
    val ink: Color,
    val param1: String,
    val param2: String
)

val PEDAL_SPECS = listOf(
    PedalSpec(1, "OD-1", "OVERDRIVE", Color(0xFFE0971B), Color(0xFFB56E0C), Color(0xFF1A1408), "DRIVE", ""),
    PedalSpec(2, "8-BIT", "BITCRUSH", Color(0xFF218C9C), Color(0xFF135B68), Color(0xFFF2FAFC), "CRUSH", "RATE"),
    PedalSpec(3, "CE-2", "CHORUS", Color(0xFF2B6EC2), Color(0xFF1A4682), Color(0xFFF2F7FF), "DEPTH", "SPEED"),
    PedalSpec(4, "PH-90", "PHASER", Color(0xFFD95621), Color(0xFFA1350D), Color(0xFFFFF5F0), "DEPTH", "SPEED"),
    PedalSpec(5, "MF-101", "LO-PASS", Color(0xFF1F7A52), Color(0xFF124D33), Color(0xFFF0FFF7), "CUTOFF", "RESO"),
    PedalSpec(6, "DYNA", "COMP", Color(0xFFC22D32), Color(0xFF871B1F), Color(0xFFFFF2F2), "SQUASH", "")
)

// The pedals of Loop Master's collection. The phone does not offer them, but shows and plays them when a project brings them.
val MASTER_PEDAL_SPECS = listOf(
    PedalSpec(7, "DST", "DISTORTION", Color(0xFFE0661B), Color(0xFFA8430C), Color(0xFFF7F7F7), "GAIN", "TONE"),
    PedalSpec(8, "FUZZ", "FUZZ", Color(0xFF7A3FB5), Color(0xFF4F2480), Color(0xFFF7F7F7), "FUZZ", "TONE"),
    PedalSpec(9, "BST", "BOOSTER", Color(0xFFE8E2D0), Color(0xFFBDB59C), Color(0xFF1A1408), "BOOST", "TREBLE"),
    PedalSpec(10, "TREM", "TREMOLO", Color(0xFF3E9A45), Color(0xFF25692B), Color(0xFFF7F7F7), "DEPTH", "SPEED"),
    PedalSpec(11, "VIB", "VIBRATO", Color(0xFF2F8FC9), Color(0xFF1B5F8C), Color(0xFFF7F7F7), "DEPTH", "SPEED"),
    PedalSpec(12, "FLG", "FLANGER", Color(0xFF8A5AD6), Color(0xFF5B3699), Color(0xFFF7F7F7), "FEEDBK", "SPEED"),
    PedalSpec(13, "ROTO", "ROTARY", Color(0xFF8C5A32), Color(0xFF5E3A1D), Color(0xFFF7F7F7), "DEPTH", "SPEED"),
    PedalSpec(14, "WAH", "WAH", Color(0xFF33363D), Color(0xFF1C1E22), Color(0xFFF7F7F7), "DEPTH", "SPEED"),
    PedalSpec(15, "AWAH", "AUTO-WAH", Color(0xFFE3C22B), Color(0xFFB09210), Color(0xFF1A1408), "SENS", "PEAK"),
    PedalSpec(16, "OCT", "OCTAVER", Color(0xFF6B4A2E), Color(0xFF45301D), Color(0xFFF7F7F7), "SUB", "TONE"),
    PedalSpec(17, "RING", "RING MOD", Color(0xFF7D8794), Color(0xFF525B66), Color(0xFFF7F7F7), "MIX", "PITCH"),
    PedalSpec(18, "DLY", "DELAY", Color(0xFFDCE6F2), Color(0xFFA9B8CC), Color(0xFF1A1408), "LEVEL", "TIME"),
    PedalSpec(19, "TAPE", "TAPE ECHO", Color(0xFF2E5E46), Color(0xFF1B3B2B), Color(0xFFF7F7F7), "LEVEL", "TIME"),
    PedalSpec(20, "VERB", "REVERB", Color(0xFF5E7FA3), Color(0xFF3B5575), Color(0xFFF7F7F7), "LEVEL", "SIZE"),
    PedalSpec(21, "GATE", "GATE", Color(0xFF26282D), Color(0xFF121316), Color(0xFFF7F7F7), "THRESH", "DECAY")
)

fun pedalSpec(typeId: Int): PedalSpec? = PEDAL_SPECS.firstOrNull { it.typeId == typeId } ?: MASTER_PEDAL_SPECS.firstOrNull { it.typeId == typeId }

private fun swapFxSlots(track: Int, a: Int, b: Int) {
    val da = Native.fxSlot(track, a)
    val db = Native.fxSlot(track, b)
    Native.setFxSlot(track, a, db.getOrElse(0) { 0f }.roundToInt(), db.getOrElse(1) { 0.5f }, db.getOrElse(2) { 0.4f }, db.getOrElse(3) { 0f } > 0.5f)
    Native.setFxSlot(track, b, da.getOrElse(0) { 0f }.roundToInt(), da.getOrElse(1) { 0.5f }, da.getOrElse(2) { 0.4f }, da.getOrElse(3) { 0f } > 0.5f)
}

/**
 * Guitar Pedalboard FX Chain:
 * IN jack -> 3 Stompbox Slots linked by angled 1/4" patch cables -> OUT jack.
 * Each stompbox supports reorder (<- / ->), bypass footswitch, skin-aware slider/knob controls,
 * and Song Automation picking (`s.pickAuto`).
 */
@Composable
fun PedalboardPanel(s: AppState, track: Int) {
    val skin = LocalSkin.current
    var pickerSlot by remember(track) { mutableIntStateOf(-1) }

    val slotData = (0 until 3).map { slot ->
        remember(s.rev, track, slot) { Native.fxSlot(track, slot) }
    }
    val activeCount = slotData.count { it.getOrElse(0) { 0f }.roundToInt() > 0 && it.getOrElse(3) { 0f } <= 0.5f }

    ModulePanel(
        title = "Pedalboard: ${TRACK_NAMES[track]} ($activeCount active)",
        accent = skin.secondary,
        trailing = {
            SilkLabel("IN -> FX1 -> FX2 -> FX3 -> OUT", color = skin.muted, size = 8, mono = true)
        }
    ) {
        // Textured velcro pedalboard surface
        val boardShape = RoundedCornerShape(10.dp)
        Box(
            Modifier
                .fillMaxWidth()
                .clip(boardShape)
                .background(
                    Brush.verticalGradient(listOf(Color(0xFF16181E), Color(0xFF101217), Color(0xFF171920)))
                )
                .border(1.5.dp, Color(0xFF2F3440), boardShape)
                .padding(vertical = 10.dp, horizontal = 8.dp)
        ) {
            Row(
                Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                verticalAlignment = Alignment.CenterVertically
            ) {
                JackBadge("IN", skin.positive)
                PatchCable(active = slotData[0].getOrElse(0) { 0f } > 0.5f)

                for (slot in 0 until 3) {
                    val fd = slotData[slot]
                    val fxType = fd.getOrElse(0) { 0f }.roundToInt()
                    val fxAmt = fd.getOrElse(1) { 0.5f }
                    val fxRate = fd.getOrElse(2) { 0.4f }
                    val fxBypass = fd.getOrElse(3) { 0f } > 0.5f

                    if (fxType > 0) {
                        val spec = pedalSpec(fxType) ?: PEDAL_SPECS[0]
                        StompboxCard(
                            s = s,
                            track = track,
                            slot = slot,
                            spec = spec,
                            amount = fxAmt,
                            rate = fxRate,
                            bypass = fxBypass,
                            canMoveLeft = slot > 0,
                            canMoveRight = slot < 2,
                            onMoveLeft = { swapFxSlots(track, slot, slot - 1); s.rev++ },
                            onMoveRight = { swapFxSlots(track, slot, slot + 1); s.rev++ },
                            onSwapType = { pickerSlot = if (pickerSlot == slot) -1 else slot },
                            onRemove = {
                                Native.setFxSlot(track, slot, 0, fxAmt, fxRate, false)
                                if (pickerSlot == slot) pickerSlot = -1
                                s.rev++
                            },
                            onToggleBypass = {
                                Native.setFxSlot(track, slot, fxType, fxAmt, fxRate, !fxBypass)
                                s.rev++
                            }
                        )
                    } else {
                        EmptyPedalBay(
                            slot = slot,
                            selected = pickerSlot == slot,
                            onClick = { pickerSlot = if (pickerSlot == slot) -1 else slot }
                        )
                    }

                    val nextActive = if (slot < 2) slotData[slot + 1].getOrElse(0) { 0f } > 0.5f else fxType > 0
                    PatchCable(active = fxType > 0 || nextActive)
                }

                JackBadge("OUT", skin.primary)
            }
        }

        // Inline Stompbox Tray when adding or swapping a pedal
        if (pickerSlot in 0..2) {
            val fd = slotData[pickerSlot]
            val curType = fd.getOrElse(0) { 0f }.roundToInt()
            val curAmt = fd.getOrElse(1) { 0.55f }
            val curRate = fd.getOrElse(2) { 0.42f }
            Column(
                Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(skin.recess)
                    .border(1.dp, skin.secondary.copy(alpha = 0.6f), RoundedCornerShape(8.dp))
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    SilkLabel("CHOOSE PEDAL FOR SLOT ${pickerSlot + 1}", color = skin.silk, size = 10, modifier = Modifier.weight(1f))
                    HwButton("CLOSE", height = 24.dp, pad = 8.dp) { pickerSlot = -1 }
                }
                Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (spec in PEDAL_SPECS) {
                        val isCur = curType == spec.typeId
                        val chipShape = RoundedCornerShape(7.dp)
                        Column(
                            Modifier
                                .width(84.dp)
                                .clip(chipShape)
                                .background(Brush.verticalGradient(listOf(spec.bodyTop, spec.bodyBottom)))
                                .border(if (isCur) 2.dp else 1.dp, if (isCur) Color.White else Color.Black.copy(alpha = 0.4f), chipShape)
                                .clickable {
                                    Native.setFxSlot(track, pickerSlot, spec.typeId, curAmt, curRate, false)
                                    pickerSlot = -1
                                    s.rev++
                                }
                                .padding(horizontal = 6.dp, vertical = 7.dp),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.spacedBy(2.dp)
                        ) {
                            SilkLabel(spec.code, color = spec.ink, size = 11, mono = true)
                            SilkLabel(spec.name, color = spec.ink.copy(alpha = 0.85f), size = 8)
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun JackBadge(label: String, color: Color) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(3.dp),
        modifier = Modifier.width(30.dp)
    ) {
        Box(
            Modifier
                .size(18.dp)
                .clip(CircleShape)
                .background(Color(0xFF3E434E))
                .border(2.dp, Color(0xFF9EA6B5), CircleShape),
            contentAlignment = Alignment.Center
        ) {
            Box(Modifier.size(7.dp).clip(CircleShape).background(Color(0xFF0A0B0E)))
        }
        SilkLabel(label, color = color, size = 8, mono = true)
    }
}

@Composable
private fun PatchCable(active: Boolean) {
    Canvas(Modifier.width(22.dp).height(40.dp)) {
        val midY = size.height / 2f
        val cableColor = if (active) Color(0xFFD6DAE3) else Color(0xFF474C59)
        // Left & right angled 1/4" metal plugs
        drawRoundRect(
            color = Color(0xFF8C94A3),
            topLeft = Offset(0f, midY - 6.dp.toPx()),
            size = Size(5.dp.toPx(), 12.dp.toPx()),
            cornerRadius = CornerRadius(1.5.dp.toPx(), 1.5.dp.toPx())
        )
        drawRoundRect(
            color = Color(0xFF8C94A3),
            topLeft = Offset(size.width - 5.dp.toPx(), midY - 6.dp.toPx()),
            size = Size(5.dp.toPx(), 12.dp.toPx()),
            cornerRadius = CornerRadius(1.5.dp.toPx(), 1.5.dp.toPx())
        )
        // Patch cable cord
        drawLine(
            color = Color.Black.copy(alpha = 0.6f),
            start = Offset(4.dp.toPx(), midY + 1.5f),
            end = Offset(size.width - 4.dp.toPx(), midY + 1.5f),
            strokeWidth = 4.5.dp.toPx(),
            cap = StrokeCap.Round
        )
        drawLine(
            color = cableColor,
            start = Offset(4.dp.toPx(), midY),
            end = Offset(size.width - 4.dp.toPx(), midY),
            strokeWidth = 3.dp.toPx(),
            cap = StrokeCap.Round
        )
    }
}

@Composable
private fun StompboxCard(
    s: AppState,
    track: Int,
    slot: Int,
    spec: PedalSpec,
    amount: Float,
    rate: Float,
    bypass: Boolean,
    canMoveLeft: Boolean,
    canMoveRight: Boolean,
    onMoveLeft: () -> Unit,
    onMoveRight: () -> Unit,
    onSwapType: () -> Unit,
    onRemove: () -> Unit,
    onToggleBypass: () -> Unit
) {
    val engaged = !bypass
    val shape = RoundedCornerShape(10.dp)
    val dimAlpha = if (engaged) 1f else 0.58f
    val pedalW = 148.dp

    Column(
        Modifier
            .width(pedalW)
            .clip(shape)
            .background(
                Brush.verticalGradient(
                    listOf(spec.bodyTop.copy(alpha = dimAlpha), spec.bodyBottom.copy(alpha = dimAlpha))
                )
            )
            .border(1.5.dp, if (engaged) Color.White.copy(alpha = 0.35f) else Color(0xFF2B2E36), shape)
            .padding(7.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        // Top hardware utility strip: reorder + LED jewel + swap + remove
        Row(
            Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(3.dp), verticalAlignment = Alignment.CenterVertically) {
                MiniPedalChip("<", enabled = canMoveLeft, onClick = onMoveLeft)
                MiniPedalChip(">", enabled = canMoveRight, onClick = onMoveRight)
            }
            // Glowing status LED jewel
            Box(
                Modifier
                    .size(10.dp)
                    .clip(CircleShape)
                    .background(if (engaged) Color(0xFFFF3830) else Color(0xFF2E1414))
                    .border(1.5.dp, Color(0xFF222222), CircleShape)
            )
            Row(horizontalArrangement = Arrangement.spacedBy(3.dp), verticalAlignment = Alignment.CenterVertically) {
                MiniPedalChip("SWP", enabled = true, onClick = onSwapType)
                MiniPedalChip("X", enabled = true, onClick = onRemove)
            }
        }

        // Pedal badge & model name
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            SilkLabel(spec.code, color = spec.ink, size = 12, mono = true)
            SilkLabel(spec.name, color = spec.ink.copy(alpha = 0.85f), size = 9)
        }

        // Skin-aware controls (Left-Right Sliders OR Rotary Knobs + Song Automation Pick support!)
        Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
            RotaryKnob(
                s = s,
                label = spec.param1,
                id = P.FX_SLOT1_AMOUNT + slot * 2,
                index = track,
                min = 0f,
                max = 1f,
                modifier = Modifier.fillMaxWidth(),
                capColor = Color(0xFF1D2028)
            ) { pct(it) }

            if (spec.param2.isNotEmpty()) {
                RotaryKnob(
                    s = s,
                    label = spec.param2,
                    id = P.FX_SLOT1_AMOUNT + slot * 2 + 1,
                    index = track,
                    min = 0f,
                    max = 1f,
                    modifier = Modifier.fillMaxWidth(),
                    capColor = Color(0xFF1D2028)
                ) { pct(it) }
            }
        }

        // Metallic Stomp Footswitch Plate
        val stompShape = RoundedCornerShape(7.dp)
        Row(
            Modifier
                .fillMaxWidth()
                .height(34.dp)
                .clip(stompShape)
                .background(Color(0xFF16181D))
                .border(1.dp, if (engaged) Color.White.copy(alpha = 0.45f) else Color(0xFF3E424D), stompShape)
                .clickable(onClick = onToggleBypass)
                .padding(horizontal = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            // Chrome footswitch stud
            Box(
                Modifier
                    .size(16.dp)
                    .clip(CircleShape)
                    .background(Brush.radialGradient(listOf(Color(0xFFF2F5FA), Color(0xFF8B93A3))))
                    .border(1.5.dp, Color(0xFF474D59), CircleShape)
            )
            SilkLabel(
                text = if (engaged) "ENGAGED" else "BYPASS",
                color = if (engaged) HwGreen else Color(0xFF8E939E),
                size = 9,
                mono = true
            )
        }
    }
}

@Composable
private fun MiniPedalChip(text: String, enabled: Boolean, onClick: () -> Unit) {
    val shape = RoundedCornerShape(4.dp)
    Box(
        Modifier
            .height(20.dp)
            .clip(shape)
            .background(Color.Black.copy(alpha = if (enabled) 0.42f else 0.16f))
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 5.dp),
        contentAlignment = Alignment.Center
    ) {
        SilkLabel(text, color = Color.White.copy(alpha = if (enabled) 0.92f else 0.35f), size = 8, mono = true)
    }
}

@Composable
private fun EmptyPedalBay(slot: Int, selected: Boolean, onClick: () -> Unit) {
    val skin = LocalSkin.current
    val shape = RoundedCornerShape(10.dp)
    Column(
        Modifier
            .width(106.dp)
            .height(148.dp)
            .clip(shape)
            .background(Color(0xFF1B1E26).copy(alpha = 0.65f))
            .border(1.5.dp, if (selected) skin.secondary else Color(0xFF363C4A), shape)
            .clickable(onClick = onClick)
            .padding(8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Box(
            Modifier
                .size(28.dp)
                .clip(CircleShape)
                .background(if (selected) skin.secondary else Color(0xFF282D38)),
            contentAlignment = Alignment.Center
        ) {
            SilkLabel("+", color = if (selected) skin.onAccentInk else Color.White, size = 16)
        }
        Spacer(Modifier.height(6.dp))
        SilkLabel("SLOT ${slot + 1}", color = Color(0xFF9AA1B2), size = 9, mono = true)
        SilkLabel("ADD PEDAL", color = if (selected) skin.secondary else Color(0xFFD5DAE6), size = 9)
    }
}
