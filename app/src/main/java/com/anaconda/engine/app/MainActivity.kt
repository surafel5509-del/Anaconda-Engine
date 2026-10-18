// ============================================================================
//  MainActivity.kt — the app shell.
//
//  Deliberately minimal: edge-to-edge window + EditorScreen. Everything the
//  user sees lives in :editor-ui so the same UI can be embedded later (e.g.
//  a "project picker" activity on top, Month 2) without touching engine code.
// ============================================================================
package com.anaconda.engine.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import com.anaconda.editor.EditorScreen
import com.anaconda.editor.theme.EditorTheme

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)

        setContent {
            EditorTheme {
                // The full IDE — toolbar, docks, GL viewport.
                // Surface-ready callbacks trigger the first native scene
                // snapshot so the hierarchy populates as soon as GL is live.
                EditorScreen()
            }
        }
    }
}
