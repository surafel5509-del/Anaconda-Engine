// ============================================================================
//  GameViewport.kt — the native GLES 3.0 surface inside the Compose editor.
//
//  Responsibilities:
//    * Host a GLSurfaceView (EGL context v3, RGBA8888+depth16) via AndroidView
//    * Forward the three GL callbacks into NativeEngine (GL thread only)
//    * Convert raw touch input into SEMANTIC gestures (pan / pinch-zoom / tap)
//      and ship them to the engine, where camera state lives — the UI never
//      does world-space math itself
//    * Forward lifecycle pause/resume so the GL thread stops cleanly when the
//      app backgrounds (preserveEGLContextOnPause keeps shaders warm)
//
//  THREADING: setRenderer callbacks arrive on the GL thread; everything that
//  touches Compose state is posted back to the main thread.
// ============================================================================
package com.anaconda.editor.viewport

import android.annotation.SuppressLint
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Handler
import android.os.Looper
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.View
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.anaconda.engine.core.NativeEngine
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/**
 * @param onSurfaceReady invoked on the MAIN thread the first time the GL
 *        surface exists (the ViewModel uses it to pull the first snapshot —
 *        the native demo scene is only built once the GL context is live).
 */
@Composable
fun GameViewport(
    onSurfaceReady: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val lifecycleOwner = LocalLifecycleOwner.current

    // State-backed so ANY composition pass sees the same reference (the GL
    // surface is created once, but parent recompositions happen every FPS tick).
    var glView by remember { mutableStateOf<GLSurfaceView?>(null) }
    var surfaceAnnounced by remember { mutableStateOf(false) }

    AndroidView(
        modifier = modifier,
        factory = { ctx -> createSurfaceView(ctx, onReady = {
            if (!surfaceAnnounced) {
                surfaceAnnounced = true
                onSurfaceReady()
            }
        }).also { glView = it } },
    )

    // Pause/resume the GL thread with the Activity/Fragment lifecycle.
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_PAUSE -> {
                    glView?.onPause()
                    NativeEngine.nativeOnPause()
                }
                Lifecycle.Event.ON_RESUME -> {
                    NativeEngine.nativeOnResume()
                    glView?.onResume()
                }
                else -> Unit
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose {
            lifecycleOwner.lifecycle.removeObserver(observer)
            glView?.onPause()
            glView = null
        }
    }
}

// ---------------------------------------------------------------------------
// Surface construction (main thread)
// ---------------------------------------------------------------------------
private fun createSurfaceView(
    context: Context,
    onReady: () -> Unit,
): GLSurfaceView {
    val mainHandler = Handler(Looper.getMainLooper())

    // Gesture plumbing lives outside the GL renderer — the engine only ever
    // sees semantic events.
    var lastPanX = 0f
    var lastPanY = 0f
    var downX = 0f
    var downY = 0f
    var downAt = 0L
    var movedBeyondTap = true

    val scaleDetector = ScaleGestureDetector(
        context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                NativeEngine.nativeEditorGesture(
                    NativeEngine.GESTURE_ZOOM,
                    detector.scaleFactor,
                    0f,
                    detector.focusX,
                    detector.focusY,
                )
                return true
            }
        },
    )

    @SuppressLint("ClickableViewAccessibility")
    val touchListener = View.OnTouchListener { _, event ->
        scaleDetector.onTouchEvent(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                lastPanX = event.x
                lastPanY = event.y
                downX = event.x
                downY = event.y
                downAt = System.currentTimeMillis()
                movedBeyondTap = false
                true
            }

            MotionEvent.ACTION_MOVE -> {
                if (!scaleDetector.isInProgress && event.pointerCount == 1) {
                    val dx = event.x - lastPanX
                    val dy = event.y - lastPanY
                    if (dx * dx + dy * dy > 4f) {   // 2 px dead-zone
                        NativeEngine.nativeEditorGesture(
                            NativeEngine.GESTURE_PAN, dx, dy, event.x, event.y,
                        )
                        lastPanX = event.x
                        lastPanY = event.y
                        movedBeyondTap = true
                    }
                } else {
                    movedBeyondTap = true
                }
                true
            }

            MotionEvent.ACTION_UP -> {
                if (!movedBeyondTap &&
                    System.currentTimeMillis() - downAt < 250
                ) {
                    // Tap → future Phase-3 ray picking.
                    NativeEngine.nativeEditorGesture(
                        NativeEngine.GESTURE_TAP, 0f, 0f, downX, downY,
                    )
                }
                true
            }

            else -> true
        }
    }

    return GLSurfaceView(context).apply {
        // Keep the EGL context alive across pause → faster resume, no shader
        // recompilation on quick app switches.
        preserveEGLContextOnPause = true

        // OpenGL ES 3.0 (matches glEsVersion in the manifest + CMake target).
        setEGLContextClientVersion(3)
        setEGLConfigChooser(8, 8, 8, 8, 16, 0)   // RGBA8888 + 16-bit depth

        setRenderer(object : GLSurfaceView.Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                NativeEngine.nativeSurfaceCreated()
                // Announce on the main thread so Compose state stays safe.
                mainHandler.post(onReady)
            }

            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                NativeEngine.nativeSurfaceChanged(width, height)
            }

            override fun onDrawFrame(gl: GL10?) {
                NativeEngine.nativeDrawFrame()
            }
        })

        // Continuous: the editor viewport animates (Play mode physics) and the
        // scene is dirty-driven only later (Phase 3 could switch to RENDERMODE
        // WHEN_DIRTY + wakeups for battery savings in Edit mode).
        renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY

        setOnTouchListener(touchListener)
    }
}
