// ============================================================================
//  EditorViewModel.kt — single source of truth for the editor UI.
//
//  State holders (StateFlow → Compose via collectAsState):
//    entities       every engine entity as an immutable snapshot row
//    selectedId     hierarchy/inspector selection
//    runState       Edit / Play / Pause (mirrors the native engine)
//    fps            smoothed native frame rate
//    messages       one-shot snackbars
//    projectAssets  real sprite files from the active project folder
//    dirty/scene    persistence + title bar
//    canUndo/canRedo history availability
//
//  Sync strategy (see docs/ARCHITECTURE.md §5.4):
//    DOWN  (edits)  → immediate native call + local patch (zero latency)
//    UP    (pulled) → scene snapshot after lifecycle events / play-stop /
//                     manual refresh / while PLAY auto-refresh selection.
//  Phase 3 replaces polling with a native dirty-flag delta channel.
//
//  Month-2 persistence contract:
//    save   → filesDir/projects/<name>/scenes/main.scene, clears [dirty]
//    load   → auto-saves dirty work first (never silently discards edits)
//    new    → auto-saves dirty work, then nativeNewScene
//    errors → snackbar with the native message ("" success string means OK)
//    autosave every 60 s while EDIT + dirty, silent on success
//
//  Undo/redo contract (see editor/UndoStack.kt):
//    value edits push coalesced strokes, create/delete push commands,
//    history is CLEARED on scene load/new and on entering PLAY.
// ============================================================================
package com.anaconda.editor

import android.app.Application
import android.net.Uri
import android.provider.OpenableColumns
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.anaconda.engine.core.EntitySnapshot
import com.anaconda.engine.core.MathConversions
import com.anaconda.engine.core.NativeEngine
import com.anaconda.engine.core.Vec3F
import com.anaconda.engine.core.Vec4F
import com.anaconda.engine.core.parseSceneSnapshot
import com.anaconda.editor.project.ProjectManager
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

// ---------------------------------------------------------------------------
// Run modes (mirrors native RunState ordinals)
// ---------------------------------------------------------------------------
enum class RunState(val nativeId: Int) {
    EDIT(NativeEngine.RUN_EDIT),
    PLAY(NativeEngine.RUN_PLAY),
    PAUSE(NativeEngine.RUN_PAUSE),
}

// ---------------------------------------------------------------------------
// Asset browser row. `handle` is the AssetManager handle minted when this
// session imported the file; 0 means "load on demand on first assign".
// `path` is the absolute in-project file location.
// ---------------------------------------------------------------------------
data class AssetEntry(
    val name: String,
    val kind: String,
    val color: Vec4F,
    val isPlaceholder: Boolean = false,
    val path: String = "",
    val handle: Int = 0,
)

class EditorViewModel(application: Application) : AndroidViewModel(application) {

    // ---- state -------------------------------------------------------------
    private val _entities = MutableStateFlow<List<EntitySnapshot>>(emptyList())
    val entities: StateFlow<List<EntitySnapshot>> = _entities.asStateFlow()

    private val _selectedId = MutableStateFlow<Int?>(null)
    val selectedId: StateFlow<Int?> = _selectedId.asStateFlow()

    private val _runState = MutableStateFlow(RunState.EDIT)
    val runState: StateFlow<RunState> = _runState.asStateFlow()

    private val _fps = MutableStateFlow(0f)
    val fps: StateFlow<Float> = _fps.asStateFlow()

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 8)
    val messages: SharedFlow<String> = _messages.asSharedFlow()

    private val _projectAssets = MutableStateFlow<List<AssetEntry>>(emptyList())
    val projectAssets: StateFlow<List<AssetEntry>> = _projectAssets.asStateFlow()

    private val _dirty = MutableStateFlow(false)
    val dirty: StateFlow<Boolean> = _dirty.asStateFlow()

    private val _sceneLabel = MutableStateFlow("…/main.scene")
    val sceneLabel: StateFlow<String> = _sceneLabel.asStateFlow()

    private val _canUndo = MutableStateFlow(false)
    val canUndo: StateFlow<Boolean> = _canUndo.asStateFlow()

    private val _canRedo = MutableStateFlow(false)
    val canRedo: StateFlow<Boolean> = _canRedo.asStateFlow()

    val engineVersion: String = NativeEngine.version()

    private val undoStack = UndoStack(capacity = 50)

    init {
        // Project folders must exist before any save/load/import call.
        ProjectManager.init(application)
        _sceneLabel.value = ProjectManager.currentProject().let {
            "${it.name} / ${it.sceneFile.name}"
        }

        // Initial pull — the demo scene is built natively on first GL surface,
        // which happens shortly after launch (hence the delayed retry).
        refresh()
        refreshAssets()
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

        // Periodic autosave (60 s cadence — docs/ROADMAP.md Month 2).
        // Silent on success, snackbar on failure, never runs mid-simulation.
        viewModelScope.launch {
            while (isActive) {
                delay(60_000)
                if (_dirty.value && _runState.value == RunState.EDIT) {
                    saveScene(silent = true)
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

    // ---- persistence (Month 2) ---------------------------------------------
    /**
     * Serialize the live world to the project scene file.
     * @param silent suppress the success snackbar (autosave path).
     */
    fun saveScene(silent: Boolean = false) {
        runCatching {
            val path = ProjectManager.currentProject().sceneFile.absolutePath
            val error = NativeEngine.saveScene(path)   // "" == success
            if (error.isNotEmpty()) {
                emitMessage("Save failed: $error")
                return
            }
            _dirty.value = false
            if (!silent) emitMessage("Scene saved → ${ProjectManager.currentProject().sceneFile.name}")
        }.onFailure {
            emitMessage("Save failed: ${it.message ?: "unknown error"}")
        }
    }

    /**
     * Load the project scene. Dirty work is auto-saved first so Load can
     * never silently discard edits (the snackbar surfaces a failed save).
     */
    fun loadScene() {
        if (!ensureSaved("load")) return
        runCatching {
            val error = NativeEngine.loadScene(
                ProjectManager.currentProject().sceneFile.absolutePath,
            )
            if (error.isNotEmpty()) {
                emitMessage("Load failed: $error")
                return
            }
            _dirty.value = false
            undoStack.clear()
            syncUndoState()
            _selectedId.value = null
            refresh()
            emitMessage("Scene loaded")
        }.onFailure {
            emitMessage("Load failed: ${it.message ?: "unknown error"}")
        }
    }

    /** Start a fresh scene (current work auto-saved first). */
    fun newScene() {
        if (!ensureSaved("start a new scene")) return
        runCatching {
            val error = NativeEngine.newScene()
            if (error.isNotEmpty()) {
                emitMessage("New scene failed: $error")
                return
            }
            _dirty.value = false
            undoStack.clear()
            syncUndoState()
            _selectedId.value = null
            refresh()
            refreshAssets()
            emitMessage("New scene created")
        }.onFailure {
            emitMessage("New scene failed: ${it.message ?: "unknown error"}")
        }
    }

    /**
     * Auto-save-before-destructive-action helper.
     * @return true when it is safe to proceed (saved or had nothing to save).
     */
    private fun ensureSaved(action: String): Boolean {
        if (!_dirty.value) return true
        saveScene(silent = true)
        if (_dirty.value) {
            emitMessage("Cannot $action — scene is not saved (see snackbar error)")
            return false
        }
        emitMessage("Unsaved changes saved first")
        return true
    }

    // ---- asset import (SAF → project folder → decoder → GL queue) ----------
    /**
     * Copy `uri` into the project sprite folder and decode it on an IO
     * thread. Auto-assigns the texture to the selected sprite entity (the
     * Month-2 primary flow: import → see it on the sprite).
     */
    fun importSprite(uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            runCatching {
                val resolver = getApplication<Application>().contentResolver
                val name = displayNameOf(uri)
                    ?: File(uri.lastPathSegment ?: "sprite").name
                val bytes = resolver.openInputStream(uri)?.use { it.readBytes() }
                    ?: throw java.io.IOException("Cannot read the selected file")
                val file = ProjectManager.importSpriteBytes(name, bytes)
                val handle = NativeEngine.importTexture(file.absolutePath)
                if (handle == 0) {
                    throw java.io.IOException("Unsupported or corrupt image (PNG/JPEG only)")
                }
                file.name to handle
            }.onSuccess { (fileName, handle) ->
                withContext(Dispatchers.Main) {
                    refreshAssets()
                    emitMessage("Imported $fileName")
                    val id = _selectedId.value
                    val entity = _entities.value.firstOrNull { it.id == id }
                    if (id != null && entity != null && entity.hasSprite) {
                        assignTexture(id, entity, handle, fileName)
                    }
                }
            }.onFailure { error ->
                withContext(Dispatchers.Main) {
                    emitMessage("Import failed: ${error.message ?: "unknown error"}")
                }
            }
        }
    }

    /** Tap-to-assign from the asset browser (loads on demand if needed). */
    fun assignAssetToSelected(asset: AssetEntry) {
        val id = _selectedId.value
        val entity = _entities.value.firstOrNull { it.id == id }
        if (id == null || entity == null) {
            emitMessage("Select a sprite entity to assign ${asset.name}")
            return
        }
        if (!entity.hasSprite) {
            emitMessage("Selected entity has no Sprite component")
            return
        }
        viewModelScope.launch(Dispatchers.IO) {
            val handle = if (asset.handle != 0) {
                asset.handle
            } else {
                NativeEngine.importTexture(asset.path)   // first touch this session
            }
            if (handle == 0) {
                withContext(Dispatchers.Main) {
                    emitMessage("Could not load ${asset.name}")
                }
                return@launch
            }
            withContext(Dispatchers.Main) {
                assignTexture(id, entity, handle, asset.name)
                refreshAssets()   // handle badge may have just been minted
            }
        }
    }

    /** Down-channel + undo push + dirty flag, shared by both assign paths. */
    private fun assignTexture(
        id: Int,
        entity: EntitySnapshot,
        handle: Int,
        label: String,
    ) {
        if (entity.textureHandle == handle) return
        NativeEngine.nativeSetSpriteTexture(id, handle)
        patchEntity(id) { it.copy(textureHandle = handle) }
        pushUndo(
            UndoStack.ValueEditCommand(
                label = "Texture $label",
                targetId = id,
                before = UndoStack.Props(textureHandle = entity.textureHandle),
                after = UndoStack.Props(textureHandle = handle),
                apply = ::applyProps,
                group = "tex",
            ),
        )
        markDirty()
        emitMessage("$label → ${entity.name}")
    }

    /** Re-scan the project sprite folder (import, load, manual refresh). */
    fun refreshAssets() {
        runCatching {
            _projectAssets.value = ProjectManager.listSpriteFiles().map { file ->
                AssetEntry(
                    name = file.name,
                    kind = "Sprite",
                    color = colorForName(file.name),
                    path = file.absolutePath,
                    handle = NativeEngine.nativeTextureHandleForPath(file.absolutePath),
                )
            }
        }.onFailure {
            emitMessage("Asset listing failed: ${it.message ?: "unknown error"}")
        }
    }

    // ---- entity mutation (down-channel: immediate native + local patch) -----
    fun createEntity() {
        val name = uniqueName("Entity")
        val id = NativeEngine.createEntity(name)
        refresh()
        _selectedId.value = id
        markDirty()
        pushUndo(
            UndoStack.CreateCommand(
                label = "Create $name",
                targetId = id,
                props = UndoStack.Props(name = name),
                apply = ::applyProps,
                destroy = ::destroyNative,
                create = ::createFromProps,
            ),
        )
        emitMessage("Created $name")
    }

    fun deleteSelected() {
        val id = _selectedId.value ?: return
        val entity = selectedEntity ?: return
        // The codec line carries EVERY restorable component (incl. texture) —
        // this is what nativeRestoreEntity replays on undo.
        val line = NativeEngine.sceneSnapshot()
            .lineSequence()
            .firstOrNull { it.startsWith("$id|") }
            .orEmpty()
        NativeEngine.destroyEntity(id)
        _selectedId.value = null
        refresh()
        markDirty()
        if (line.isNotEmpty()) {
            pushUndo(
                UndoStack.DeleteCommand(
                    label = "Delete ${entity.name}",
                    targetId = id,
                    snapshotLine = line,
                    restore = { NativeEngine.restoreEntity(it) },
                    destroy = ::destroyNative,
                ),
            )
        }
        emitMessage("Deleted ${entity.name}")
    }

    fun renameSelected(newName: String) {
        val id = _selectedId.value ?: return
        if (newName.isBlank()) return
        val before = selectedEntity?.name ?: return
        if (before == newName) return
        NativeEngine.nativeSetEntityName(id, newName)
        patchEntity(id) { it.copy(name = newName) }
        pushValueEdit(
            label = "Rename → $newName",
            id = id,
            before = UndoStack.Props(name = before),
            after = UndoStack.Props(name = newName),
            group = "name",
        )
        markDirty()
    }

    /** Position/rotation(deg)/scale edit from the Inspector. */
    fun updateSelectedTransform(position: Vec3F, rotationDeg: Vec3F, scale: Vec3F) {
        val id = _selectedId.value ?: return
        val current = selectedEntity ?: return
        NativeEngine.nativeSetTransform(
            id,
            position.x, position.y, position.z,
            MathConversions.degToRad(rotationDeg.x),
            MathConversions.degToRad(rotationDeg.y),
            MathConversions.degToRad(rotationDeg.z),
            scale.x, scale.y, scale.z,
        )
        patchEntity(id) {
            it.copy(
                position = position,
                rotationRad = Vec3F(
                    MathConversions.degToRad(rotationDeg.x),
                    MathConversions.degToRad(rotationDeg.y),
                    MathConversions.degToRad(rotationDeg.z),
                ),
                scale = scale,
            )
        }
        pushValueEdit(
            label = "Transform",
            id = id,
            before = UndoStack.Props(
                position = current.position,
                rotationDeg = current.rotationDeg,
                scale = current.scale,
            ),
            after = UndoStack.Props(
                position = position,
                rotationDeg = rotationDeg,
                scale = scale,
            ),
            group = "tf",   // keystroke bursts coalesce into one history step
        )
        markDirty()
    }

    fun updateSelectedSpriteColor(color: Vec4F) {
        val id = _selectedId.value ?: return
        val before = selectedEntity?.spriteColor ?: return
        NativeEngine.nativeSetSpriteColor(id, color.x, color.y, color.z, color.w)
        patchEntity(id) { it.copy(spriteColor = color) }
        pushValueEdit(
            label = "Sprite tint",
            id = id,
            before = UndoStack.Props(color = before),
            after = UndoStack.Props(color = color),
            group = "color",
        )
        markDirty()
    }

    // Gravity edits apply natively but intentionally do NOT push history yet:
    // the snapshot protocol doesn't round-trip rb values (so `before` would
    // be guesswork), and sliders flood the stack per tick. Month 3 extends
    // the snapshot with RigidBody fields and enables coalesced gravity undo.
    fun updateSelectedGravityScale(scale: Float) {
        val id = _selectedId.value ?: return
        NativeEngine.nativeSetRigidBodyGravityScale(id, scale)
        markDirty()
    }

    fun updateSelectedUseGravity(use: Boolean) {
        val id = _selectedId.value ?: return
        NativeEngine.nativeSetRigidBodyUseGravity(id, use)
        markDirty()
    }

    private fun patchEntity(id: Int, transform: (EntitySnapshot) -> EntitySnapshot) {
        _entities.value = _entities.value.map { if (it.id == id) transform(it) else it }
    }

    // ---- undo / redo (Month 2) ---------------------------------------------
    fun undo() {
        val command = undoStack.undo() ?: return
        syncUndoState()
        refresh()
        markDirty()
        // Focus what just changed — for delete-undo this is the restored id.
        if (command.targetId != 0 && command.targetId >= 0) {
            _selectedId.value = command.targetId
        }
        emitMessage("Undo: ${command.label}")
    }

    fun redo() {
        val command = undoStack.redo() ?: return
        syncUndoState()
        refresh()
        markDirty()
        if (command.targetId != 0 && command.targetId >= 0) {
            _selectedId.value = command.targetId
        }
        emitMessage("Redo: ${command.label}")
    }

    private fun pushUndo(command: UndoStack.Command) {
        undoStack.push(command)
        syncUndoState()
    }

    private fun pushValueEdit(
        label: String,
        id: Int,
        before: UndoStack.Props,
        after: UndoStack.Props,
        group: String?,
    ) {
        pushUndo(
            UndoStack.ValueEditCommand(
                label = label,
                targetId = id,
                before = before,
                after = after,
                apply = ::applyProps,
                group = group,
            ),
        )
    }

    private fun syncUndoState() {
        _canUndo.value = undoStack.canUndo
        _canRedo.value = undoStack.canRedo
    }

    private fun markDirty() {
        _dirty.value = true
    }

    /**
     * Props applier — the inverse direction of a value-edit command. Missing
     * fields fall back to the CURRENT entity state so partial edits (name
     * only, color only) never clobber the untouched fields.
     */
    private fun applyProps(id: Int, props: UndoStack.Props) {
        if (props.snapshotLine != null) {
            // Defensive: snapshot restorations belong to DeleteCommand.
            NativeEngine.restoreEntity(props.snapshotLine)
            refresh()
            return
        }
        val current = _entities.value.firstOrNull { it.id == id }

        props.name?.let { NativeEngine.nativeSetEntityName(id, it) }

        val pos = props.position ?: current?.position
        val rotRad = props.rotationDeg?.let {
            Vec3F(
                MathConversions.degToRad(it.x),
                MathConversions.degToRad(it.y),
                MathConversions.degToRad(it.z),
            )
        } ?: current?.rotationRad
        val scale = props.scale ?: current?.scale
        if ((props.position != null || props.rotationDeg != null || props.scale != null) &&
            pos != null && rotRad != null && scale != null
        ) {
            NativeEngine.nativeSetTransform(
                id,
                pos.x, pos.y, pos.z,
                rotRad.x, rotRad.y, rotRad.z,   // already radians
                scale.x, scale.y, scale.z,
            )
        }

        props.color?.let { c ->
            NativeEngine.nativeSetSpriteColor(id, c.x, c.y, c.z, c.w)
        }
        props.textureHandle?.let { h ->
            NativeEngine.nativeSetSpriteTexture(id, h)
        }
        props.gravityScale?.let { NativeEngine.nativeSetRigidBodyGravityScale(id, it) }
        props.useGravity?.let { NativeEngine.nativeSetRigidBodyUseGravity(id, it) }

        // Mirror everything into the local snapshot row (down-channel rule).
        patchEntity(id) { e ->
            e.copy(
                name = props.name ?: e.name,
                position = props.position ?: e.position,
                rotationRad = props.rotationDeg?.let {
                    Vec3F(
                        MathConversions.degToRad(it.x),
                        MathConversions.degToRad(it.y),
                        MathConversions.degToRad(it.z),
                    )
                } ?: e.rotationRad,
                scale = props.scale ?: e.scale,
                spriteColor = props.color ?: e.spriteColor,
                textureHandle = props.textureHandle ?: e.textureHandle,
            )
        }
    }

    private fun destroyNative(id: Int) {
        NativeEngine.destroyEntity(id)
        if (_selectedId.value == id) _selectedId.value = null
        refresh()
    }

    private fun createFromProps(props: UndoStack.Props): Int {
        val id = NativeEngine.createEntity(props.name ?: "Entity")
        applyProps(id, props)
        refresh()
        return id
    }

    private fun uniqueName(prefix: String): String {
        var i = 1
        while (_entities.value.any { it.name == "$prefix $i" }) i++
        return "$prefix $i"
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

        // History never crosses a simulation boundary (ids/state drift while
        // physics runs; Stop restores the play-backup snapshot).
        if (target == RunState.PLAY && current == RunState.EDIT) {
            undoStack.clear()
            syncUndoState()
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

    fun emitMessage(text: String) {
        _messages.tryEmit(text)
    }

    // ---- private helpers ---------------------------------------------------
    private fun displayNameOf(uri: Uri): String? = runCatching {
        getApplication<Application>().contentResolver
            .query(uri, null, null, null, null)?.use { cursor ->
                val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (index >= 0 && cursor.moveToFirst()) cursor.getString(index) else null
            }
    }.getOrNull()

    /** Deterministic tile tint per filename (no real thumbnails until Phase 6). */
    private fun colorForName(name: String): Vec4F {
        val hue = ((name.hashCode() % 360 + 360) % 360) / 360f
        val h6 = hue * 6f
        val i = h6.toInt() % 6
        val f = h6 - h6.toInt()
        val q = 1f - f
        return when (i) {
            0 -> Vec4F(1f, f, 0f, 1f)
            1 -> Vec4F(q, 1f, 0f, 1f)
            2 -> Vec4F(0f, 1f, f, 1f)
            3 -> Vec4F(0f, q, 1f, 1f)
            4 -> Vec4F(f, 0f, 1f, 1f)
            else -> Vec4F(1f, 0f, q, 1f)
        }
    }
}
