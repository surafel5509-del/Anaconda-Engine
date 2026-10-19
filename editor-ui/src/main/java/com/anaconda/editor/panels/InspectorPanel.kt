// ============================================================================
//  InspectorPanel.kt — right dock: property editor for the selection.
//
//  Phase-1 editable surfaces:
//    * Name                → nativeSetEntityName
//    * Transform           → nativeSetTransform (degrees ⇄ radians at JNI)
//    * Sprite tint         → nativeSetSpriteColor (preset swatches)
//    * Rigidbody           → gravity on/off + gravity scale slider
//  Phase-3 stub: "Add Component" button (component set expands with Month 3).
// ============================================================================
package com.anaconda.editor.panels

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.material3.Switch
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
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.anaconda.editor.components.HintBody
import com.anaconda.editor.components.NumberField
import com.anaconda.editor.components.PanelSurface
import com.anaconda.editor.components.SectionHeader
import com.anaconda.editor.components.Vec3Field
import com.anaconda.engine.core.EntitySnapshot
import com.anaconda.engine.core.Vec3F
import com.anaconda.engine.core.Vec4F

@Composable
fun InspectorPanel(
    entity: EntitySnapshot?,
    onNameChange: (String) -> Unit,
    onTransformChange: (position: Vec3F, rotationDeg: Vec3F, scale: Vec3F) -> Unit,
    onSpriteColorChange: (Vec4F) -> Unit,
    onGravityScaleChange: (Float) -> Unit,
    onUseGravityChange: (Boolean) -> Unit,
    onSpriteLayerChange: (Int) -> Unit = {},
    onRequestAddComponent: () -> Unit,
    modifier: Modifier = Modifier,
) {
    PanelSurface(title = "Inspector", modifier = modifier) {
        if (entity == null) {
            HintBody("Nothing selected.\nPick an entity in the Scene Hierarchy\nor tap it in the viewport (Phase 3).")
            return@PanelSurface
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 12.dp, vertical = 4.dp),
        ) {
            // ---------------------------------------------------- identity ---
            NameEditor(initialName = entity.name, onCommit = onNameChange)

            SectionHeader("Transform")
            Vec3Field(
                label = "Position",
                value = entity.position,
                onChange = { pos ->
                    onTransformChange(pos, currentRotationDeg(entity), entity.scale)
                },
            )
            Vec3Field(
                label = "Rotation",
                value = entity.rotationDeg,
                isDegrees = true,
                onChange = { rotDeg ->
                    onTransformChange(entity.position, rotDeg, entity.scale)
                },
            )
            Vec3Field(
                label = "Scale",
                value = entity.scale,
                onChange = { scale ->
                    onTransformChange(entity.position, currentRotationDeg(entity), scale)
                },
            )

            // ------------------------------------------------------ sprite ---
            if (entity.hasSprite) {
                SectionHeader("Sprite Renderer")
                SpriteTintEditor(
                    color = entity.spriteColor,
                    onColorChange = onSpriteColorChange,
                )
                SpriteLayerEditor(
                    layer = entity.spriteLayer,
                    onLayerChange = onSpriteLayerChange,
                )
            }

            // ---------------------------------------------------- rigidbody ---
            if (entity.hasRigidBody) {
                SectionHeader("Rigid Body")
                RigidBodyEditor(
                    gravityScale = 1f,   // snapshot Phase 1 doesn't carry the
                                         // value back; Month 2 extends flags.
                    onGravityScaleChange = onGravityScaleChange,
                    onUseGravityChange = onUseGravityChange,
                )
            }

            if (entity.hasCollider) {
                SectionHeader("Box Collider")
                Text(
                    text = "Auto-sized from Transform (editing lands Month 3)",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }

            // ------------------------------------------------- add component --
            SectionHeader("Components")
            OutlinedButton(
                onClick = onRequestAddComponent,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Icon(
                    imageVector = Icons.Filled.Add,
                    contentDescription = null,
                    modifier = Modifier.size(16.dp),
                )
                Text(
                    text = "  Add Component (Phase 3)",
                    style = MaterialTheme.typography.labelMedium,
                )
            }

            Text(
                text = "Entity #${entity.id.toUInt().toString(16)}",
                style = MaterialTheme.typography.labelSmall,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.6f),
                modifier = Modifier.padding(top = 12.dp, bottom = 16.dp),
            )
        }
    }
}

/** Snapshot rotations are radians; Inspector works in degrees. */
private fun currentRotationDeg(entity: EntitySnapshot): Vec3F = entity.rotationDeg

// ---------------------------------------------------------------------------
@Composable
private fun NameEditor(initialName: String, onCommit: (String) -> Unit) {
    var text by remember(initialName) { mutableStateOf(initialName) }
    OutlinedTextField(
        value = text,
        onValueChange = { input ->
            text = input
            onCommit(input)   // live rename (native sanitizes delimiters)
        },
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 4.dp),
        singleLine = true,
        label = { Text("Name") },
        textStyle = TextStyle(
            fontSize = 14.sp,
            fontWeight = FontWeight.Medium,
            color = MaterialTheme.colorScheme.onSurface,
        ),
        colors = TextFieldDefaults.colors(
            focusedContainerColor = MaterialTheme.colorScheme.surfaceVariant,
            unfocusedContainerColor = MaterialTheme.colorScheme.surfaceVariant,
        ),
    )
}

// ---------------------------------------------------------------------------
@Composable
private fun SpriteTintEditor(
    color: Vec4F,
    onColorChange: (Vec4F) -> Unit,
) {
    val current = Color(color.x, color.y, color.z, color.w)
    val presets = listOf(
        "White" to Color.White,
        "Red" to Color(0xFFE5544B),
        "Green" to Color(0xFF5FCF6B),
        "Blue" to Color(0xFF5B8CFF),
        "Yellow" to Color(0xFFF5D941),
        "Purple" to Color(0xFFB26BE8),
    )

    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Text(
            text = "Tint",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            presets.forEach { (name, preset) ->
                val selected =
                    preset.red == current.red && preset.green == current.green &&
                        preset.blue == current.blue
                Box(
                    modifier = Modifier
                        .size(26.dp)
                        .clip(CircleShape)
                        .background(preset)
                        .border(
                            width = if (selected) 2.dp else 0.dp,
                            color = MaterialTheme.colorScheme.onSurface,
                            shape = CircleShape,
                        )
                        .clickable {
                            onColorChange(Vec4F(preset.red, preset.green, preset.blue, 1f))
                        }
                        .semantics { contentDescription = name },
                )
            }
        }
    }
}

// ---------------------------------------------------------------------------
@Composable
private fun RigidBodyEditor(
    gravityScale: Float,
    onGravityScaleChange: (Float) -> Unit,
    onUseGravityChange: (Boolean) -> Unit,
) {
    var useGravity by remember { mutableStateOf(true) }
    // Local mirror so the slider tracks the user's drag; external value
    // (param) resets it when the selection changes.
    var localGravityScale by remember(gravityScale) { mutableStateOf(gravityScale) }

    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
    ) {
        Text(
            text = "Use Gravity",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurface,
        )
        Switch(
            checked = useGravity,
            onCheckedChange = { checked ->
                useGravity = checked
                onUseGravityChange(checked)
            },
        )
    }

    Text(
        text = "Gravity Scale",
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Slider(
            value = localGravityScale,
            onValueChange = {
                localGravityScale = it
                onGravityScaleChange(it)
            },
            valueRange = -2f..3f,
            steps = 9,
            modifier = Modifier.weight(1f),
        )
        NumberField(
            value = localGravityScale,
            onCommit = {
                localGravityScale = it
                onGravityScaleChange(it)
            },
            modifier = Modifier.size(width = 72.dp, height = 48.dp),
            label = "G",
        )
    }
}

@Composable
private fun SpriteLayerEditor(
    layer: Int,
    onLayerChange: (Int) -> Unit,
) {
    var localLayer by remember(layer) { mutableStateOf(layer) }
    LaunchedEffect(layer) { if (localLayer != layer) localLayer = layer }

    Text(
        text = "Sorting Order (lower = behind)",
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        modifier = Modifier.padding(top = 8.dp),
    )
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Slider(
            value = localLayer.toFloat(),
            onValueChange = {
                localLayer = it.toInt()
                onLayerChange(localLayer)
            },
            valueRange = -10f..10f,
            steps = 19,
            modifier = Modifier.weight(1f),
        )
        OutlinedTextField(
            value = localLayer.toString(),
            onValueChange = { input ->
                input.toIntOrNull()?.let {
                    localLayer = it.coerceIn(-100, 100)
                    onLayerChange(localLayer)
                }
            },
            modifier = Modifier.width(72.dp),
            singleLine = true,
            label = { Text("Layer", style = MaterialTheme.typography.labelSmall) },
            textStyle = TextStyle(
                fontSize = 13.sp,
                fontFamily = FontFamily.Monospace,
                color = MaterialTheme.colorScheme.onSurface,
            ),
        )
    }
}
