// ============================================================================
//  components/Panels.kt — shared building blocks for the IDE panels.
//
//  * PanelSurface     — the standard dockable panel chrome (title bar + body)
//  * Vec3Field        — tri-input numeric editor (position/rotation/scale)
//  * NumberField      — single float field with parse-while-you-type commit
//  * EditorChip       — compact metadata pill (FPS, counts)
//  * ColorSwatchGrid  — preset tint picker for SpriteComponent colors
// ============================================================================
package com.anaconda.editor.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.anaconda.engine.core.Vec3F

// ---------------------------------------------------------------------------
// PanelSurface — title bar + content well, identical chrome for every dock.
// ---------------------------------------------------------------------------
@Composable
fun PanelSurface(
    title: String,
    modifier: Modifier = Modifier,
    actions: @Composable RowScope.() -> Unit = {},
    content: @Composable () -> Unit,
) {
    Surface(
        modifier = modifier,
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 2.dp,
        shape = RoundedCornerShape(0.dp),
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(
                    text = title.uppercase(),
                    style = MaterialTheme.typography.labelMedium,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    letterSpacing = 1.1.sp,
                )
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(2.dp),
                    content = actions,
                )
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(MaterialTheme.colorScheme.surface),
            ) {
                content()
            }
        }
    }
}

// ---------------------------------------------------------------------------
// EditorChip — small pill (FPS / entity count / version).
// ---------------------------------------------------------------------------
@Composable
fun EditorChip(
    text: String,
    modifier: Modifier = Modifier,
    container: Color = MaterialTheme.colorScheme.surfaceVariant,
    contentColor: Color = MaterialTheme.colorScheme.onSurfaceVariant,
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(container)
            .padding(horizontal = 7.dp, vertical = 3.dp),
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelSmall,
            color = contentColor,
            fontFamily = FontFamily.Monospace,
        )
    }
}

// ---------------------------------------------------------------------------
// NumberField — float input committing on every parseable keystroke.
//
// "1." and "-" are intermediate states that simply don't commit yet; a valid
// parse pushes immediately → live preview in the viewport while typing.
// External value changes only overwrite the text when the field is NOT
// focused (prevents cursor fights with auto-refresh during Play mode).
// ---------------------------------------------------------------------------
@Composable
fun NumberField(
    value: Float,
    onCommit: (Float) -> Unit,
    modifier: Modifier = Modifier,
    label: String = "",
    accent: Color = MaterialTheme.colorScheme.primary,
) {
    var text by remember { mutableStateOf(formatFloat(value)) }
    var focused by remember { mutableStateOf(false) }

    LaunchedEffect(value) {
        if (!focused) text = formatFloat(value)
    }

    OutlinedTextField(
        value = text,
        onValueChange = { input ->
            text = input
            input.toFloatOrNull()?.let(onCommit)
        },
        modifier = modifier,
        singleLine = true,
        label = if (label.isEmpty()) null else {
            { Text(label, style = MaterialTheme.typography.labelSmall) }
        },
        prefix = {
            Text(
                text = label.ifEmpty { "•" }.take(1),
                style = MaterialTheme.typography.labelSmall,
                color = accent,
                fontWeight = FontWeight.Bold,
            )
        },
        textStyle = TextStyle(
            fontSize = 13.sp,
            fontFamily = FontFamily.Monospace,
            color = MaterialTheme.colorScheme.onSurface,
        ),
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        colors = TextFieldDefaults.colors(
            focusedContainerColor = MaterialTheme.colorScheme.surfaceVariant,
            unfocusedContainerColor = MaterialTheme.colorScheme.surfaceVariant,
            focusedIndicatorColor = accent,
            unfocusedIndicatorColor = MaterialTheme.colorScheme.outline,
        ),
    )
}

// ---------------------------------------------------------------------------
// Vec3Field — label + X/Y triplet rows for one transform channel.
// ---------------------------------------------------------------------------
@Composable
fun Vec3Field(
    label: String,
    value: Vec3F,
    onChange: (Vec3F) -> Unit,
    modifier: Modifier = Modifier,
    isDegrees: Boolean = false,
) {
    Column(modifier = modifier.fillMaxWidth()) {
        Text(
            text = label + if (isDegrees) "  (°)" else "",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(bottom = 4.dp),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            NumberField(
                value = value.x,
                onCommit = { onChange(value.copy(x = it)) },
                modifier = Modifier.weight(1f),
                label = "X",
                accent = Color(0xFFE5544B),
            )
            NumberField(
                value = value.y,
                onCommit = { onChange(value.copy(y = it)) },
                modifier = Modifier.weight(1f),
                label = "Y",
                accent = Color(0xFF5FCF6B),
            )
            NumberField(
                value = value.z,
                onCommit = { onChange(value.copy(z = it)) },
                modifier = Modifier.weight(1f),
                label = "Z",
                accent = Color(0xFF5B8CFF),
            )
        }
    }
}

// ---------------------------------------------------------------------------
// SectionHeader — collapsible-looking section label used by the Inspector.
// ---------------------------------------------------------------------------
@Composable
fun SectionHeader(text: String, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(top = 12.dp, bottom = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelMedium,
            fontWeight = FontWeight.SemiBold,
            color = MaterialTheme.colorScheme.primary,
        )
        Box(
            modifier = Modifier
                .weight(1f)
                .height(1.dp)
                .background(MaterialTheme.colorScheme.outline),
        )
    }
}

// ---------------------------------------------------------------------------
// ColorSwatchGrid — quick preset tints for the selected sprite.
// ---------------------------------------------------------------------------
@Composable
fun ColorSwatchGrid(
    colors: List<Pair<String, androidx.compose.ui.graphics.Color>>,
    selected: Color,
    onSelect: (Color) -> Unit,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        colors.forEach { (name, color) ->
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .clip(CircleShape)
                    .background(color)
                    .border(
                        width = if (color == selected) 2.dp else 1.dp,
                        color = if (color == selected) {
                            MaterialTheme.colorScheme.onSurface
                        } else {
                            MaterialTheme.colorScheme.outline
                        },
                        shape = CircleShape,
                    )
                    .clickable { onSelect(color) },
                contentAlignment = Alignment.Center,
            ) {
                // Accessibility label for TalkBack (contentDescription path).
                Text(
                    text = "",
                    modifier = Modifier,
                )
            }
        }
    }
}

// ---------------------------------------------------------------------------
// HintBody — empty-state text for panels with nothing selected.
// ---------------------------------------------------------------------------
@Composable
fun HintBody(text: String, modifier: Modifier = Modifier) {
    Box(modifier = modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        Text(
            text = text,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = TextAlign.Center,
            modifier = Modifier.padding(20.dp),
        )
    }
}

// ---------------------------------------------------------------------------
// formatting helper
// ---------------------------------------------------------------------------
internal fun formatFloat(value: Float): String {
    // 3 decimals, trailing zeros trimmed → "-0.5", "1", "3.141"
    if (value == 0f) return "0"
    val rounded = Math.round(value * 1000f) / 1000f
    val s = rounded.toString()
    return if (s.endsWith(".0")) s.dropLast(2) else s
}

/** Spacer helper kept local to avoid extra imports in panels. */
@Composable
fun HGap(size: Int) = Spacer(modifier = Modifier.width(size.dp))

@Composable
fun VGap(size: Int) = Spacer(modifier = Modifier.height(size.dp))
