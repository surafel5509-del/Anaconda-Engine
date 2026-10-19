// ============================================================================
//  EditorScreen.kt — root IDE layout.
//
//  RESPONSIVE STRATEGY
//  ───────────────────
//  ≥ 840 dp wide (tablets, landscape phones): classic three-column IDE
//      ┌─────────────────────────────────────────────────────────┐
//      │                       Toolbar                          │
//      ├──────────┬─────────────────────────────┬───────────────┤
//      │ Hierarchy│        Game Viewport        │   Inspector   │
//      │  240 dp  │                             │    300 dp     │
//      │          ├─────────────────────────────┤               │
//      │          │       Asset Browser         │               │
//      ├──────────┴─────────────────────────────┴───────────────┤
//      │  (snackbar)                                             │
//      └─────────────────────────────────────────────────────────┘
//  < 840 dp (portrait phones): viewport stays on top; the three docks become
//  tabs in a switchable bottom sheet area so nothing overflows.
// ============================================================================
package com.anaconda.editor

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.anaconda.engine.core.EntitySnapshot
import com.anaconda.editor.panels.AssetBrowserPanel
import com.anaconda.editor.panels.InspectorPanel
import com.anaconda.editor.panels.SceneHierarchyPanel
import com.anaconda.editor.panels.ToolbarPanel
import com.anaconda.editor.theme.EditorTheme
import com.anaconda.editor.viewport.GameViewport

@Composable
fun EditorScreen(
    viewModel: EditorViewModel = viewModel(),
) {
    EditorTheme {
        val entities by viewModel.entities.collectAsState()
        val selectedId by viewModel.selectedId.collectAsState()
        val runState by viewModel.runState.collectAsState()
        val fps by viewModel.fps.collectAsState()
        val sceneLabel by viewModel.sceneLabel.collectAsState()
        val dirty by viewModel.dirty.collectAsState()
        val canUndo by viewModel.canUndo.collectAsState()
        val canRedo by viewModel.canRedo.collectAsState()

        val snackbarHostState = remember { SnackbarHostState() }

        // SAF image picker (Month 2) — OpenDocument needs no runtime permission
        // and lets the system picker surface cloud/storage providers for free.
        var pendingImport by remember { mutableStateOf(false) }
        val importLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument(),
        ) { uri ->
            if (uri != null) viewModel.importSprite(uri)
        }
        LaunchedEffect(pendingImport) {
            if (pendingImport) {
                pendingImport = false
                importLauncher.launch(arrayOf("image/png", "image/jpeg"))
            }
        }

        // One-shot messages from the ViewModel → snackbars.
        LaunchedEffect(Unit) {
            viewModel.messages.collect { message ->
                snackbarHostState.showSnackbar(message)
            }
        }

        val drawCalls by viewModel.drawCalls.collectAsState()
        val spriteCount by viewModel.spriteCount.collectAsState()
        val selectedEntity = entities.firstOrNull { it.id == selectedId }

        Scaffold(
            modifier = Modifier.fillMaxSize(),
            containerColor = MaterialTheme.colorScheme.background,
            snackbarHost = {
                SnackbarHost(
                    hostState = snackbarHostState,
                    modifier = Modifier.padding(bottom = 8.dp),
                )
            },
        ) { paddingValues ->
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(paddingValues),
            ) {
                ToolbarPanel(
                    sceneName = sceneLabel + if (dirty) "  \u2022" else "",
                    runState = runState,
                    fps = fps,
                    entityCount = entities.size,
                    engineVersion = viewModel.engineVersion,
                    onPlay = { viewModel.setRunState(RunState.PLAY) },
                    onPause = { viewModel.setRunState(RunState.PAUSE) },
                    onStop = { viewModel.setRunState(RunState.EDIT) },
                    onRefresh = viewModel::refresh,
                    onCreateEntity = viewModel::createEntity,
                    onExport = viewModel::requestExport,
                    onNew = viewModel::newScene,
                    onSave = { viewModel.saveScene() },
                    onLoad = viewModel::loadScene,
                    onUndo = viewModel::undo,
                    onRedo = viewModel::redo,
                    canUndo = canUndo,
                    canRedo = canRedo,
                    modifier = Modifier.fillMaxWidth(),
                )

                BoxWithConstraints(
                    modifier = Modifier
                        .fillMaxSize()
                        .weight(1f),
                ) {
                    if (maxWidth >= 840.dp) {
                        WideLayout(
                            viewModel = viewModel,
                            entities = entities,
                            selectedId = selectedId,
                            selectedEntity = selectedEntity,
                            onImport = { pendingImport = true },
                        )
                    } else {
                        CompactLayout(
                            viewModel = viewModel,
                            entities = entities,
                            selectedId = selectedId,
                            selectedEntity = selectedEntity,
                            onImport = { pendingImport = true },
                        )
                    }
                }
                // Profiling HUD (Month 3) — lightweight overlay below viewport area
                ProfilingHud(
                    fps = fps,
                    drawCalls = drawCalls,
                    spriteCount = spriteCount,
                    entityCount = entities.size,
                )
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Wide (three-dock) layout
// ---------------------------------------------------------------------------
@Composable
private fun WideLayout(
    viewModel: EditorViewModel,
    entities: List<EntitySnapshot>,
    selectedId: Int?,
    selectedEntity: EntitySnapshot?,
    onImport: () -> Unit,
) {
    Row(modifier = Modifier.fillMaxSize()) {
        SceneHierarchyPanel(
            entities = entities,
            selectedId = selectedId,
            onSelect = viewModel::select,
            onCreate = viewModel::createEntity,
            onDelete = viewModel::deleteSelected,
            onDuplicate = viewModel::duplicateSelected,
            onRefresh = viewModel::refresh,
            modifier = Modifier
                .width(240.dp)
                .fillMaxHeight(),
        )

        Column(
            modifier = Modifier
                .weight(1f)
                .fillMaxHeight(),
        ) {
            GameViewport(
                onSurfaceReady = viewModel::refresh,
                onPick = { x, y -> viewModel.pickEntityAt(x, y) },
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f),
            )
            AssetBrowserPanel(
                assets = viewModel.projectAssets.collectAsState().value,
                onImport = onImport,
                onAssetClick = viewModel::assignAssetToSelected,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(172.dp),
            )
        }

        InspectorPanel(
            entity = selectedEntity,
            onNameChange = viewModel::renameSelected,
            onTransformChange = { pos, rotDeg, scale ->
                viewModel.updateSelectedTransform(pos, rotDeg, scale)
            },
            onSpriteColorChange = viewModel::updateSelectedSpriteColor,
            onGravityScaleChange = viewModel::updateSelectedGravityScale,
            onUseGravityChange = viewModel::updateSelectedUseGravity,
            onSpriteLayerChange = viewModel::updateSelectedSpriteLayer,
            onRequestAddComponent = {
                viewModel.emitMessage("Component library expands in Phase 3 (Month 3 roadmap)")
            },
            modifier = Modifier
                .width(304.dp)
                .fillMaxHeight(),
        )
    }
}

@Composable
private fun ProfilingHud(
    fps: Float,
    drawCalls: Int,
    spriteCount: Int,
    entityCount: Int,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.9f))
            .padding(horizontal = 8.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = "FPS ${if (fps > 0f) fps.toInt().toString() else "—"}  •  Draw $drawCalls  •  Sprites $spriteCount  •  Entities $entityCount",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

// ---------------------------------------------------------------------------
// Compact (phone portrait) layout — viewport + dockable bottom tabs
// ---------------------------------------------------------------------------
@Composable
private fun CompactLayout(
    viewModel: EditorViewModel,
    entities: List<EntitySnapshot>,
    selectedId: Int?,
    selectedEntity: EntitySnapshot?,
    onImport: () -> Unit,
) {
    var dockIndex by rememberSaveable { mutableIntStateOf(0) }
    val dockTitles = listOf("Scene", "Inspector", "Assets")

    Column(modifier = Modifier.fillMaxSize()) {
        GameViewport(
            onSurfaceReady = viewModel::refresh,
            onPick = { x, y -> viewModel.pickEntityAt(x, y) },
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f),
        )

        TabRow(
            selectedTabIndex = dockIndex,
            containerColor = MaterialTheme.colorScheme.surface,
            contentColor = MaterialTheme.colorScheme.primary,
            modifier = Modifier.fillMaxWidth(),
        ) {
            dockTitles.forEachIndexed { index, title ->
                Tab(
                    selected = dockIndex == index,
                    onClick = { dockIndex = index },
                    text = { Text(title, style = MaterialTheme.typography.labelMedium) },
                )
            }
        }

        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(248.dp),
        ) {
            when (dockIndex) {
                0 -> SceneHierarchyPanel(
                    entities = entities,
                    selectedId = selectedId,
                    onSelect = viewModel::select,
                    onCreate = viewModel::createEntity,
                    onDelete = viewModel::deleteSelected,
                    onDuplicate = viewModel::duplicateSelected,
                    onRefresh = viewModel::refresh,
                    modifier = Modifier.fillMaxSize(),
                )

                1 -> InspectorPanel(
                    entity = selectedEntity,
                    onNameChange = viewModel::renameSelected,
                    onTransformChange = { pos, rotDeg, scale ->
                        viewModel.updateSelectedTransform(pos, rotDeg, scale)
                    },
                    onSpriteColorChange = viewModel::updateSelectedSpriteColor,
                    onGravityScaleChange = viewModel::updateSelectedGravityScale,
                    onUseGravityChange = viewModel::updateSelectedUseGravity,
                    onSpriteLayerChange = viewModel::updateSelectedSpriteLayer,
                    onRequestAddComponent = {
                        viewModel.emitMessage("Component library expands in Phase 3")
                    },
                    modifier = Modifier.fillMaxSize(),
                )

                2 -> AssetBrowserPanel(
                    assets = viewModel.projectAssets.collectAsState().value,
                    onImport = onImport,
                    onAssetClick = viewModel::assignAssetToSelected,
                    modifier = Modifier.fillMaxSize(),
                )
            }
        }
    }
}
