# Anaconda-Engine — System Architecture

> **Document status:** Phase 1 authoritative design.
> Companion documents: [ROADMAP.md](ROADMAP.md) · [SCRIPTING.md](SCRIPTING.md) · [ASSET_PIPELINE_AND_EXPORT.md](ASSET_PIPELINE_AND_EXPORT.md)

---

## 1. Vision & Scope

Anaconda-Engine is a **full standalone game engine + IDE for Android**: a single
installable app where a creator can build 2D/3D games *entirely on a phone or
tablet* — scene editing, scripting, asset import, play-testing, and finally
**exporting a shareable APK/AAB** — without ever touching a desktop computer.

That splits into three products inside one repository:

| Product | What it is | Module(s) |
|---|---|---|
| **Engine** | Real-time runtime: renderer, ECS, physics, scripting, assets | `engine-core` (C++/NDK) |
| **Editor (IDE)** | Scene hierarchy, inspector, viewport, asset browser, play mode | `editor-ui` (Compose) |
| **Player/Exporter** | Runtime shell that loads an exported project & renders it | `app` shell + export templates (Phase 5) |

---

## 2. Tech Stack (with rationale)

| Layer | Choice | Why this, not the alternative |
|---|---|---|
| **Editor UI** | **Kotlin + Jetpack Compose (Material 3)** | Compose gives IDE-grade declarative panels (state → UI), fast iteration, small APK vs XML View hierarchies. XML Views kept out on purpose. |
| **Native core** | **C++20 via Android NDK** | Deterministic memory/layout for ECS, access to GLES/Vulkan drivers, reusability of the runtime in an exported player. |
| **Build (native)** | **CMake 3.22.1** (AGP `externalNativeBuild`) | First-class NDK integration, target-per-subsystem design (see §4), host-side unit tests for math/ECS/physics. |
| **Rendering** | **OpenGL ES 3.0** (Phase 1) → abstraction ready for **Vulkan** (Phase 6) | GLES3 covers ~100% of active devices, fastest to get a correct pipeline. All GL calls live inside `anaconda_renderer`, so a Vulkan backend swaps in behind the same `Renderer` API. |
| **ECS** | **Custom sparse-set registry** | Cache-friendly iteration (dense component arrays), O(1) add/remove, no virtual dispatch in hot loops. Chosen over EnTT as a *vendored-forever* dependency we fully control. |
| **Physics** | **Custom mini-2.5D physics** (gravity, velocity, AABB) → Box2D-style solver later | Phase 1 needs predictable, tiny, testable physics; full constraint solver is Month 3–4 work. |
| **Scripting** | **Lua 5.4** (recommended, see SCRIPTING.md) + optional visual nodes | Tiny (~300 KB), embeddable, sandboxable, hot-reloadable. V8/JSCore rejected for size & integration cost. |
| **Editor↔Engine bridge** | **JNI + `RegisterNatives`** | Airtight signatures validated at `JNI_OnLoad`; no fragile mangled names; single `NativeEngine` Kotlin object as the *only* entry point. |
| **Async/state in UI** | `StateFlow` + single `EditorViewModel` | One source of truth for hierarchy/selection/inspector; snapshot-based scene sync in Phase 1, delta events in Phase 3. |
| **CI** | GitHub Actions | Two jobs: host C++ unit tests (fast, every push) + full Android NDK build (assembleDebug) (see `.github/workflows/ci.yml`). |

### Version matrix (Phase 1, pinned)

| Tool | Version |
|---|---|
| AGP | 8.7.3 |
| Gradle | 8.9 (CI via `gradle/actions/setup-gradle`) |
| Kotlin | 2.0.21 (Compose Compiler plugin bundled) |
| Compose BOM | 2024.09.03 |
| compileSdk / targetSdk | 35 |
| minSdk | **24** (Android 7.0 — GLES 3.0 devices, 64-bit capable) |
| NDK | 26.3.11579264 (r26c) |
| CMake | 3.22.1 |
| JDK | 17 |

---

## 3. High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                              :app  (Kotlin)                              │
│   MainActivity  ──  window / lifecycle / theming  ──  launcher manifest   │
└───────────────────────────────┬──────────────────────────────────────────┘
                                │ composes
┌───────────────────────────────▼──────────────────────────────────────────┐
│                        :editor-ui  (Jetpack Compose)                     │
│  ┌────────────┐ ┌─────────────────────────────┐ ┌──────────────────────┐  │
│  │  Toolbar   │ │  GameViewport               │ │  Scene Hierarchy     │  │
│  │ play/stop  │ │  (AndroidView<GLSurfaceView>)│ │  Inspector           │  │
│  │ export     │ │  + gesture detectors        │ │  Asset Browser       │  │
│  └────────────┘ └──────────────┬──────────────┘ └──────────┬───────────┘  │
│                    EditorViewModel (StateFlow) ◄───────────┘              │
└───────────────────────────────┬──────────────────────────────────────────┘
                    JNI (thread-safe, snapshot + command API)
┌───────────────────────────────▼──────────────────────────────────────────┐
│                     :engine-core — libanaconda_engine.so                 │
│                                                                          │
│  ┌──────────── Engine façade (C++) ────────────┐                         │
│  │ frame loop · run-state (Edit/Play/Pause)    │                         │
│  │ world mutex · demo scene · snapshots        │                         │
│  └──┬──────────────┬───────────────┬───────────┘                         │
│     │              │               │                                     │
│  ┌──▼─────────┐ ┌──▼──────────┐ ┌──▼──────────────┐  ┌────────────────┐  │
│  │ anaconda_  │ │ anaconda_   │ │ anaconda_       │  │ anaconda_math  │  │
│  │ renderer   │ │ sim         │ │ ecs             │◄─┤ Vec2/3/4 Mat4  │  │
│  │ GLES 3.0   │ │ gravity+AABB│ │ sparse-set      │  └────────────────┘  │
│  │ shaders    │ │ rigidbodies │ │ Registry/View   │                      │
│  │ VAO/VBO    │ │ contacts    │ │ Transform etc.  │   (Phase 4:          │
│  │ cameras    │ └─────────────┘ └─────────────────┘    ScriptEngine/Lua) │  │
│  └────────────┘                                                          │
└──────────────────────────────────────────────────────────────────────────┘
         │ GLES3 calls                          ▲ SAF / Storage (Phase 2)
         ▼                                      │
   GPU (via GLSurfaceView EGL context)     Project files: assets/, scenes/
```

### Frame flow (who calls what, in order)

```
Choreographer/vsync
   └─ GLSurfaceView GL thread
        └─ Renderer.onSurfaceChanged/Created/DrawFrame   (Kotlin)
             └─ NativeEngine.nativeDrawFrame()           (JNI)
                  └─ Engine::renderFrame()               (C++)
                       1. compute dt (steady_clock, clamped ≤ 250 ms)
                       2. lock worldMutex_
                       3. if RunState::Play → PhysicsSystem::step(world, dt)
                       4. Renderer::beginFrame(clear)     // glClear, bind sprite shader
                       5. Renderer::renderScene(world, camera)
                       6. Renderer::endFrame()            // stats; swap by GLSurfaceView
                       7. unlock
   Main/UI thread
        ├─ Compose panels render from StateFlow<EntitySnapshot>
        ├─ user edits → NativeEngine.setTransform(...) → worldMutex_-guarded write
        └─ NativeEngine.getSceneSnapshot() → parse → StateFlow refresh
```

### Threading model (Phase 1 rules — enforced in code)

1. **GL thread only** may create/destroy GL objects (`onSurfaceCreated`,
   `onSurfaceChanged`, `renderFrame`). The Kotlin `GLSurfaceView.Renderer`
   guarantees this ordering.
2. **All world data access** (read *and* write, any thread) is serialized by
   `Engine::worldMutex_`. Reads are cheap snapshot serialization; writes are
   single-field updates. No nested locks exist → no deadlock classes.
3. **UI never blocks on GL.** Mutations are immediate under the mutex; if a
   mutation must be ordered *after* the next frame (e.g. texture upload from
   the asset pipeline), Phase 2 introduces a **render command queue** drained
   at the top of `renderFrame()`.
4. `GLSurfaceView.onPause()/onResume()` are forwarded from lifecycle events in
   `GameViewport.kt`, with `preserveEGLContextOnPause = true` to avoid shader
   recompiles on quick tab-outs.

---

## 4. Repository / Module Structure

```
Anaconda-Engine/
├── README.md                       # public face: badges, features, quick start
├── LICENSE                         # MIT
├── .gitignore                      # Android + NDK + C++ + Kotlin specific
├── .editorconfig
├── settings.gradle.kts             # includes :app :editor-ui :engine-core
├── build.gradle.kts                # plugin aliases (version catalog)
├── gradle/libs.versions.toml       # single source of truth for versions
├── gradle.properties
│
├── .github/workflows/ci.yml        # CI: host C++ tests + Android NDK build
│
├── docs/
│   ├── ARCHITECTURE.md             # this file
│   ├── ROADMAP.md                  # 6-month phased plan
│   ├── SCRIPTING.md                # Lua/node-scripting design
│   └── ASSET_PIPELINE_AND_EXPORT.md# import + APK/AAB export design
│
├── app/                            # ── Android application shell
│   ├── build.gradle.kts
│   ├── proguard-rules.pro
│   └── src/main/
│       ├── AndroidManifest.xml     # GLES3 feature, launcher activity
│       ├── java/.../MainActivity.kt
│       └── res/values/{strings,themes}.xml
│
├── editor-ui/                      # ── Compose IDE
│   ├── build.gradle.kts
│   └── src/main/java/com/anaconda/editor/
│       ├── EditorScreen.kt         # responsive panel layout + snackbars
│       ├── EditorViewModel.kt      # editor state machine
│       ├── theme/EditorTheme.kt    # dark IDE palette
│       ├── viewport/GameViewport.kt# GLSurfaceView + gestures + lifecycle
│       ├── panels/
│       │   ├── ToolbarPanel.kt     # play/pause/stop, FPS, export
│       │   ├── SceneHierarchyPanel.kt
│       │   ├── InspectorPanel.kt   # transform/sprite/rigidbody editing
│       │   └── AssetBrowserPanel.kt# Phase 1 placeholder grid (Phase 2: SAF)
│       └── components/Panels.kt    # shared panel scaffolding, fields
│
└── engine-core/                    # ── Native engine + JNI facade
    ├── build.gradle.kts            # NDK, CMake, ABI filters
    └── src/main/
        ├── java/.../core/
        │   ├── NativeEngine.kt     # object + external funs (JNI)
        │   └── SceneSnapshot.kt    # snapshot data classes + parser
        └── cpp/
            ├── CMakeLists.txt      # target graph (math→ecs→sim→renderer→engine)
            ├── include/anaconda/
            │   ├── core/{Log,Engine}.hpp
            │   ├── math/Math.hpp
            │   ├── ecs/{Ecs,Components}.hpp
            │   ├── physics/PhysicsSystem.hpp
            │   └── renderer/{Camera,Shader,Mesh,Texture,Renderer,Shaders}.hpp
            ├── src/
            │   ├── core/Engine.cpp
            │   ├── physics/PhysicsSystem.cpp
            │   ├── renderer/{Shader,Mesh,Texture,Renderer}.cpp
            │   └── platform/android/JniBridge.cpp
            └── tests/              # host-buildable unit tests (no GL, no Android)
                ├── test_framework.hpp
                ├── test_math.cpp
                ├── test_ecs.cpp
                └── test_physics.cpp
```

### CMake target graph

```
anaconda_math    (INTERFACE)  headers only
anaconda_ecs     (INTERFACE)  headers only ──► anaconda_math
anaconda_sim     (STATIC)     PhysicsSystem.cpp ──► math + ecs
        │
        │   [only when ANDROID]
        ▼
anaconda_renderer(STATIC)     Shader/Mesh/Texture/Renderer ──► GLESv3, EGL, log
        │
        ▼
anaconda_engine  (SHARED)     Engine.cpp + JniBridge.cpp
        └── libanaconda_engine.so  ──► sim + renderer + android + log + GLESv3

tests (option ANACONDA_BUILD_TESTS, host): test_math/test_ecs/test_physics → sim
```

Why split targets? **Incremental builds + enforced layering**: renderer code
*cannot* include physics headers without changing a CMake link line reviewers
will see; tests never link GL, so they build on any Linux runner in seconds.

---

## 5. Key Subsystems (design detail)

### 5.1 Renderer (`anaconda_renderer`)

* `Shader` — compile/link with full info-log diagnostics, RAII (move-only,
  `release()` on context loss), uniform lookup cache.
* `Mesh` — VAO + VBO + EBO wrapper. Interleaved vertex:
  `pos(3) | color(4) | uv(2)`. Built-ins: **unit quad**, **cube**.
* `Texture` — GLES3 wrapper; `createWhite()` default 1×1 binding so the sprite
  shader has a valid sampler from frame one; `uploadRGBA()` ready for Phase 2
  image import.
* `Camera` — orthographic (2D, editor default: half-height 5 world units) and
  perspective modes, `lookAt` view matrix, pan/zoom helpers driven by touch.
* `Renderer` — owns GL state (blend on, depth off for 2D Phase 1), embedded
  GLSL 300 es **sprite shader** (`uProj·uView·uModel`, texture × vertex color),
  `renderScene(registry, camera)` iterates `View<Transform, Sprite>` and draws.
  Draw-call/stat counters exposed to the toolbar FPS chip.

Phase 1 intentionally ships **one shader + white texture**: correct matrices,
correct buffer plumbing, correct JNI frame loop. Textures, batching, lighting,
and a debug-grid pass are staged in the roadmap.

### 5.2 ECS (`anaconda_ecs`)

* Entity = `uint32` packed `generation(12 bits) | index(20 bits)` → up to
  1,048,575 live entities, stale-handle detection for free.
* `ComponentPool<T>`: sparse array `entity index → dense slot+1`, dense arrays
  for entities and components (struct-of-arrays style swaps on removal).
* `Registry`: type-erased pool map (`std::type_index`), `add/get/has/remove`,
  `aliveEntities()`.
* `View<Ts...>`: iterates the *smallest* matching pool and checks the rest —
  no world-wide scans.

Components (Phase 1): `TagComponent`, `TransformComponent`,
`SpriteComponent`, `RigidBodyComponent`, `BoxColliderComponent`,
`ScriptComponent` (dormant until Phase 4).

### 5.3 Physics (`anaconda_sim`)

`PhysicsSystem::step(world, dt)`:

1. **Integrate** — `v += gravity * gravityScale * dt`, `p += v * dt` for every
   `Transform + RigidBody`.
2. **Broadphase (Phase 1)** — O(n²)/O(n·m) AABB gather (≤ a few hundred
   bodies; grid/SAP scheduled Month 3).
3. **Narrowphase** — AABB overlap; minimal translation vector on the axis of
   least penetration (X/Y only when `is2D` — prevents bogus Z resolutions on
   flat sprites).
4. **Resolve** — dynamic vs static pushes the dynamic body fully; dynamic vs
   dynamic splits the MTV. Contact along +Y sets `grounded` and zeroes
   downward velocity (jump-ready).

### 5.4 Editor ↔ Engine bridge

* **Down (UI → engine):** direct calls (`setTransform`, `setRunState`, …)
  under `worldMutex_`. Instant, no frame latency.
* **Up (engine → UI):** `getSceneSnapshot()` serializes every live entity to
  a line protocol:
  `id|name|px,py,pz|rx,ry,rz|sx,sy,sz|flags|cr,cg,cb,ca`
  with `flags` bitmask = sprite/rigidbody/collider. Parsed into immutable
  `EntitySnapshot` data classes. Chosen over per-field JNI getters: one call,
  easy to evolve, trivially testable. Phase 3 replaces polling with a
  dirty-flag delta channel.

### 5.5 Input

Kotlin owns gesture recognition (`ScaleGestureDetector` + manual pan math),
native receives **semantic gestures**: `PAN(dxPx,dyPx)`, `ZOOM(scaleFactor)`,
`TAP(x,y)` (reserved for GPU-less ray picking in Phase 3). Pixels are
converted to world units *inside* native where camera state lives.

---

## 6. Design Decisions & Trade-offs (ADR log)

| # | Decision | Alternatives rejected | Consequence |
|---|---|---|---|
| ADR-1 | GLES3 first, backend interface from day one | Vulkan-first | Ship Phase 1 in weeks; Vulkan risk isolated to `Renderer`. |
| ADR-2 | Custom sparse-set ECS | EnTT (vendor), entity-per-GameObject (Unity-style) | ~500 LOC to own forever; iteration is cache-friendly; no submodule rot. |
| ADR-3 | Snapshot-based hierarchy sync (Phase 1) | JNI callbacks per entity | Polling simplicity now; delta events added when scenes exceed ~1k nodes. |
| ADR-4 | Thin Kotlin facade; all logic in C++ | Kotlin-first engine with JNI per feature | Exported player reuses the exact same `.so`; one place for logic bugs. |
| ADR-5 | `RegisterNatives` in `JNI_OnLoad` | Mangled `Java_...` symbols | Compile-time-ish safety: class/method/signature mismatch fails at load, loudly. |
| ADR-6 | World mutex (coarse) over lock-free ECS access | Per-pool atomics | Editor-scale concurrency (1 writer UI + 1 frame loop) never contends; revisit only if profiling says so. |
| ADR-7 | Lua for scripts (recommendation) | V8, JSI/Hermes, visual-only | See SCRIPTING.md §comparison — 300 KB vs ~9 MB, pure C API, hot reload. |
| ADR-8 | On-device APK export via template + `apksig` (Phase 5) | Full toolchain on device (aapt2/d8) | Template approach needs ~15 MB of bundled tooling instead of the full build stack; AAB remains a CLI/CI path. See ASSET_PIPELINE_AND_EXPORT.md. |

---

## 7. Extension Points (where Phase 2–6 plug in)

| Extension point | Phase | Mechanism |
|---|---|---|
| Texture import (PNG/JPG → `Texture`) | 2 | SAF → copy to project → decode on GL thread via render-command queue |
| Audio (`AudioEngine`, OpenSL ES/AAudio) | 2 | new `anaconda_audio` static lib |
| Ray picking / gizmos | 3 | CPU ray vs AABB from camera unproject (no GL readback) |
| Lua `ScriptEngine` + per-frame `onUpdate` | 4 | `anaconda_script` lib; `ScriptComponent` already present |
| Scene serialization (JSON/binary) | 4 | `SceneSerializer` walks Registry; snapshot parser shares field format |
| Player template + APK signer | 5 | template APK in CI artifacts + `apksig` in-app |
| Vulkan backend | 6 | `Renderer` impl swap behind identical API |

---

## 8. Quality Gates

* **Host unit tests** (every push): math (transforms, inverse, projections),
  ECS (generation recycling, view integrity), physics (gravity, grounded,
  MTV resolution). Run by CI job `native-tests`.
* **Android build** (every push): `gradle :app:assembleDebug` with NDK —
  catches JNI signature drift because `RegisterNatives` fails fast on load in
  instrumentation smoke tests (Phase 2 adds `androidTest`).
* **Static analysis**: `-Wall -Wextra` (Phase 2: clang-tidy + Android Lint
  fatal severity in CI).
* **Performance budgets** (Phase 3+): 10k sprites ≤ 2 ms CPU/frame on a
  mid-range device; snapshot of 1k entities ≤ 1 ms.
