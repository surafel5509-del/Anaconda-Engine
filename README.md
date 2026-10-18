<div align="center">

# 🐍 Anaconda-Engine

### A full professional game engine & IDE — built entirely for Android

**Create, script, and export 2D/3D games right from your phone or tablet.**

[![CI](https://github.com/surafel5509-del/Anaconda-Engine/actions/workflows/ci.yml/badge.svg)](https://github.com/surafel5509-del/Anaconda-Engine/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Android Min SDK](https://img.shields.io/badge/Min%20SDK-24%20(Android%207.0)-3DDC84.svg)](https://developer.android.com/about/versions/nougat)
[![OpenGL ES](https://img.shields.io/badge/OpenGL%20ES-3.0-0B0E13.svg)](https://www.khronos.org/opengles/)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)](https://en.cppreference.com/w/cpp/20)
[![Kotlin](https://img.shields.io/badge/Kotlin-2.0-7F52FF.svg)](https://kotlinlang.org/)
[![Jetpack Compose](https://img.shields.io/badge/UI-Jetpack%20Compose-4285F4.svg)](https://developer.android.com/jetpack/compose)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)

</div>

---

## ✨ What is this?

**Anaconda-Engine** is not just a game *library* — it's a **standalone
Android application**: a mobile IDE where you

* 🎬 build scenes in a **live editor** (hierarchy, inspector, viewport, assets),
* 🎮 hit **Play** and test your game *inside* the editor with real physics,
* 🧠 script gameplay with an **embedded scripting runtime** (Lua, Phase 4),
* 📦 **export a real APK/AAB** of your game and share it — no PC required.

The engine core is a modern **C++20 / NDK** runtime (OpenGL ES 3.0 renderer,
sparse-set ECS, custom physics), driven by a **Kotlin + Jetpack Compose**
editor over a thin, typed JNI bridge.

> **Current status: Phase 1 (Month 1) foundation complete** — build system,
> renderer, ECS, physics, JNI bridge, and the full editor shell are in the
> repo and covered by host-side unit tests. See the [Roadmap](docs/ROADMAP.md).

---

## 🧭 Architecture at a glance

```
:app  (Kotlin shell — Activity, manifest, theme)
  └── :editor-ui  (Jetpack Compose IDE — toolbar, hierarchy, viewport,
                   inspector, asset browser, EditorViewModel)
         └── :engine-core  (thin JNI facade: NativeEngine)
                └── libanaconda_engine.so  (C++20 / NDK / CMake)
                      ├── anaconda_renderer  OpenGL ES 3.0 pipeline
                      ├── anaconda_sim       gravity + AABB physics world
                      ├── anaconda_ecs       sparse-set Entity Component System
                      └── anaconda_math      Vec2/3/4 · Mat4 (column-major)
```

Full design documents:

| Document | Contents |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Tech stack rationale, module/CMake graphs, threading & frame flow, ADR log |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Month-by-month plan from empty repo → public beta |
| [docs/SCRIPTING.md](docs/SCRIPTING.md) | Lua integration design + C++ binding sketches |
| [docs/ASSET_PIPELINE_AND_EXPORT.md](docs/ASSET_PIPELINE_AND_EXPORT.md) | SAF import pipeline & on-device APK/AAB export |

---

## 🗂️ Repository layout

```
Anaconda-Engine/
├── app/            # Android application shell (launcher, manifest, res)
├── editor-ui/      # Jetpack Compose editor IDE (panels + GL viewport)
├── engine-core/    # Native C++ engine (CMake/NDK) + Kotlin JNI facade
│   └── src/main/cpp/
│       ├── include/anaconda/   # math, ecs, physics, renderer, core headers
│       ├── src/                # Engine loop, GLES pipeline, JNI bridge
│       └── tests/              # host-buildable unit tests (no GL needed)
├── docs/           # architecture, roadmap, scripting & export designs
├── gradle/         # version catalog (libs.versions.toml)
└── .github/workflows/ci.yml    # CI: host tests + Android NDK build
```

---

## 🚀 Getting started

### Prerequisites

| Tool | Version |
|---|---|
| JDK | 17+ |
| Android Studio | Ladybug (or newer) with SDK 35 |
| Android NDK | `26.3.11579264` (r26c) — install via SDK Manager ▸ SDK Tools |
| CMake | `3.22.1` (SDK Manager ▸ SDK Tools) |
| Gradle | 8.9 (see note below) |

### Build & run

```bash
# 1) Clone
git clone https://github.com/surafel5509-del/Anaconda-Engine.git
cd Anaconda-Engine

# 2) One-time: generate the Gradle wrapper (or open directly in Android Studio)
gradle wrapper --gradle-version 8.9

# 3) Build the editor APK (arm64 + armeabi-v7a, NDK + CMake wired automatically)
./gradlew :app:assembleDebug

# 4) Install on a connected device/emulator that supports GLES 3.0
./gradlew :app:installDebug
```

> **Why no committed wrapper JAR?** The binary jar is generated, not written;
> run step 2 once (or let Android Studio sync create it) and commit the
> resulting `gradle/wrapper/` folder. CI installs Gradle explicitly instead —
> see [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

### Run the native unit tests on your desktop (no Android SDK needed)

```bash
cmake -S engine-core/src/main/cpp -B build-host -DANACONDA_BUILD_TESTS=ON
cmake --build build-host -j
ctest --test-dir build-host --output-on-failure
```

---

## 🕹️ Using the editor (Phase 1 feature set)

| Panel | What it does today |
|---|---|
| **Toolbar** | Play / Pause / Stop with transform rollback, live FPS, Export entry point |
| **Scene Hierarchy** | Lists every entity; select, create, delete, rename |
| **Game Viewport** | Native GLES3 surface — demo scene, pinch-zoom, two-finger/drag pan |
| **Inspector** | Edit name, position, rotation, scale live; tint sprites; toggle gravity |
| **Asset Browser** | Project categories shell (real import lands in Phase 2) |

Play mode runs real physics: gravity, velocity integration, AABB collision,
grounded detection — all unit-tested.

---

## 🗺️ Roadmap (summary)

| Month | Milestone |
|---|---|
| **1** ✅ | Core engine + editor shell (this repository state) |
| **2** | Project model, scene save/load, asset import (SAF), audio |
| **3** | Viewport picking & gizmos, undo/redo, prefabs, physics tuning |
| **4** | Lua scripting + hot reload, atlas batching, animations |
| **5** | **APK/AAB export** — standalone player builds & share sheet |
| **6** | Polish, sample games, tablet layouts, crash reporting, public beta |

Detailed acceptance criteria per month: [docs/ROADMAP.md](docs/ROADMAP.md).

---

## 🏗️ Tech stack

* **Kotlin 2.0 + Jetpack Compose (Material 3)** — editor UI
* **C++20 / Android NDK r26 / CMake** — engine core
* **OpenGL ES 3.0** — rendering (Vulkan backend planned behind the same API)
* **Custom sparse-set ECS** — deterministic, cache-friendly, zero dependencies
* **Custom physics** — gravity/velocity/AABB now; solver-driven contacts next
* **Lua 5.4** (Phase 4) — sandboxed in-app scripting with hot reload
* **GitHub Actions** — host unit tests + NDK build on every push

---

## 🤝 Contributing

Issues and PRs are welcome — especially device-driver bug reports (GPU model +
`adb logcat -s AnacondaGL`) and reproducible rendering glitches.

1. Fork & create a feature branch from `main`.
2. Keep native code warning-clean (`-Wall -Wextra`) and add tests for
   math/ecs/physics changes.
3. Open a PR with a short design note (or a link to a `docs/` update).

## 📜 License

[MIT](LICENSE) © Anaconda-Engine Authors — free for commercial and personal use.

---

<div align="center">
<sub>Built with ⚡ C++20, Compose, and far too much coffee — Phase 1 of 6.</sub>
</div>
