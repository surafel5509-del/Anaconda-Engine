// ============================================================================
// Anaconda-Engine — settings.gradle.kts
//
// Declares the multi-module project layout:
//   :app         — Android application shell (launcher Activity, manifest, res)
//   :editor-ui   — Jetpack Compose editor IDE (panels, viewport, theme)
//   :engine-core — Native C++ engine (NDK/CMake) + thin Kotlin JNI facade
//
// Module dependency graph:
//   :app  ──►  :editor-ui  ──►  :engine-core  ──►  libanaconda_engine.so (NDK)
// ============================================================================

pluginManagement {
    repositories {
        google {
            content {
                includeGroupByRegex("com\\.android.*")
                includeGroupByRegex("com\\.google.*")
                includeGroupByRegex("androidx.*")
            }
        }
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "Anaconda-Engine"

include(":app")
include(":editor-ui")
include(":engine-core")
