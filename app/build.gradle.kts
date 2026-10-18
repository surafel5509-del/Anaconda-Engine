// ============================================================================
// Anaconda-Engine — :app (Android Application)
//
// Thin application shell. All product UI is contributed by :editor-ui and all
// engine capability by :engine-core. Keeping the app module thin means the
// same editor UI can later be re-packaged (e.g. a headless "player" APK that
// only embeds :engine-core + a launcher activity for exported games).
// ============================================================================

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.anaconda.engine"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.anaconda.engine"
        minSdk = 24
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
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
    // Editor UI pulls in engine-core transitively; both are declared explicitly
    // so the dependency graph stays obvious to readers and tooling.
    implementation(project(":editor-ui"))
    implementation(project(":engine-core"))

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)

    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.material3)

    testImplementation(libs.junit)
}
