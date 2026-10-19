// ============================================================================
//  SceneSnapshot.kt — engine → editor data model.
//
//  The native engine serializes every live entity as ONE line:
//
//      id|name|px,py,pz|rx,ry,rz|sx,sy,sz|flags|cr,cg,cb,ca|textureHandle
//
//    * id         opaque entity handle (generation|index packed to UInt bits,
//                 carried as a signed Int bit-pattern across JNI)
//    * rotation   RADIANS (native canonical unit)
//    * flags      bit0 = has Sprite, bit1 = has RigidBody, bit2 = has Collider
//    * color      sprite tint in [0,1] (defaults to 1,1,1,1 without a sprite)
//    * texture    AssetManager handle (0 = untextured white; column may be
//                 absent in pre-Month-2 snapshots — parser tolerates that)
//
//  Why a line protocol instead of per-field JNI getters? One crossing per
//  refresh, trivially testable parser, and the format can grow fields without
//  breaking older UI code (parser tolerates extra trailing columns in Phase 3).
// ============================================================================
package com.anaconda.engine.core

// ---------------------------------------------------------------------------
// Immutable value types
// ---------------------------------------------------------------------------
data class Vec3F(val x: Float = 0f, val y: Float = 0f, val z: Float = 0f) {
    companion object {
        val ZERO = Vec3F(0f, 0f, 0f)
        val ONE = Vec3F(1f, 1f, 1f)
    }
}

data class Vec4F(val x: Float = 1f, val y: Float = 1f, val z: Float = 1f, val w: Float = 1f) {
    companion object {
        val WHITE = Vec4F(1f, 1f, 1f, 1f)
    }
}

/**
 * One row of the engine snapshot. Rotation is stored in radians; UI helpers
 * below convert to/from degrees at display boundaries.
 *
 * Column layout (see EditorSnapshotCodec.hpp):
 *   0 id | 1 name | 2 pos | 3 rot | 4 scale | 5 flags | 6 color | 7 texture
 */
data class EntitySnapshot(
    val id: Int,
    val name: String,
    val position: Vec3F,
    val rotationRad: Vec3F,
    val scale: Vec3F,
    val hasSprite: Boolean,
    val hasRigidBody: Boolean,
    val hasCollider: Boolean,
    val spriteColor: Vec4F,
    /** AssetManager texture handle; 0 = untextured (white default). */
    val textureHandle: Int = 0,
) {
    val rotationDeg: Vec3F
        get() = Vec3F(
            Math.toDegrees(rotationRad.x.toDouble()).toFloat(),
            Math.toDegrees(rotationRad.y.toDouble()).toFloat(),
            Math.toDegrees(rotationRad.z.toDouble()).toFloat(),
        )

    val componentSummary: String
        get() = buildList {
            add("Transform")
            if (hasSprite) add("Sprite")
            if (hasRigidBody) add("RigidBody")
            if (hasCollider) add("BoxCollider")
        }.joinToString(" · ")
}

// ---------------------------------------------------------------------------
// Flags bitmask (mirrors JniBridge/Engine::sceneSnapshot)
// ---------------------------------------------------------------------------
private const val FLAG_SPRITE = 1
private const val FLAG_RIGID_BODY = 2
private const val FLAG_COLLIDER = 4

/**
 * Parse the native snapshot string. Malformed lines are skipped rather than
 * thrown — a partial snapshot must never crash the editor UI.
 */
fun parseSceneSnapshot(raw: String): List<EntitySnapshot> {
    if (raw.isEmpty()) return emptyList()
    return raw.lineSequence()
        .mapNotNull { line ->
            if (line.isBlank()) return@mapNotNull null
            val parts = line.split('|')
            if (parts.size < 7) return@mapNotNull null
            runCatching {
                // Entity handles are UInt bit-patterns → Long → Int keeps bits.
                val id = parts[0].toLong().toInt()
                val name = parts[1]
                val position = parts[2].toVec3()
                val rotation = parts[3].toVec3()
                val scale = parts[4].toVec3()
                val flags = parts[5].toInt()
                val color = parts[6].toVec4()
                // Column 7 arrived with the Month-2 asset pipeline; tolerate
                // its absence so older snapshots still parse.
                val textureHandle = parts.getOrNull(7)?.trim()?.toInt() ?: 0
                EntitySnapshot(
                    id = id,
                    name = name,
                    position = position,
                    rotationRad = rotation,
                    scale = scale,
                    hasSprite = flags and FLAG_SPRITE != 0,
                    hasRigidBody = flags and FLAG_RIGID_BODY != 0,
                    hasCollider = flags and FLAG_COLLIDER != 0,
                    spriteColor = color,
                    textureHandle = textureHandle,
                )
            }.getOrNull()
        }
        .toList()
}

private fun String.toVec3(): Vec3F {
    val t = split(',')
    require(t.size >= 3) { "vec3 expects 3 components: $this" }
    return Vec3F(t[0].trim().toFloat(), t[1].trim().toFloat(), t[2].trim().toFloat())
}

private fun String.toVec4(): Vec4F {
    val t = split(',')
    require(t.size >= 4) { "vec4 expects 4 components: $this" }
    return Vec4F(t[0].trim().toFloat(), t[1].trim().toFloat(), t[2].trim().toFloat(), t[3].trim().toFloat())
}

// ---------------------------------------------------------------------------
// Unit-testable helpers for the ViewModel
// ---------------------------------------------------------------------------
object MathConversions {
    fun degToRad(deg: Float): Float = Math.toRadians(deg.toDouble()).toFloat()
    fun radToDeg(rad: Float): Float = Math.toDegrees(rad.toDouble()).toFloat()
}
