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
// Month 2 — persistence + asset pipeline.
// Scene save/load return "" (empty) on SUCCESS or an error message on
// FAILURE — one atomic crossing, no last-error race between threads.
// ---------------------------------------------------------------------------
jstring nativeSaveScene(JNIEnv* env, jobject, jstring path) {
    std::string error;
    const bool ok = Engine::instance().saveScene(toStdString(env, path), &error);
    return toJString(env, ok ? std::string{} : error);
}

jstring nativeLoadScene(JNIEnv* env, jobject, jstring path) {
    std::string error;
    const bool ok = Engine::instance().loadScene(toStdString(env, path), &error);
    return toJString(env, ok ? std::string{} : error);
}

void nativeNewScene(JNIEnv*, jobject) { Engine::instance().newScene(); }

jint nativeImportTexture(JNIEnv* env, jobject, jstring path) {
    return static_cast<jint>(Engine::instance().importTexture(
        toStdString(env, path)));
}

void nativeSetSpriteTexture(JNIEnv*, jobject, jint entity, jint textureHandle) {
    Engine::instance().setSpriteTexture(toEntity(entity),
                                        static_cast<std::uint32_t>(textureHandle));
}

jint nativeRestoreEntity(JNIEnv* env, jobject, jstring line) {
    std::string error;
    const int id = Engine::instance().restoreEntity(toStdString(env, line),
                                                    &error);
    if (id < 0) {
        ANA_LOGE("[JNI] restoreEntity failed: %s", error.c_str());
    }
    return id;
}

jint nativeTextureHandleForPath(JNIEnv* env, jobject, jstring path) {
    return static_cast<jint>(
        Engine::instance().textureHandleForPath(toStdString(env, path)));
}

// Month 3 — sorting / duplicate / picking
jint nativeDuplicateEntity(JNIEnv*, jobject, jint entity) {
    return static_cast<jint>(Engine::instance().duplicateEntity(toEntity(entity)));
}

void nativeSetSpriteSortingOrder(JNIEnv*, jobject, jint entity, jint layer) {
    Engine::instance().setSpriteSortingOrder(toEntity(entity), static_cast<int>(layer));
}

jint nativePickEntity(JNIEnv*, jobject, jfloat x, jfloat y) {
    return static_cast<jint>(Engine::instance().pickEntity(x, y));
}

jint nativeGetDrawCalls(JNIEnv*, jobject) {
    return Engine::instance().frameStats().drawCalls;
}

jint nativeGetSpriteCount(JNIEnv*, jobject) {
    return Engine::instance().frameStats().sprites;
}

// Month 3-5 — grid/gizmo/prefab/anim/script/camera/export
void nativeSetGridEnabled(JNIEnv*, jobject, jboolean v){ Engine::instance().setGridEnabled(v==JNI_TRUE); }
void nativeSetGridSnap(JNIEnv*, jobject, jboolean v){ Engine::instance().setGridSnapEnabled(v==JNI_TRUE); }
void nativeSetGridCellSize(JNIEnv*, jobject, jfloat s){ Engine::instance().setGridCellSize(s); }
void nativeSetGizmoMode(JNIEnv*, jobject, jint m){ Engine::instance().setGizmoMode(static_cast<GizmoMode>(m)); }
jboolean nativeGizmoBeginDrag(JNIEnv*, jobject, jfloat x, jfloat y){ return Engine::instance().gizmoBeginDrag(x,y)?JNI_TRUE:JNI_FALSE; }
void nativeGizmoDrag(JNIEnv*, jobject, jfloat x, jfloat y){ Engine::instance().gizmoDrag(x,y); }
void nativeGizmoEndDrag(JNIEnv*, jobject){ Engine::instance().gizmoEndDrag(); }
void nativeFrameSelected(JNIEnv*, jobject, jint e){ Engine::instance().frameSelected(toEntity(e)); }
void nativeSetPhysicsMaterial(JNIEnv*, jobject, jint e, jfloat f, jfloat r){ Engine::instance().setPhysicsMaterial(toEntity(e),f,r); }
void nativeSetColliderDebugDraw(JNIEnv*, jobject, jboolean v){ Engine::instance().setColliderDebugDraw(v==JNI_TRUE); }
void nativeSetFixedTimestep(JNIEnv*, jobject, jboolean v){ Engine::instance().setFixedTimestepEnabled(v==JNI_TRUE); }
jstring nativeSavePrefab(JNIEnv* env, jobject, jint e, jstring path){ std::string err; bool ok=Engine::instance().savePrefab(toEntity(e), toStdString(env,path), &err); return toJString(env, ok?"":err); }
jint nativeInstantiatePrefab(JNIEnv* env, jobject, jstring path, jfloat ox, jfloat oy, jfloat oz){ std::string err; int id=Engine::instance().instantiatePrefab(toStdString(env,path), Vec3{ox,oy,oz}, &err); if(id<0) ANA_LOGE("[JNI] instantiatePrefab failed: %s", err.c_str()); return id; }
jstring nativeCreateAnimationClip(JNIEnv* env, jobject, jstring name, jintArray frames, jfloat fps, jboolean loop){ jsize len=env->GetArrayLength(frames); std::vector<std::uint32_t> f(len); jint* p=env->GetIntArrayElements(frames,nullptr); for(int i=0;i<len;++i) f[i]=static_cast<std::uint32_t>(p[i]); env->ReleaseIntArrayElements(frames,p,0); std::string err; bool ok=Engine::instance().createAnimationClip(toStdString(env,name), f, fps, loop==JNI_TRUE, &err); return toJString(env, ok?"":err); }
void nativeSetAnimatorClip(JNIEnv* env, jobject, jint e, jstring clip){ Engine::instance().setAnimatorClip(toEntity(e), toStdString(env,clip)); }
void nativeSetAnimatorPlaying(JNIEnv*, jobject, jint e, jboolean p){ Engine::instance().setAnimatorPlaying(toEntity(e), p==JNI_TRUE); }
void nativeSetAnimatorSpeed(JNIEnv*, jobject, jint e, jfloat s){ Engine::instance().setAnimatorSpeed(toEntity(e), s); }
jstring nativeLoadScript(JNIEnv* env, jobject, jstring path, jstring src){ std::string err; bool ok=Engine::instance().loadScript(toStdString(env,path), toStdString(env,src), &err); return toJString(env, ok?"":err); }
jstring nativeBindScript(JNIEnv* env, jobject, jint e, jstring path){ std::string err; bool ok=Engine::instance().bindScriptToEntity(toEntity(e), toStdString(env,path), &err); return toJString(env, ok?"":err); }
jstring nativeHotReloadScript(JNIEnv* env, jobject, jint e, jstring src){ std::string err; bool ok=Engine::instance().hotReloadScript(toEntity(e), toStdString(env,src), &err); return toJString(env, ok?"":err); }
void nativeCameraShake(JNIEnv*, jobject, jfloat a, jfloat d){ Engine::instance().cameraShake(a,d); }
void nativeCameraZoomTo(JNIEnv*, jobject, jfloat target, jfloat dur){ Engine::instance().cameraZoomTo(target,dur); }
jstring nativeExportProject(JNIEnv* env, jobject, jstring pkg, jstring name, jstring ver, jint code, jstring outPath, jint target){ anaconda::export_cfg::ExportConfig cfg; cfg.packageName=toStdString(env,pkg); cfg.appName=toStdString(env,name); cfg.versionName=toStdString(env,ver); cfg.versionCode=code; cfg.target=static_cast<anaconda::export_cfg::ExportTarget>(target); auto r=Engine::instance().exportProject(cfg); return toJString(env, r.success?r.outputPath:r.error); }
jstring nativeGetLastScriptError(JNIEnv* env, jobject){ return toJString(env, Engine::instance().lastScriptError()); }

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
    {"nativeSaveScene", "(Ljava/lang/String;)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeSaveScene)},
    {"nativeLoadScene", "(Ljava/lang/String;)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeLoadScene)},
    {"nativeNewScene", "()V",
     reinterpret_cast<void*>(nativeNewScene)},
    {"nativeImportTexture", "(Ljava/lang/String;)I",
     reinterpret_cast<void*>(nativeImportTexture)},
    {"nativeSetSpriteTexture", "(II)V",
     reinterpret_cast<void*>(nativeSetSpriteTexture)},
    {"nativeRestoreEntity", "(Ljava/lang/String;)I",
     reinterpret_cast<void*>(nativeRestoreEntity)},
    {"nativeTextureHandleForPath", "(Ljava/lang/String;)I",
     reinterpret_cast<void*>(nativeTextureHandleForPath)},
    {"nativeDuplicateEntity", "(I)I",
     reinterpret_cast<void*>(nativeDuplicateEntity)},
    {"nativeSetSpriteSortingOrder", "(II)V",
     reinterpret_cast<void*>(nativeSetSpriteSortingOrder)},
    {"nativePickEntity", "(FF)I",
     reinterpret_cast<void*>(nativePickEntity)},
    {"nativeGetDrawCalls", "()I",
     reinterpret_cast<void*>(nativeGetDrawCalls)},
    {"nativeGetSpriteCount", "()I",
     reinterpret_cast<void*>(nativeGetSpriteCount)},
    {"nativeSetGridEnabled", "(Z)V",
     reinterpret_cast<void*>(nativeSetGridEnabled)},
    {"nativeSetGridSnap", "(Z)V",
     reinterpret_cast<void*>(nativeSetGridSnap)},
    {"nativeSetGridCellSize", "(F)V",
     reinterpret_cast<void*>(nativeSetGridCellSize)},
    {"nativeSetGizmoMode", "(I)V",
     reinterpret_cast<void*>(nativeSetGizmoMode)},
    {"nativeGizmoBeginDrag", "(FF)Z",
     reinterpret_cast<void*>(nativeGizmoBeginDrag)},
    {"nativeGizmoDrag", "(FF)V",
     reinterpret_cast<void*>(nativeGizmoDrag)},
    {"nativeGizmoEndDrag", "()V",
     reinterpret_cast<void*>(nativeGizmoEndDrag)},
    {"nativeFrameSelected", "(I)V",
     reinterpret_cast<void*>(nativeFrameSelected)},
    {"nativeSetPhysicsMaterial", "(IFF)V",
     reinterpret_cast<void*>(nativeSetPhysicsMaterial)},
    {"nativeSetColliderDebugDraw", "(Z)V",
     reinterpret_cast<void*>(nativeSetColliderDebugDraw)},
    {"nativeSetFixedTimestep", "(Z)V",
     reinterpret_cast<void*>(nativeSetFixedTimestep)},
    {"nativeSavePrefab", "(ILjava/lang/String;)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeSavePrefab)},
    {"nativeInstantiatePrefab", "(Ljava/lang/String;FFF)I",
     reinterpret_cast<void*>(nativeInstantiatePrefab)},
    {"nativeCreateAnimationClip", "(Ljava/lang/String;[IFZ)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeCreateAnimationClip)},
    {"nativeSetAnimatorClip", "(ILjava/lang/String;)V",
     reinterpret_cast<void*>(nativeSetAnimatorClip)},
    {"nativeSetAnimatorPlaying", "(IZ)V",
     reinterpret_cast<void*>(nativeSetAnimatorPlaying)},
    {"nativeSetAnimatorSpeed", "(IF)V",
     reinterpret_cast<void*>(nativeSetAnimatorSpeed)},
    {"nativeLoadScript", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeLoadScript)},
    {"nativeBindScript", "(ILjava/lang/String;)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeBindScript)},
    {"nativeHotReloadScript", "(ILjava/lang/String;)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeHotReloadScript)},
    {"nativeCameraShake", "(FF)V",
     reinterpret_cast<void*>(nativeCameraShake)},
    {"nativeCameraZoomTo", "(FF)V",
     reinterpret_cast<void*>(nativeCameraZoomTo)},
    {"nativeExportProject", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;I)Ljava/lang/String;",
     reinterpret_cast<void*>(nativeExportProject)},
    {"nativeGetLastScriptError", "()Ljava/lang/String;",
     reinterpret_cast<void*>(nativeGetLastScriptError)},
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
