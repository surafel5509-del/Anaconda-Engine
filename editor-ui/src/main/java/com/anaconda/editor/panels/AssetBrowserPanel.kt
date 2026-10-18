// ============================================================================
//  AssetBrowserPanel.kt — bottom dock: project assets.
//
//  PHASE 1: seeded placeholder grid that exercises the exact UX (categories,
//  tiles, import affordance) the real pipeline will fill in Month 2:
//    SAF picker → project/assets copy → decoder → GL upload queue.
//  Every tile taps through to a "ships Phase 2" message so the flow reads as
//  intentionally staged, not broken.
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
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AudioFile
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.FileDownload
import androidx.compose.material.icons.filled.Image
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ScrollableTabRow
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.anaconda.editor.AssetEntry
import com.anaconda.editor.components.PanelSurface

private val kCategories = listOf("All", "Sprites", "Audio", "Scripts")

@Composable
fun AssetBrowserPanel(
    assets: List<AssetEntry>,
    onImport: () -> Unit,
    onAssetClick: (AssetEntry) -> Unit,
    modifier: Modifier = Modifier,
) {
    var categoryIndex by remember { mutableIntStateOf(0) }

    PanelSurface(
        title = "Assets",
        modifier = modifier,
        actions = {
            IconButton(onClick = onImport, modifier = Modifier.size(30.dp)) {
                Icon(
                    imageVector = Icons.Filled.FileDownload,
                    contentDescription = "Import asset",
                    tint = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.size(16.dp),
                )
            }
        },
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            ScrollableTabRow(
                selectedTabIndex = categoryIndex,
                containerColor = MaterialTheme.colorScheme.surface,
                contentColor = MaterialTheme.colorScheme.primary,
                edgePadding = 8.dp,
                divider = {},
                modifier = Modifier.fillMaxWidth(),
            ) {
                kCategories.forEachIndexed { index, title ->
                    Tab(
                        selected = categoryIndex == index,
                        onClick = { categoryIndex = index },
                        text = {
                            Text(
                                text = title,
                                style = MaterialTheme.typography.labelMedium,
                            )
                        },
                    )
                }
            }

            val filtered = when (kCategories[categoryIndex]) {
                "Sprites" -> assets.filter { it.kind == "Sprite" }
                "Audio" -> assets.filter { it.kind == "Audio" }
                "Scripts" -> assets.filter { it.kind == "Script" }
                else -> assets
            }

            if (filtered.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(12.dp),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        text = "No assets in this category yet — tap ⬇ to import.",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            } else {
                LazyRow(
                    contentPadding = androidx.compose.foundation.layout.PaddingValues(
                        horizontal = 10.dp,
                        vertical = 8.dp,
                    ),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    modifier = Modifier.fillMaxSize(),
                ) {
                    items(filtered, key = { it.name }) { asset ->
                        AssetTile(asset = asset, onClick = { onAssetClick(asset) })
                    }
                }
            }
        }
    }
}

@Composable
private fun AssetTile(asset: AssetEntry, onClick: () -> Unit) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier
            .width(78.dp)
            .clip(RoundedCornerShape(8.dp))
            .clickable(onClick = onClick)
            .padding(vertical = 4.dp),
    ) {
        Box(
            modifier = Modifier
                .size(56.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(
                    androidx.compose.ui.graphics.Color(
                        red = asset.color.x,
                        green = asset.color.y,
                        blue = asset.color.z,
                        alpha = 0.28f,
                    )
                ),
            contentAlignment = Alignment.Center,
        ) {
            Icon(
                imageVector = iconFor(asset.kind),
                contentDescription = null,
                tint = Color(asset.color.x, asset.color.y, asset.color.z, 1f),
                modifier = Modifier.size(26.dp),
            )
        }
        Text(
            text = asset.name,
            style = MaterialTheme.typography.labelSmall,
            fontSize = 10.sp,
            fontWeight = FontWeight.Medium,
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.padding(top = 4.dp),
        )
        Text(
            text = asset.kind,
            style = MaterialTheme.typography.labelSmall,
            fontSize = 9.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

private fun iconFor(kind: String): ImageVector = when (kind) {
    "Sprite" -> Icons.Filled.Image
    "Audio" -> Icons.Filled.AudioFile
    "Script" -> Icons.Filled.Code
    else -> Icons.Filled.Image
}
