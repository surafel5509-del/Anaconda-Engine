// ============================================================================
//  UndoStack.kt — editor history (Month 2).
//
//  Snapshot-based linear history, capacity-capped (50 — matches roadmap).
//  Three command kinds cover every Phase-2 mutation channel:
//
//    ValueEdit  transform / name / sprite tint / gravity edits
//               → apply(before) / apply(after), zero native round-trips per
//                 direction beyond the property write itself
//    Create     undo = destroy(id) · redo = re-create + re-apply (new id)
//    Delete     undo = nativeRestoreEntity(snapshotLine) → NEW id, then
//                 REMAP every other entry (old → new) so earlier history
//                 stays valid; redo = destroy(current id)
//
//  Rules enforced by the ViewModel:
//    * any new mutation clears the REDO branch
//    * Load / New / Play clear BOTH stacks (history never crosses a
//      world-swap or simulation boundary — ids and state stop meaning it)
// ============================================================================
package com.anaconda.editor

import com.anaconda.engine.core.EntitySnapshot

/**
 * Applies a [UndoStack.Props] bag to a live entity (native write + local
 * snapshot patch). Top-level because Kotlin forbids nested type aliases.
 */
typealias PropsApplier = (entityId: Int, props: UndoStack.Props) -> Unit

class UndoStack(private val capacity: Int = 50) {

    /** Property bag: null fields = "not part of this edit". */
    data class Props(
        val name: String? = null,
        val position: com.anaconda.engine.core.Vec3F? = null,
        val rotationDeg: com.anaconda.engine.core.Vec3F? = null,
        val scale: com.anaconda.engine.core.Vec3F? = null,
        val color: com.anaconda.engine.core.Vec4F? = null,
        val gravityScale: Float? = null,
        val useGravity: Boolean? = null,
        val textureHandle: Int? = null,
        /** Full-line restore (delete undo) — when set, Props above ignored. */
        val snapshotLine: String? = null,
    )

    sealed class Command {
        abstract val label: String

        /** Mutable: delete-undo remaps ids after minting a replacement. */
        abstract var targetId: Int

        /** Run on user Undo. */
        abstract fun undo()

        /** Run on user Redo. */
        abstract fun redo()

        /** Keep this command meaningful after its entity was re-created. */
        open fun remapId(oldId: Int, newId: Int) {
            if (targetId == oldId) targetId = newId
        }
    }

    class ValueEditCommand(
        override val label: String,
        override var targetId: Int,
        private val before: Props,
        private var after: Props,
        private val apply: PropsApplier,
        /**
         * Stroke group ("tf", "name", "color", "tex"). Consecutive pushes
         * with the same (targetId, group) MERGE instead of stacking, so a
         * NumberField keystroke burst ("1" → "12" → "123") stays a single
         * undo step — the `before` of the first stroke survives.
         */
        val group: String? = null,
    ) : Command() {
        override fun undo() = apply(targetId, before)
        override fun redo() = apply(targetId, after)
        /** Adopt the newer stroke's end state; keep OUR `before`. */
        fun mergeFrom(other: ValueEditCommand) { after = other.after }
    }

    class CreateCommand(
        override val label: String,
        override var targetId: Int,
        private val props: Props,
        private val apply: PropsApplier,
        private val destroy: (Int) -> Unit,
        private val create: (Props) -> Int,
    ) : Command() {
        override fun undo() = destroy(targetId)
        override fun redo() { targetId = create(props) }
    }

    class DeleteCommand(
        override val label: String,
        override var targetId: Int,
        private val snapshotLine: String,
        private val restore: (String) -> Int,
        private val destroy: (Int) -> Unit,
    ) : Command() {
        override fun undo() { targetId = restore(snapshotLine) }
        override fun redo() = destroy(targetId)
    }

    private val undoStack = ArrayDeque<Command>()
    private val redoStack = ArrayDeque<Command>()

    val canUndo: Boolean get() = undoStack.isNotEmpty()
    val canRedo: Boolean get() = redoStack.isNotEmpty()
    val undoLabel: String? get() = undoStack.lastOrNull()?.label
    val redoLabel: String? get() = redoStack.lastOrNull()?.label

    fun push(command: Command) {
        // Stroke coalescing — see ValueEditCommand.group docs.
        val top = undoStack.lastOrNull()
        if (command is ValueEditCommand &&
            top is ValueEditCommand &&
            command.group != null &&
            command.group == top.group &&
            command.targetId == top.targetId
        ) {
            top.mergeFrom(command)
            redoStack.clear()
            return
        }
        undoStack.addLast(command)
        redoStack.clear()          // branching history dies on new edits
        while (undoStack.size > capacity) undoStack.removeFirst()
    }

    /**
     * Runs the top command's inverse. When that inverse RE-CREATES its entity
     * (delete-undo), the fresh id is remapped across BOTH branches so the
     * rest of history stays pointed at the right entity.
     *
     * @return the executed command (its label and final targetId), or null
     * when history is empty.
     */
    fun undo(): Command? {
        val command = undoStack.removeLastOrNull() ?: return null
        val oldId = command.targetId
        command.undo()
        redoStack.addLast(command)
        if (command.targetId != oldId) {
            remapId(oldId, command.targetId)
        }
        return command
    }

    /** Symmetric to [undo]; create-redo can also mint a new id. */
    fun redo(): Command? {
        val command = redoStack.removeLastOrNull() ?: return null
        val oldId = command.targetId
        command.redo()
        undoStack.addLast(command)
        if (command.targetId != oldId) {
            remapId(oldId, command.targetId)
        }
        return command
    }

    /** Rewrite old → new across BOTH branches (delete-undo path). */
    fun remapId(oldId: Int, newId: Int) {
        if (oldId == newId || newId < 0) return
        undoStack.forEach { it.remapId(oldId, newId) }
        redoStack.forEach { it.remapId(oldId, newId) }
    }

    fun clear() {
        undoStack.clear()
        redoStack.clear()
    }

    companion object {
        /**
         * Capture the full restorable state of an entity as Props + the
         * native snapshot line used by delete-undo.
         */
        fun propsOf(entity: EntitySnapshot): Props = Props(
            name = entity.name,
            position = entity.position,
            rotationDeg = entity.rotationDeg,
            scale = entity.scale,
            color = entity.spriteColor,
            textureHandle = entity.textureHandle,
        )
    }
}
