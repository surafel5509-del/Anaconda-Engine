// ============================================================================
//  anaconda/core/Log.hpp
//
//  Tiny logging façade for the engine.
//
//  On Android every message goes to logcat under the single tag "Anaconda"
//  (readable with: adb logcat -s Anaconda). On any other host (unit tests,
//  desktop tooling) it falls back to stdout so the SAME engine code builds
//  and is debuggable off-device.
//
//  Usage:
//      ANA_LOGI("renderer initialized in %.2f ms", ms);
//      ANA_LOGE("shader compile failed: %s", infoLog);
//      ANA_ASSERT(world_.valid(e), "stale entity passed to system");
// ============================================================================
#pragma once

#if defined(__ANDROID__)
#include <android/log.h>

#define ANA_LOG_TAG "Anaconda"
#define ANA_LOGI(...) __android_log_print(ANDROID_LOG_INFO, ANA_LOG_TAG, __VA_ARGS__)
#define ANA_LOGW(...) __android_log_print(ANDROID_LOG_WARN, ANA_LOG_TAG, __VA_ARGS__)
#define ANA_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, ANA_LOG_TAG, __VA_ARGS__)
#define ANA_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, ANA_LOG_TAG, __VA_ARGS__)

#else  // Desktop / CI host build -------------------------------------------
#include <cstdio>

#define ANA_LOGI(...) (std::printf("[I][Anaconda] " __VA_ARGS__), std::printf("\n"))
#define ANA_LOGW(...) (std::printf("[W][Anaconda] " __VA_ARGS__), std::printf("\n"))
#define ANA_LOGE(...) (std::fprintf(stderr, "[E][Anaconda] " __VA_ARGS__), std::fprintf(stderr, "\n"))
#define ANA_LOGD(...) ((void)0)  // debug noise compiled away on host
#endif

// ---------------------------------------------------------------------------
// Always-on engine invariant check (not stripped by NDEBUG): violations are
// programmer errors (stale entity, missing component) that must never ship
// silently. Aborts after logging so CI and tests fail loudly.
// ---------------------------------------------------------------------------
#include <cstdlib>

#define ANA_ASSERT(cond, ...)                                        \
    do {                                                             \
        if (!(cond)) {                                               \
            ANA_LOGE("ASSERT(%s) failed @ %s:%d — ", #cond, __FILE__, \
                     __LINE__);                                      \
            ANA_LOGE(__VA_ARGS__);                                   \
            std::abort();                                            \
        }                                                            \
    } while (0)
