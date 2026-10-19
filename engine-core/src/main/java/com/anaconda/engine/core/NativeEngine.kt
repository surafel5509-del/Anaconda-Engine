// ============================================================================
//  NativeEngine.kt — the ONLY Kotlin class that touches native code.
//
//  Thin, typed façade over libanaconda_engine.so:
//    * `System.loadLibrary` triggers JNI_OnLoad, which REGISTERS every native
//      method against explicit signature strings (JniBridge.cpp). A mismatch
//      between this file and the C++ table fails immediately at class-load
//      with UnsatisfiedLinkError instead of crashing mid-frame.
//    * All calls are thread-safe on the native side (world mutex), so the
//      UI thread may call them directly — no queueEvent() indirection needed
//      for editor operations. GL work (surfaceCreated/Changed/DrawFrame) is
//      still invoked exclusively from the GLSurfaceView GL thread.
//
//  Keep in sync with: engine-core/src/main/cpp/src/platform/android/JniBridge.cpp
// ============================================================================
package com.anaconda.engine.core

object NativeEngine {

    init {
        System.loadLibrary("anaconda_engine")
    }

    // ---- constants mirrored from C++ (RunState / EditorGesture enums) ------
    const val RUN_EDIT = 0
    const val RUN_PLAY = 1
    const val RUN_PAUSE = 2

    const val GESTURE_PAN = 0
    const val GESTURE_ZOOM = 1
    const val GESTURE_TAP = 2

    // ---- lifecycle (GL thread unless noted) --------------------------------
    /** UI thread, once per process: logs boot, seeds initial viewport. */
    external fun nativeInit(width: Int, height: Int): Boolean

    /** GL thread — (re)build shaders/meshes/textures, build demo scene. */
    external fun nativeSurfaceCreated()

    /** GL thread — viewport + camera aspect. */
    external fun nativeSurfaceChanged(width: Int, height: Int)

    /** GL thread — one simulation+render step (vsync). */
    external fun nativeDrawFrame()

    /** UI thread. */
    external fun nativeOnPause()

    /** UI thread. */
    external fun nativeOnResume()

    /** GL thread. */
    external fun nativeShutdown()

    // ---- editor control ----------------------------------------------------
    /** Any thread. 0=Edit, 1=Play, 2=Pause (see RUN_* constants). */
    external fun nativeSetRunState(state: Int)

    /** Any thread — serialized entity snapshot, see SceneSnapshot.kt. */
    external fun nativeGetSceneSnapshot(): String

    /** Any thread — returns the raw entity handle (opaque Int). */
    external fun nativeCreateEntity(name: String): Int

    /** Any thread — stale handles are ignored natively. */
    external fun nativeDestroyEntity(entity: Int)

    /** Any thread. */
    external fun nativeSetEntityName(entity: Int, name: String)

    /**
     * Any thread. Rotation is in RADIANS — the ViewModel converts the
     * Inspector's degree fields at this single boundary.
     */
    external fun nativeSetTransform(
        entity: Int,
        px: Float, py: Float, pz: Float,
        rx: Float, ry: Float, rz: Float,
        sx: Float, sy: Float, sz: Float,
    )

    /** Any thread — component-wise colors in [0,1]. */
    external fun nativeSetSpriteColor(entity: Int, r: Float, g: Float, b: Float, a: Float)

    /** Any thread. */
    external fun nativeSetRigidBodyGravityScale(entity: Int, scale: Float)

    /** Any thread. */
    external fun nativeSetRigidBodyUseGravity(entity: Int, useGravity: Boolean)

    /**
     * Any thread — semantic gesture from the viewport:
     *   GESTURE_PAN   dx/dy in viewport pixels
     *   GESTURE_ZOOM  dx = pinch scale factor (>1 zooms in)
     *   GESTURE_TAP   x/y in viewport pixels (picking: Phase 3)
     */
    external fun nativeEditorGesture(type: Int, dx: Float, dy: Float, x: Float, y: Float)

    /** Any thread — smoothed frames-per-second for the toolbar chip. */
    external fun nativeGetFps(): Float

    /** Any thread. */
    external fun nativeGetEntityCount(): Int

    /** Any thread — engine semantic version string. */
    external fun nativeGetEngineVersion(): String

    // ---- Month 2: persistence + asset pipeline ----------------------------
    /**
     * Any thread. "" (empty) = SUCCESS; non-empty = error message for the
     * user. String-returning on purpose: one atomic crossing, no last-error
     * race between autosave (background) and manual save (UI).
     */
    external fun nativeSaveScene(path: String): String

    /** Any thread — same ""-means-success contract as [nativeSaveScene]. */
    external fun nativeLoadScene(path: String): String

    /** Any thread — File > New: force Edit mode and clear the world. */
    external fun nativeNewScene()

    /**
     * Any thread (call from Dispatchers.IO — decode is a blocking CPU job).
     * Returns a stable TextureHandle immediately; the GL upload is queued for
     * the next frame. 0 = decode failed.
     */
    external fun nativeImportTexture(path: String): Int

    /** Any thread — point a sprite at a texture handle (0 = white default). */
    external fun nativeSetSpriteTexture(entity: Int, textureHandle: Int)

    /**
     * Any thread — recreate an entity (ALL line-described components) from a
     * snapshot line. Powers undo-of-delete. Returns the new id, -1 on error.
     */
    external fun nativeRestoreEntity(line: String): Int

    /**
     * Any thread — texture handle previously imported for this absolute path,
     * or 0 when never imported this session (asset browser badge).
     */
    external fun nativeTextureHandleForPath(path: String): Int

    // ---- friendly wrappers (kept trivially thin on purpose) -----------------
    fun setRunState(state: Int) = nativeSetRunState(state)
    fun sceneSnapshot(): String = nativeGetSceneSnapshot()
    fun createEntity(name: String): Int = nativeCreateEntity(name)
    fun destroyEntity(entity: Int) = nativeDestroyEntity(entity)
    fun saveScene(path: String): String = nativeSaveScene(path)
    fun loadScene(path: String): String = nativeLoadScene(path)
    fun newScene(): String = nativeNewScene()
    fun importTexture(path: String): Int = nativeImportTexture(path)
    fun restoreEntity(line: String): Int = nativeRestoreEntity(line)
    fun fps(): Float = nativeGetFps()
    fun version(): String = nativeGetEngineVersion()
}
