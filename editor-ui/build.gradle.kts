// ============================================================================
// Anaconda-Engine — :editor-ui (Android Library, Jetpack Compose)
//
// The entire IDE/Editor UI lives here:
//   * EditorScreen       — root layout (toolbar / hierarchy / viewport /
//                          inspector / asset browser, responsive panels)
//   * EditorViewModel    — single source of truth for editor state
//   * viewport/          — GLSurfaceView interop that hosts the native renderer
//   * panels/            — SceneHierarchy, Inspector, AssetBrowser, Toolbar
//   * theme/             — dark professional IDE theme
//
// Depends on :engine-core (JNI facade) — the native .so propagates through
// this module to :app automatically.
// ============================================================================

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.anaconda.editor"
    compileSdk = 35

    defaultConfig {
        minSdk = 24
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        compose = true
    }
}

dependencies {
    // Native engine facade (JNI entry points + scene snapshot model).
    implementation(project(":engine-core"))

    // Compose — versions aligned by the BOM.
    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.graphics)
    implementation(libs.compose.ui.tooling.preview)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons.extended)

    // Lifecycle: ViewModel + per-frame state collection.
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.viewmodel.ktx)

    debugImplementation(libs.compose.ui.tooling)
}
