// ============================================================================
// Anaconda-Engine — build.gradle.kts (root)
//
// Plugin versions are managed centrally in gradle/libs.versions.toml
// (Gradle Version Catalog). Submodules alias plugins from that catalog so a
// single version bump upgrades the whole build.
// ============================================================================

plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.kotlin.android) apply false
    alias(libs.plugins.kotlin.compose) apply false
}

// Clean task aliases so `gradle cleanAll` wipes every module build tree.
tasks.register("cleanAll") {
    group = "build"
    description = "Cleans all subprojects and the native CMake build trees (.cxx)."
    dependsOn(subprojects.map { "${it.path}:clean" })
}
