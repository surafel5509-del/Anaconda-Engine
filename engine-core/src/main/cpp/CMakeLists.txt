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
        LANGUAGES CXX)

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
            PRIVATE anaconda_sim anaconda_scene anaconda_assets)
        add_test(NAME ${test_name} COMMAND test_${test_name})
    endforeach()
endif()
