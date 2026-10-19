// ============================================================================
// Anaconda-Engine — :engine-core (Android Library)
//
// This module is the beating heart of the product:
//   * Builds the native shared library `libanaconda_engine.so` with the NDK
//     through CMake (src/main/cpp/CMakeLists.txt).
//   * Exposes a minimal, thread-safe Kotlin facade (NativeEngine) that the
//     editor UI talks to. The Kotlin layer is intentionally thin: it performs
//     NO engine logic, it only marshals values across JNI.
//
// Native targets produced by CMake:
//   anaconda_math    (INTERFACE) — vectors, matrices, transforms
//   anaconda_ecs     (INTERFACE) — sparse-set Entity Component System
//   anaconda_sim     (STATIC)    — physics world (gravity + AABB collisions)
//   anaconda_renderer(STATIC)    — OpenGL ES 3.0 pipeline (Android only)
//   anaconda_engine  (SHARED)    — Engine façade + JNI bridge (Android only)
// ============================================================================

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "com.anaconda.engine.core"
    compileSdk = 35

    // Must match the NDK installed by the SDK manager (see docs/ARCHITECTURE.md).
    ndkVersion = "26.3.11579264"

    defaultConfig {
        minSdk = 24                       // GLES 3.0 is API 18+, 24 is our practical floor
        consumerProguardFiles("consumer-rules.pro")

        externalNativeBuild {
            cmake {
                // Warnings on, C++20 (designated initializers, concepts-ready).
                // Exceptions/RTTI stay ENABLED (default): the ECS uses
                // std::type_index and the editor API reports errors safely.
                cppFlags += listOf("-std=c++20", "-Wall", "-Wextra")
                // c++_shared: one libc++_shared.so for all our native code.
                arguments += listOf("-DANDROID_STL=c++_shared")
            }
        }

        ndk {
            // Phase-1 ABI set. x86_64 can be added for emulator testing.
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    // Ship only the compressed-native variant decision is app-level; keep
    // uncompressed .so for faster mmap/dlopen on modern Android (AGP default).
    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)

    // Host-side unit tests (SceneSnapshotParserTest and friends) run on the
    // JVM via `:engine-core:testDebugUnitTest` in CI.
    testImplementation(libs.junit)
}
