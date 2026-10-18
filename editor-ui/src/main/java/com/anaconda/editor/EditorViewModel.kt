// ============================================================================
//  EditorViewModel.kt — single source of truth for the editor UI.
//
//  State holders (StateFlow → Compose via collectAsState):
//    entities     every engine entity as an immutable snapshot row
//    selectedId   hierarchy/inspector selection
//    runState     Edit / Play / Pause (mirrors the native engine)
//    fps          smoothed native frame rate
//    messages     one-shot snackbars (export stub, import stub, errors)
//
//  Sync strategy (Phase 1 — see docs/ARCHITECTURE.md §5.4):
//    DOWN  (edits)  → immediate native call + local patch (zero latency)
//    UP    (pulled) → scene snapshot after lifecycle events / play-stop /
//                     manual refresh / while PLAY auto-refresh selection.
//  Phase 3 replaces polling with a native dirty-flag delta channel.
// ============================================================================
package com.anaconda.editor

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.anaconda.engine.core.EntitySnapshot
import com.anaconda.engine.core.MathConversions
import com.anaconda.engine.core.NativeEngine
import com.anaconda.engine.core.Vec3F
import com.anaconda.engine.core.Vec4F
import com.anaconda.engine.core.parseSceneSnapshot
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// ---------------------------------------------------------------------------
// Run modes (mirrors native RunState ordinals)
// ---------------------------------------------------------------------------
enum class RunState(val nativeId: Int) {
    EDIT(NativeEngine.RUN_EDIT),
    PLAY(NativeEngine.RUN_PLAY),
    PAUSE(NativeEngine.RUN_PAUSE),
}

// ---------------------------------------------------------------------------
// Seed content for the Phase-1 Asset Browser placeholder grid.
// (Phase 2 replaces this with the real project asset listing via SAF.)
// ---------------------------------------------------------------------------
data class AssetEntry(
    val name: String,
    val kind: String,
    val color: Vec4F,
    val isPlaceholder: Boolean = true,
)

class EditorViewModel : ViewModel() {

    // ---- state -------------------------------------------------------------
    private val _entities = MutableStateFlow<List<EntitySnapshot>>(emptyList())
    val entities: StateFlow<List<EntitySnapshot>> = _entities.asStateFlow()

    private val _selectedId = MutableStateFlow<Int?>(null)
    val selectedId: StateFlow<Int?> = _selectedId.asStateFlow()

    private val _runState = MutableStateFlow(RunState.EDIT)
    val runState: StateFlow<RunState> = _runState.asStateFlow()

    private val _fps = MutableStateFlow(0f)
    val fps: StateFlow<Float> = _fps.asStateFlow()

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 4)
    val messages: SharedFlow<String> = _messages.asSharedFlow()

    val engineVersion: String = NativeEngine.version()

    /** Placeholder assets shown until the Phase-2 import pipeline lands. */
    val seedAssets: List<AssetEntry> = listOf(
        AssetEntry("hero_idle.png", "Sprite", Vec4F(0.91f, 0.30f, 0.24f, 1f)),
        AssetEntry("tiles_ground.png", "Sprite", Vec4F(0.18f, 0.55f, 0.34f, 1f)),
        AssetEntry("coin_gold.png", "Sprite", Vec4F(0.96f, 0.85f, 0.25f, 1f)),
        AssetEntry("platform.png", "Sprite", Vec4F(0.22f, 0.44f, 0.86f, 1f)),
        AssetEntry("jump.ogg", "Audio", Vec4F(0.65f, 0.45f, 0.95f, 1f)),
        AssetEntry("music_theme.ogg", "Audio", Vec4F(0.40f, 0.55f, 0.90f, 1f)),
        AssetEntry("player.lua", "Script", Vec4F(0.85f, 0.65f, 0.20f, 1f)),
        AssetEntry("pickup.lua", "Script", Vec4F(0.85f, 0.65f, 0.20f, 1f)),
    )

    private var nameCounter = 0

    init {
        // Initial pull — the demo scene is built natively on first GL surface,
        // which happens shortly after launch (hence the delayed retry).
        refresh()
        viewModelScope.launch {
            delay(750)
            refresh()
        }

        // Toolbar FPS chip — cheap native atomic read.
        viewModelScope.launch {
            while (isActive) {
                _fps.value = NativeEngine.fps()
                delay(400)
            }
        }

        // While PLAY is running, keep the selected entity's transform live so
        // watching a body fall feels like a real editor. Skipped when the user
        // is focused in a field (Inspector guards with focus state).
        viewModelScope.launch {
            while (isActive) {
                delay(500)
                if (_runState.value == RunState.PLAY && _selectedId.value != null) {
                    refreshSelectedEntity()
                }
            }
        }
    }

    // ---- snapshot sync -----------------------------------------------------
    fun refresh() {
        runCatching {
            val parsed = parseSceneSnapshot(NativeEngine.sceneSnapshot())
            _entities.value = parsed
            // Drop selection for entities that no longer exist.
            val current = _selectedId.value
            if (current != null && parsed.none { it.id == current }) {
                _selectedId.value = null
            }
        }.onFailure {
            emitMessage("Engine snapshot failed: ${it.message ?: "unknown error"}")
        }
    }

    private fun refreshSelectedEntity() {
        val id = _selectedId.value ?: return
        runCatching {
            val parsed = parseSceneSnapshot(NativeEngine.sceneSnapshot())
            _entities.value = parsed
            if (parsed.none { it.id == id }) _selectedId.value = null
        }
    }

    val selectedEntity: EntitySnapshot?
        get() = _entities.value.firstOrNull { it.id == _selectedId.value }

    // ---- selection ---------------------------------------------------------
    fun select(entityId: Int?) {
        _selectedId.value = entityId
    }

    // ---- entity mutation (down-channel: immediate native + local patch) -----
    fun createEntity() {
        val name = "Entity ${++nameCounter}"
        val id = NativeEngine.createEntity(name)
        refresh()
        _selectedId.value = id
        emitMessage("Created $name")
    }

    fun deleteSelected() {
        val id = _selectedId.value ?: return
        NativeEngine.destroyEntity(id)
        _selectedId.value = null
        refresh()
    }

    fun renameSelected(newName: String) {
        val id = _selectedId.value ?: return
        if (newName.isBlank()) return
        NativeEngine.nativeSetEntityName(id, newName)
        patchSelected { it.copy(name = newName) }
    }

    /** Position/rotation(deg)/scale edit from the Inspector. */
    fun updateSelectedTransform(position: Vec3F, rotationDeg: Vec3F, scale: Vec3F) {
        val id = _selectedId.value ?: return
        NativeEngine.nativeSetTransform(
            id,
            position.x, position.y, position.z,
            MathConversions.degToRad(rotationDeg.x),
            MathConversions.degToRad(rotationDeg.y),
            MathConversions.degToRad(rotationDeg.z),
            scale.x, scale.y, scale.z,
        )
        patchSelected {
            it.copy(position = position, rotationRad = Vec3F(
                MathConversions.degToRad(rotationDeg.x),
                MathConversions.degToRad(rotationDeg.y),
                MathConversions.degToRad(rotationDeg.z),
            ), scale = scale)
        }
    }

    fun updateSelectedSpriteColor(color: Vec4F) {
        val id = _selectedId.value ?: return
        NativeEngine.nativeSetSpriteColor(id, color.x, color.y, color.z, color.w)
        patchSelected { it.copy(spriteColor = color) }
    }

    fun updateSelectedGravityScale(scale: Float) {
        val id = _selectedId.value ?: return
        NativeEngine.nativeSetRigidBodyGravityScale(id, scale)
    }

    fun updateSelectedUseGravity(use: Boolean) {
        val id = _selectedId.value ?: return
        NativeEngine.nativeSetRigidBodyUseGravity(id, use)
    }

    private fun patchSelected(transform: (EntitySnapshot) -> EntitySnapshot) {
        val id = _selectedId.value ?: return
        _entities.value = _entities.value.map { if (it.id == id) transform(it) else it }
    }

    // ---- run controls ------------------------------------------------------
    fun setRunState(target: RunState) {
        val current = _runState.value
        if (current == target) return

        // Illegal transition guard mirrors the native one; keep UI honest.
        if (target == RunState.PAUSE && current != RunState.PLAY) return
        if (target == RunState.PLAY && current == RunState.PAUSE) {
            // Pause → Play resumes natively.
            NativeEngine.setRunState(target.nativeId)
            _runState.value = target
            return
        }

        NativeEngine.setRunState(target.nativeId)
        _runState.value = target

        if (target == RunState.EDIT) {
            // Native restores the play backup asynchronously on the GL thread —
            // pull a fresh snapshot shortly after. TODO(Phase 3): native event.
            viewModelScope.launch {
                delay(150)
                refresh()
            }
            emitMessage("Play stopped — scene restored")
        }
    }

    fun togglePlayPause() {
        when (_runState.value) {
            RunState.EDIT -> setRunState(RunState.PLAY)
            RunState.PLAY -> setRunState(RunState.PAUSE)
            RunState.PAUSE -> setRunState(RunState.PLAY)
        }
    }

    // ---- UX helpers --------------------------------------------------------
    fun requestExport() {
        emitMessage("APK/AAB export ships in Phase 5 — see docs/ASSET_PIPELINE_AND_EXPORT.md")
    }

    fun requestImport() {
        emitMessage("Asset import (SAF) ships in Phase 2 — see docs/ASSET_PIPELINE_AND_EXPORT.md")
    }

    fun emitMessage(text: String) {
        _messages.tryEmit(text)
    }
}
