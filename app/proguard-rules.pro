# ============================================================================
# Anaconda-Engine — app/proguard-rules.pro (R8 minification, release builds)
# ============================================================================

# --- JNI safety -------------------------------------------------------------
# The native side registers methods via JNI_OnLoad/FindClass against the
# EXACT class + method names. If R8 renames/removes them, System.loadLibrary
# fails with UnsatisfiedLinkError. Keep the whole bridge surface.
-keep class com.anaconda.engine.core.NativeEngine { *; }
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# Snapshot model is parsed by field order/name in tests — keep data classes.
-keep class com.anaconda.engine.core.EntitySnapshot { *; }
-keep class com.anaconda.engine.core.Vec3F { *; }
-keep class com.anaconda.engine.core.Vec4F { *; }

# --- Kotlin metadata (reflection-free, but keeps debuggability) -------------
-dontwarn kotlin.**
-keepattributes SourceFile,LineNumberTable,Signature,*Annotation*

# --- Compose / AndroidX (default optimize file already handles most) --------
-dontwarn androidx.compose.**
