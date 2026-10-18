# ============================================================================
# Anaconda-Engine — engine-core/consumer-rules.pro
#
# "Consumer" rules travel with the AAR: any app that depends on :engine-core
# automatically gets these keeps. They protect the JNI bridge even if a
# downstream consumer forgets to configure R8 themselves.
# ============================================================================

-keep class com.anaconda.engine.core.NativeEngine { *; }
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}
