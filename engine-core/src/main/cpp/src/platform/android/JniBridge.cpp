// ============================================================================
//  platform/android/JniBridge.cpp
//
//  The ONLY translation unit that knows JNI exists. Every native entry point
//  is registered in JNI_OnLoad against explicit name/signature strings —
//  if the Kotlin `NativeEngine` external declarations ever drift from these
//  strings, System.loadLibrary() throws at startup instead of crashing later
//  with a NoSuchMethodError mid-frame. (This is why we use RegisterNatives
//  instead of relying on mangled Java_... symbols.)
//
//  Signature cheat-sheet (JNI descriptors):
//    (II)Z                      (Int, Int) -> Boolean
//    (IFFFFFFFFF)V              (Int + 9 Floats) -> Unit
//    ()Ljava/lang/String;       () -> String
//
//  Entity handles cross JNI as opaque Int bit-patterns of the C++ uint32
//  entity (generation|index). C++20 guarantees two's-complement casts.
// ============================================================================
#include <jni.h>

#include <string>

#include "anaconda/core/Engine.hpp"
#include "anaconda/core/Log.hpp"

namespace {

using anaconda::EditorGesture;
using anaconda::Engine;
using anaconda::RunState;
using anaconda::Vec3;
using anaconda::Vec4;

// ---------------------------------------------------------------------------
// jstring helpers
// ---------------------------------------------------------------------------
std::string toStdString(JNIEnv* env, jstring value) {
    if (value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string out(chars);
    env->ReleaseStringUTFChars(value, chars);
    return out;
}

jstring toJString(JNIEnv* env, const std::string& value) {
    return env->NewStringUTF(value.c_str());
}

std::uint32_t toEntity(jint handle) {
    return static_cast<std::uint32_t>(handle);
}

// ---------------------------------------------------------------------------
// Native implementations (mirrors of NativeEngine.kt — keep in sync!)
// ---------------------------------------------------------------------------
jboolean nativeInit(JNIEnv*, jobject, jint width, jint height) {
    Engine& engine = Engine::instance();
    engine.onSurfaceChanged(static_cast<int>(width), static_cast<int>(height));
    ANA_LOGI("[JNI] nativeInit(%d, %d) — engine %s", width, height,
             Engine::version());
    return JNI_TRUE;   // GL resources are created on surfaceCreated instead
}

void nativeSurfaceCreated(JNIEnv*, jobject) {
    Engine::instance().onSurfaceCreated();
}

void nativeSurfaceChanged(JNIEnv*, jobject, jint width, jint height) {
    Engine::instance().onSurfaceChanged(static_cast<int>(width),
                                        static_cast<int>(height));
}

void nativeDrawFrame(JNIEnv*, jobject) {
    Engine::instance().renderFrame();
}

void nativeOnPause(JNIEnv*, jobject) { Engine::instance().onPause(); }
void nativeOnResume(JNIEnv*, jobject) { Engine::instance().onResume(); }
void nativeShutdown(JNIEnv*, jobject) { Engine::instance().shutdown(); }

void nativeSetRunState(JNIEnv*, jobject, jint state) {
    // Clamp unknown values back to Edit — never trap the editor in a state
    // the UI doesn't understand.
    RunState runState = RunState::Edit;
    if (state == 1) runState = RunState::Play;
    else if (state == 2) runState = RunState::Pause;
    Engine::instance().setRunState(runState);
}

jstring nativeGetSceneSnapshot(JNIEnv* env, jobject) {
    return toJString(env, Engine::instance().sceneSnapshot());
}

jint nativeCreateEntity(JNIEnv* env, jobject, jstring name) {
    return static_cast<jint>(Engine::instance().createEntity(
        toStdString(env, name)));
}

void nativeDestroyEntity(JNIEnv*, jobject, jint entity) {
    Engine::instance().destroyEntity(toEntity(entity));
}

void nativeSetEntityName(JNIEnv* env, jobject, jint entity, jstring name) {
    Engine::instance().setEntityName(toEntity(entity), toStdString(env, name));
}

void nativeSetTransform(JNIEnv*, jobject, jint entity,
                        jfloat px, jfloat py, jfloat pz,
                        jfloat rx, jfloat ry, jfloat rz,
                        jfloat sx, jfloat sy, jfloat sz) {
    // Rotation arrives in RADIANS (EditorViewModel converts from the
    // degree-based Inspector fields at this single boundary).
    Engine::instance().setTransform(toEntity(entity), Vec3{px, py, pz},
                                    Vec3{rx, ry, rz}, Vec3{sx, sy, sz});
}

void nativeSetSpriteColor(JNIEnv*, jobject, jint entity, jfloat r, jfloat g,
                          jfloat b, jfloat a) {
    Engine::instance().setSpriteColor(toEntity(entity), Vec4{r, g, b, a});
}

void nativeSetRigidBodyGravityScale(JNIEnv*, jobject, jint entity,
                                    jfloat scale) {
    Engine::instance().setRigidBodyGravityScale(toEntity(entity), scale);
}

void nativeSetRigidBodyUseGravity(JNIEnv*, jobject, jint entity,
                                  jboolean useGravity) {
    Engine::instance().setRigidBodyUseGravity(toEntity(entity),
                                              useGravity == JNI_TRUE);
}

void nativeEditorGesture(JNIEnv*, jobject, jint type, jfloat dx, jfloat dy,
                         jfloat x, jfloat y) {
    EditorGesture gesture = EditorGesture::Pan;
    if (type == 1) gesture = EditorGesture::Zoom;
    else if (type == 2) gesture = EditorGesture::Tap;
    Engine::instance().handleGesture(gesture, dx, dy, x, y);
}

jfloat nativeGetFps(JNIEnv*, jobject) {
    return static_cast<jfloat>(Engine::instance().fps());
}

jint nativeGetEntityCount(JNIEnv*, jobject) {
    return static_cast<jint>(Engine::instance().entityCount());
}

jstring nativeGetEngineVersion(JNIEnv* env, jobject) {
    return toJString(env, Engine::version());
}

// ---------------------------------------------------------------------------
// Registration table — MUST match NativeEngine.kt exactly.
// ---------------------------------------------------------------------------
const JNINativeMethod kNativeEngineMethods[] = {
    {"nativeInit", "(II)Z",
     reinterpret_cast<void*>(nativeInit)},
    {"nativeSurfaceCreated", "()V",
     reinterpret_cast<void*>(nativeSurfaceCreated)},
    {"nativeSurfaceChanged", "(II)V",
     reinterpret_cast<void*>(nativeSurfaceChanged)},
    {"nativeDrawFrame", "()V",
     reinterpret_cast<void*>(nativeDrawFrame)},
    {"nativeOnPause", "()V",
     reinterpret_cast<void*>(nativeOnPause)},
    {"nativeOnResume", "()V",
     reinterpret_cast<void*>(nativeOnResume)},
    {"nativeShutdown", "()V",
     reinterpret_cast<void*>(nativeShutdown)},
    {"nativeSetRunState", "(I)V",
     reinterpret_cast<void*>(nativeSetRunState)},
    {"nativeGetSceneSnapshot", "()Ljava/lang/String;",
     reinterpret_cast<void*>(nativeGetSceneSnapshot)},
    {"nativeCreateEntity", "(Ljava/lang/String;)I",
     reinterpret_cast<void*>(nativeCreateEntity)},
    {"nativeDestroyEntity", "(I)V",
     reinterpret_cast<void*>(nativeDestroyEntity)},
    {"nativeSetEntityName", "(ILjava/lang/String;)V",
     reinterpret_cast<void*>(nativeSetEntityName)},
    {"nativeSetTransform", "(IFFFFFFFFF)V",
     reinterpret_cast<void*>(nativeSetTransform)},
    {"nativeSetSpriteColor", "(IFFFF)V",
     reinterpret_cast<void*>(nativeSetSpriteColor)},
    {"nativeSetRigidBodyGravityScale", "(IF)V",
     reinterpret_cast<void*>(nativeSetRigidBodyGravityScale)},
    {"nativeSetRigidBodyUseGravity", "(IZ)V",
     reinterpret_cast<void*>(nativeSetRigidBodyUseGravity)},
    {"nativeEditorGesture", "(IFFFF)V",
     reinterpret_cast<void*>(nativeEditorGesture)},
    {"nativeGetFps", "()F",
     reinterpret_cast<void*>(nativeGetFps)},
    {"nativeGetEntityCount", "()I",
     reinterpret_cast<void*>(nativeGetEntityCount)},
    {"nativeGetEngineVersion", "()Ljava/lang/String;",
     reinterpret_cast<void*>(nativeGetEngineVersion)},
};

constexpr int kMethodCount =
    static_cast<int>(sizeof(kNativeEngineMethods) / sizeof(kNativeEngineMethods[0]));

}  // namespace

// ---------------------------------------------------------------------------
// JNI_OnLoad — called by System.loadLibrary("anaconda_engine").
// A failed registration throws NoSuchMethodError on the Kotlin side, which
// fails loudly in CI instrumentation tests instead of segfaulting later.
// ---------------------------------------------------------------------------
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        ANA_LOGE("[JNI] GetEnv failed in JNI_OnLoad");
        return JNI_ERR;
    }

    jclass clazz = env->FindClass("com/anaconda/engine/core/NativeEngine");
    if (clazz == nullptr) {
        ANA_LOGE("[JNI] FindClass('NativeEngine') failed — check the Kotlin "
                 "package com.anaconda.engine.core");
        return JNI_ERR;
    }

    if (env->RegisterNatives(clazz, kNativeEngineMethods, kMethodCount) != 0) {
        ANA_LOGE("[JNI] RegisterNatives failed — NativeEngine.kt and "
                 "JniBridge.cpp signatures have drifted");
        return JNI_ERR;   // propagates as UnsatisfiedLinkError to Kotlin
    }

    env->DeleteLocalRef(clazz);
    ANA_LOGI("[JNI] NativeEngine bound (%d methods), engine %s ready", kMethodCount,
             Engine::version());
    return JNI_VERSION_1_6;
}
