# ============================================================================
#  Anaconda-Engine — CMakeLists.txt  (engine-core/src/main/cpp)
#
#  Target graph:
#
#      anaconda_math      (INTERFACE)  header-only math
#      anaconda_ecs       (INTERFACE)  header-only sparse-set ECS
#      anaconda_sim       (STATIC)     physics — builds on ANY host
#             ▲
#             │   ┌── only when building for Android (NDK) ──┐
#      anaconda_renderer  (STATIC)     GLES3 pipeline        │
#             ▲                                              │
#      anaconda_engine    (SHARED)     Engine + JNI bridge   │  → libanaconda_engine.so
#
#      tests (option ANACONDA_BUILD_TESTS=ON, host): test_math/test_ecs/
#      test_physics/test_scene/test_image — link only host-safe libs
#      (sim/scene/assets) → no GL, no Android, runs on CI via ctest.
#
#  Layering is enforced by the link graph: renderer code cannot see physics
#  internals without a reviewer changing this file.
# ============================================================================
cmake_minimum_required(VERSION 3.22)

project(AnacondaEngine
        VERSION 0.1.0
        DESCRIPTION "Anaconda-Engine native core for Android"
        LANGUAGES C CXX)

option(ANACONDA_BUILD_TESTS
       "Build host-side unit tests (math/ECS/physics/scene/image)" OFF)

# ---- global C++ policy ------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Position-independent code everywhere (static libs land inside a .so).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---- warning façade ---------------------------------------------------------
# Consumed by every target via anaconda_warnings — keeps flags in one place.
add_library(anaconda_warnings INTERFACE)
if(MSVC)
    target_compile_options(anaconda_warnings INTERFACE /W4)
else()
    target_compile_options(anaconda_warnings INTERFACE -Wall -Wextra)
endif()

# ---- anaconda_math (header-only) -------------------------------------------
add_library(anaconda_math INTERFACE)
target_include_directories(anaconda_math INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaconda_math INTERFACE anaconda_warnings)

# ---- anaconda_ecs (header-only) --------------------------------------------
add_library(anaconda_ecs INTERFACE)
target_include_directories(anaconda_ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaconda_ecs INTERFACE anaconda_math anaconda_warnings)

# ---- anaconda_sim (physics — host + Android) -------------------------------
add_library(anaconda_sim STATIC
    src/physics/PhysicsSystem.cpp
)
target_include_directories(anaconda_sim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaconda_sim PUBLIC anaconda_math anaconda_ecs anaconda_warnings)

# ---- anaconda_scene (binary scene serializer — host + Android) -------------
add_library(anaconda_scene STATIC
    src/scene/SceneSerializer.cpp
    src/scene/EditorSnapshotCodec.cpp
    src/scene/Prefab.cpp
)
target_include_directories(anaconda_scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaconda_scene PUBLIC anaconda_ecs anaconda_warnings)

# ---- anaconda_assets (image decoding via vendored stb_image — host+Android)-
add_library(anaconda_assets STATIC
    src/assets/ImageDecoder.cpp
)
# SYSTEM → third-party headers compile warning-free under -Wall -Wextra.
target_include_directories(anaconda_assets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(anaconda_assets SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb   # stb_image.h
)
target_link_libraries(anaconda_assets PUBLIC anaconda_warnings)

# ---- lua (vendored 5.4.6 — host + Android) ----------------------------------
# io/os/loadlib stripped for sandbox + Android portability (no popen/dlopen).
add_library(lua_static STATIC
    third_party/lua/lapi.c
    third_party/lua/lauxlib.c
    third_party/lua/lbaselib.c
    third_party/lua/lcode.c
    third_party/lua/lcorolib.c
    third_party/lua/lctype.c
    third_party/lua/ldblib.c
    third_party/lua/ldebug.c
    third_party/lua/ldo.c
    third_party/lua/ldump.c
    third_party/lua/lfunc.c
    third_party/lua/lgc.c
    third_party/lua/linit.c
    third_party/lua/llex.c
    third_party/lua/lmathlib.c
    third_party/lua/lmem.c
    third_party/lua/lobject.c
    third_party/lua/lopcodes.c
    third_party/lua/lparser.c
    third_party/lua/lstate.c
    third_party/lua/lstring.c
    third_party/lua/lstrlib.c
    third_party/lua/ltable.c
    third_party/lua/ltablib.c
    third_party/lua/ltm.c
    third_party/lua/lundump.c
    third_party/lua/lutf8lib.c
    third_party/lua/lvm.c
    third_party/lua/lzio.c
)
target_include_directories(lua_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua)
target_include_directories(lua_static SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua)
target_compile_options(lua_static PRIVATE -w -Wno-error -Wno-everything -Wno-unknown-pragmas -Wno-unused-parameter)
# No LUA_USE_LINUX on Android (Bionic); generic POSIX is safe for sandbox.

# ---- anaconda_script (Lua sandbox — host + Android) ------------------------
add_library(anaconda_script STATIC
    src/script/ScriptEngine.cpp
)
target_include_directories(anaconda_script PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(anaconda_script SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua)
target_link_libraries(anaconda_script PUBLIC anaconda_ecs lua_static anaconda_warnings)

# ---- anaconda_anim (sprite animation — host + Android) ---------------------
add_library(anaconda_anim STATIC
    src/animation/AnimatorSystem.cpp
)
target_include_directories(anaconda_anim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaconda_anim PUBLIC anaconda_ecs anaconda_warnings)

# ---- anaconda_atlas (texture atlas — host + Android) -----------------------
add_library(anaconda_atlas STATIC
    src/renderer/Atlas.cpp
    src/renderer/DebugDraw.cpp
)
target_include_directories(anaconda_atlas PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaconda_atlas PUBLIC anaconda_ecs anaconda_math anaconda_sim anaconda_warnings)

# ---- anaconda_prefab helpers are part of anaconda_scene for reuse -----------


# ============================================================================
#  Android-only targets (renderer touches GLES3, engine touches JNI)
# ============================================================================
if(ANDROID)
    # ---- anaconda_renderer (GLES3 pipeline) --------------------------------
    add_library(anaconda_renderer STATIC
        src/renderer/Shader.cpp
        src/renderer/Mesh.cpp
        src/renderer/Texture.cpp
        src/renderer/Renderer.cpp
    )
    target_include_directories(anaconda_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(anaconda_renderer
        PUBLIC anaconda_math anaconda_ecs anaconda_warnings
        PRIVATE GLESv3 EGL log
    )

    # ---- anaconda_engine (shared JNI library) ------------------------------
    add_library(anaconda_engine SHARED
        src/core/Engine.cpp
        src/platform/android/JniBridge.cpp
    )
    target_include_directories(anaconda_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(anaconda_engine
        PRIVATE
            anaconda_sim
            anaconda_scene
            anaconda_assets
            anaconda_renderer
            anaconda_atlas
            anaconda_script
            anaconda_anim
            anaconda_math
            anaconda_ecs
            anaconda_warnings
            log       # __android_log_print
            android   # JNI_OnLoad / JavaVM
            GLESv3
            EGL
    )
    target_compile_definitions(anaconda_engine
        PRIVATE ANACONDA_VERSION="${PROJECT_VERSION}"
    )

elseif(NOT ANACONDA_BUILD_TESTS)
    message(WARNING
        "Not building for Android and ANACONDA_BUILD_TESTS=OFF — "
        "nothing to build. Enable tests: cmake -DANACONDA_BUILD_TESTS=ON")
endif()

# ============================================================================
#  Host-side unit tests (CI: ubuntu + g++, no Android SDK, no GL)
# ============================================================================
if(ANACONDA_BUILD_TESTS)
    enable_testing()

    # Each suite is its own binary — a crash in one cannot mask another's
    # failures. scene/image suites exercise the Month-2 persistence and asset
    # decoding layers end-to-end on the host (no GL, no Android).
    foreach(test_name IN ITEMS math ecs physics scene image picking)
        add_executable(test_${test_name} tests/test_${test_name}.cpp)
        target_link_libraries(test_${test_name}
            PRIVATE anaconda_sim anaconda_scene anaconda_assets anaconda_script anaconda_anim anaconda_atlas)
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()

    # Additional Month 3-5 suites
    foreach(test_name IN ITEMS prefab anim script atlas)
        add_executable(test_${test_name} tests/test_${test_name}.cpp)
        target_link_libraries(test_${test_name}
            PRIVATE anaconda_sim anaconda_scene anaconda_assets anaconda_script anaconda_anim anaconda_atlas)
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()
endif()
