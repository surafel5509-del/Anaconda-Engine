// ============================================================================
//  EditorTheme.kt — dark professional IDE palette for Anaconda-Engine.
//
//  Modeled on desktop creative tools (Blender/VS Code dark): near-black
//  canvas, slightly raised panel surfaces, high-contrast text, a single
//  accent (anaconda green) + selection blue. High contrast ratios are kept
//  above WCAG AA for onSurface vs surface.
// ============================================================================
package com.anaconda.editor.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// ---- palette ---------------------------------------------------------------
val AnacondaBackground = Color(0xFF0B0E13)
val AnacondaSurface = Color(0xFF141922)
val AnacondaSurfaceHigh = Color(0xFF1B2230)
val AnacondaOutline = Color(0xFF2B3446)
val AnacondaPrimary = Color(0xFF3DD39C)      // brand green
val AnacondaSecondary = Color(0xFF5B8CFF)    // selection / links
val AnacondaError = Color(0xFFE5544B)
val AnacondaOnSurface = Color(0xFFDCE3EE)
val AnacondaOnSurfaceDim = Color(0xFF8B96A8)
val AnacondaWarning = Color(0xFFF2B441)      // pause state
val AnacondaViewportBg = Color(0xFF0E1116)

private val AnacondaDarkScheme = darkColorScheme(
    primary = AnacondaPrimary,
    onPrimary = Color(0xFF05130D),
    primaryContainer = Color(0xFF124735),
    onPrimaryContainer = Color(0xFFA9F5D5),
    secondary = AnacondaSecondary,
    onSecondary = Color(0xFF041027),
    secondaryContainer = Color(0xFF1D3566),
    onSecondaryContainer = Color(0xFFCADCFF),
    tertiary = AnacondaWarning,
    error = AnacondaError,
    background = AnacondaBackground,
    onBackground = AnacondaOnSurface,
    surface = AnacondaSurface,
    onSurface = AnacondaOnSurface,
    surfaceVariant = AnacondaSurfaceHigh,
    onSurfaceVariant = AnacondaOnSurfaceDim,
    outline = AnacondaOutline,
    outlineVariant = AnacondaOutline,
    surfaceTint = AnacondaPrimary,
)

private val AnacondaTypography = Typography()   // Material3 defaults, tuned by sizes in composables

/**
 * Anaconda editor theme. Always dark for Phase 1 — editors in dark mode are
 * the default expectation for game tooling (light theme is a Month-6 item).
 */
@Composable
fun EditorTheme(
    @Suppress("UNUSED_PARAMETER") darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit,
) {
    MaterialTheme(
        colorScheme = AnacondaDarkScheme,
        typography = AnacondaTypography,
        content = content,
    )
}
