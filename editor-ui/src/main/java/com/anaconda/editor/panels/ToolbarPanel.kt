// ============================================================================
//  ToolbarPanel.kt — top command bar: identity, run controls, stats, export.
// ============================================================================
package com.anaconda.editor.panels

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Android
import androidx.compose.material.icons.filled.FileDownload
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.anaconda.editor.RunState
import com.anaconda.editor.components.EditorChip

@Composable
fun ToolbarPanel(
    sceneName: String,
    runState: RunState,
    fps: Float,
    entityCount: Int,
    engineVersion: String,
    onPlay: () -> Unit,
    onPause: () -> Unit,
    onStop: () -> Unit,
    onRefresh: () -> Unit,
    onCreateEntity: () -> Unit,
    onExport: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 4.dp,
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            // ---- brand ----------------------------------------------------
            Icon(
                imageVector = Icons.Filled.Android,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(22.dp),
            )
            Text(
                text = "Anaconda",
                fontWeight = FontWeight.Bold,
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Text(
                text = "Engine",
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(end = 6.dp),
            )

            EditorChip("v$engineVersion")

            Box(modifier = Modifier.size(6.dp))

            Text(
                text = sceneName,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            androidx.compose.foundation.layout.Spacer(
                modifier = Modifier.weight(1f),
            )

            // ---- stats ------------------------------------------------------
            EditorChip("entities $entityCount")
            EditorChip(
                text = if (fps > 0f) "FPS ${fps.toInt()}" else "FPS —",
                container = when {
                    fps >= 55f -> MaterialTheme.colorScheme.primaryContainer
                    fps >= 30f -> MaterialTheme.colorScheme.surfaceVariant
                    else -> MaterialTheme.colorScheme.errorContainer
                },
            )

            Box(modifier = Modifier.size(6.dp))

            // ---- run controls ----------------------------------------------
            RunButton(
                icon = Icons.Filled.PlayArrow,
                label = "Play",
                tint = MaterialTheme.colorScheme.primary,
                enabled = runState == RunState.EDIT || runState == RunState.PAUSE,
                onClick = onPlay,
            )
            RunButton(
                icon = Icons.Filled.Pause,
                label = "Pause",
                tint = MaterialTheme.colorScheme.tertiary,
                enabled = runState == RunState.PLAY,
                onClick = onPause,
            )
            RunButton(
                icon = Icons.Filled.Stop,
                label = "Stop",
                tint = MaterialTheme.colorScheme.error,
                enabled = runState == RunState.PLAY || runState == RunState.PAUSE,
                onClick = onStop,
            )

            Box(modifier = Modifier.size(4.dp))

            IconButton(onClick = onRefresh) {
                Icon(
                    imageVector = Icons.Filled.Refresh,
                    contentDescription = "Refresh scene",
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            IconButton(onClick = onCreateEntity) {
                Icon(
                    imageVector = Icons.Filled.Add,
                    contentDescription = "Create entity",
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }

            // ---- export ------------------------------------------------------
            Surface(
                onClick = onExport,
                shape = RoundedCornerShape(8.dp),
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.padding(start = 4.dp),
            ) {
                Row(
                    modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    Icon(
                        imageVector = Icons.Filled.FileDownload,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.onPrimary,
                        modifier = Modifier.size(16.dp),
                    )
                    Text(
                        text = "Export",
                        style = MaterialTheme.typography.labelMedium,
                        fontWeight = FontWeight.SemiBold,
                        color = MaterialTheme.colorScheme.onPrimary,
                    )
                }
            }
        }
    }
}

@Composable
private fun RunButton(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    tint: Color,
    enabled: Boolean,
    onClick: () -> Unit,
) {
    val background = if (enabled) tint.copy(alpha = 0.14f) else Color.Transparent
    Surface(
        onClick = onClick,
        enabled = enabled,
        shape = RoundedCornerShape(8.dp),
        color = background,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 8.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Icon(
                imageVector = icon,
                contentDescription = label,
                tint = if (enabled) tint else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.4f),
                modifier = Modifier.size(18.dp),
            )
            Text(
                text = label,
                style = MaterialTheme.typography.labelMedium,
                color = if (enabled) tint else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.4f),
            )
        }
    }
}
