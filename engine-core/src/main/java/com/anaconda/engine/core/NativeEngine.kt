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

    // ---- Month 3: picking / sorting / duplicate / stats ---------------
    /** Any thread — deep copy of an entity; -1 on stale handle. */
    external fun nativeDuplicateEntity(entity: Int): Int

    /** Any thread — sprite draw order (lower = behind). */
    external fun nativeSetSpriteSortingOrder(entity: Int, layer: Int)

    /** Any thread — viewport tap → entity id or -1 (AABB point test). */
    external fun nativePickEntity(x: Float, y: Float): Int

    /** Any thread — GL frame stats (written on GL thread, read on UI). */
    external fun nativeGetDrawCalls(): Int
    external fun nativeGetSpriteCount(): Int

    // ---- friendly wrappers (kept trivially thin on purpose) -----------------
    fun setRunState(state: Int) = nativeSetRunState(state)
    fun sceneSnapshot(): String = nativeGetSceneSnapshot()
    fun createEntity(name: String): Int = nativeCreateEntity(name)
    fun destroyEntity(entity: Int) = nativeDestroyEntity(entity)
    fun saveScene(path: String): String = nativeSaveScene(path)
    fun loadScene(path: String): String = nativeLoadScene(path)
    /** Infallible in-memory reset (native ()V — cannot fail). */
    fun newScene() = nativeNewScene()
    fun importTexture(path: String): Int = nativeImportTexture(path)
    fun restoreEntity(line: String): Int = nativeRestoreEntity(line)
    fun fps(): Float = nativeGetFps()
    fun version(): String = nativeGetEngineVersion()
    fun duplicateEntity(entity: Int): Int = nativeDuplicateEntity(entity)
    fun setSpriteSortingOrder(entity: Int, layer: Int) = nativeSetSpriteSortingOrder(entity, layer)
    fun pickEntity(x: Float, y: Float): Int = nativePickEntity(x, y)
    fun drawCalls(): Int = nativeGetDrawCalls()
    fun spriteCount(): Int = nativeGetSpriteCount()

    // ---- Month 3-5: grid/gizmo/prefab/anim/script/camera/export ----------
    external fun nativeSetGridEnabled(enabled: Boolean)
    external fun nativeSetGridSnap(snap: Boolean)
    external fun nativeSetGridCellSize(size: Float)
    external fun nativeSetGizmoMode(mode: Int)
    external fun nativeGizmoBeginDrag(x: Float, y: Float): Boolean
    external fun nativeGizmoDrag(x: Float, y: Float)
    external fun nativeGizmoEndDrag()
    external fun nativeFrameSelected(entity: Int)
    external fun nativeSetPhysicsMaterial(entity: Int, friction: Float, restitution: Float)
    external fun nativeSetColliderDebugDraw(enabled: Boolean)
    external fun nativeSetFixedTimestep(enabled: Boolean)
    external fun nativeSavePrefab(entity: Int, path: String): String
    external fun nativeInstantiatePrefab(path: String, ox: Float, oy: Float, oz: Float): Int
    external fun nativeCreateAnimationClip(name: String, frames: IntArray, fps: Float, loop: Boolean): String
    external fun nativeSetAnimatorClip(entity: Int, clip: String)
    external fun nativeSetAnimatorPlaying(entity: Int, playing: Boolean)
    external fun nativeSetAnimatorSpeed(entity: Int, speed: Float)
    external fun nativeLoadScript(path: String, source: String): String
    external fun nativeBindScript(entity: Int, path: String): String
    external fun nativeHotReloadScript(entity: Int, source: String): String
    external fun nativeCameraShake(amount: Float, duration: Float)
    external fun nativeCameraZoomTo(target: Float, duration: Float)
    external fun nativeExportProject(packageName: String, appName: String, versionName: String, versionCode: Int, outPath: String, target: Int): String
    external fun nativeGetLastScriptError(): String

    // Convenience wrappers
    fun setGridEnabled(v: Boolean) = nativeSetGridEnabled(v)
    fun setGridSnap(v: Boolean) = nativeSetGridSnap(v)
    fun setGridCellSize(s: Float) = nativeSetGridCellSize(s)
    fun setGizmoMode(m: Int) = nativeSetGizmoMode(m)
    fun gizmoBeginDrag(x: Float, y: Float) = nativeGizmoBeginDrag(x, y)
    fun gizmoDrag(x: Float, y: Float) = nativeGizmoDrag(x, y)
    fun gizmoEndDrag() = nativeGizmoEndDrag()
    fun frameSelected(e: Int) = nativeFrameSelected(e)
    fun setPhysicsMaterial(e: Int, f: Float, r: Float) = nativeSetPhysicsMaterial(e, f, r)
    fun setColliderDebugDraw(v: Boolean) = nativeSetColliderDebugDraw(v)
    fun setFixedTimestep(v: Boolean) = nativeSetFixedTimestep(v)
    fun savePrefab(e: Int, path: String): String = nativeSavePrefab(e, path)
    fun instantiatePrefab(path: String, ox: Float, oy: Float, oz: Float): Int = nativeInstantiatePrefab(path, ox, oy, oz)
    fun createAnimationClip(n: String, frames: IntArray, fps: Float, loop: Boolean): String = nativeCreateAnimationClip(n, frames, fps, loop)
    fun setAnimatorClip(e: Int, clip: String) = nativeSetAnimatorClip(e, clip)
    fun setAnimatorPlaying(e: Int, p: Boolean) = nativeSetAnimatorPlaying(e, p)
    fun setAnimatorSpeed(e: Int, s: Float) = nativeSetAnimatorSpeed(e, s)
    fun loadScript(path: String, src: String): String = nativeLoadScript(path, src)
    fun bindScript(e: Int, path: String): String = nativeBindScript(e, path)
    fun hotReloadScript(e: Int, src: String): String = nativeHotReloadScript(e, src)
    fun cameraShake(a: Float, d: Float) = nativeCameraShake(a, d)
    fun cameraZoomTo(t: Float, d: Float) = nativeCameraZoomTo(t, d)
    fun exportProject(pkg: String, app: String, ver: String, code: Int, out: String, target: Int): String = nativeExportProject(pkg, app, ver, code, out, target)
    fun lastScriptError(): String = nativeGetLastScriptError()
}
