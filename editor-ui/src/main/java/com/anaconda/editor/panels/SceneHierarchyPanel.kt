// ============================================================================
//  SceneHierarchyPanel.kt — left dock: every entity in the scene.
//
//  Phase 1: flat list with select / create / delete / rename-by-inspector.
//  Month 2 adds reparenting trees + multi-select; the row layout below keeps
//  a leading indent slot so nesting lands without a redesign.
// ============================================================================
package com.anaconda.editor.panels

import androidx.compose.foundation.background
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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.anaconda.editor.components.HintBody
import com.anaconda.editor.components.PanelSurface
import com.anaconda.engine.core.EntitySnapshot

@Composable
fun SceneHierarchyPanel(
    entities: List<EntitySnapshot>,
    selectedId: Int?,
    onSelect: (Int) -> Unit,
    onCreate: () -> Unit,
    onDelete: () -> Unit,
    onRefresh: () -> Unit,
    modifier: Modifier = Modifier,
) {
    PanelSurface(
        title = "Scene Hierarchy",
        modifier = modifier,
        actions = {
            IconButton(onClick = onRefresh, modifier = Modifier.size(30.dp)) {
                Icon(
                    imageVector = Icons.Filled.Refresh,
                    contentDescription = "Refresh hierarchy",
                    tint = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.size(16.dp),
                )
            }
            IconButton(onClick = onCreate, modifier = Modifier.size(30.dp)) {
                Icon(
                    imageVector = Icons.Filled.Add,
                    contentDescription = "Create entity",
                    tint = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.size(18.dp),
                )
            }
            if (selectedId != null) {
                IconButton(onClick = onDelete, modifier = Modifier.size(30.dp)) {
                    Icon(
                        imageVector = Icons.Filled.Delete,
                        contentDescription = "Delete selected entity",
                        tint = MaterialTheme.colorScheme.error,
                        modifier = Modifier.size(16.dp),
                    )
                }
            }
        },
    ) {
        if (entities.isEmpty()) {
            HintBody("Scene is empty.\nTap + to create an entity.")
        } else {
            LazyColumn(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(vertical = 4.dp),
            ) {
                items(entities, key = { it.id }) { entity ->
                    HierarchyRow(
                        entity = entity,
                        selected = entity.id == selectedId,
                        onClick = { onSelect(entity.id) },
                    )
                }
            }
        }
    }
}

@Composable
private fun HierarchyRow(
    entity: EntitySnapshot,
    selected: Boolean,
    onClick: () -> Unit,
) {
    val rowBackground = if (selected) {
        MaterialTheme.colorScheme.secondaryContainer
    } else {
        Color.Transparent
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 6.dp, vertical = 1.dp)
            .clip(RoundedCornerShape(6.dp))
            .background(rowBackground)
            .clickable(onClick = onClick)
            .padding(horizontal = 8.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        // Indent slot for future parenting depth (Phase 3 tree rows).
        Box(modifier = Modifier.width(2.dp))

        // Sprite color dot (or neutral dot for non-rendering entities).
        Box(
            modifier = Modifier
                .size(9.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(
                    if (entity.hasSprite) {
                        Color(
                            red = entity.spriteColor.x,
                            green = entity.spriteColor.y,
                            blue = entity.spriteColor.z,
                        )
                    } else {
                        MaterialTheme.colorScheme.outline
                    },
                ),
        )

        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = entity.name,
                style = MaterialTheme.typography.bodyMedium,
                color = if (selected) {
                    MaterialTheme.colorScheme.onSecondaryContainer
                } else {
                    MaterialTheme.colorScheme.onSurface
                },
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = entity.componentSummary,
                style = MaterialTheme.typography.labelSmall,
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.8f),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }

        // Handles are opaque — display short hex for debugging parity with logs.
        Text(
            text = "#${entity.id.toUInt().toString(16).takeLast(4)}",
            style = MaterialTheme.typography.labelSmall,
            fontFamily = FontFamily.Monospace,
            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.6f),
        )
    }
}
