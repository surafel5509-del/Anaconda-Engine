# Anaconda-Engine — 6-Month Phased Roadmap

> From an empty repository to a shippable mobile game-creation app.
> Each month lists **Goals → Deliverables → Acceptance criteria**. Anything
> marked **[DONE in Phase 1 foundation]** already exists in this repository.

---

## Month 1 — Engine Core & Editor Shell  ✅ **done (Phase 1 foundation)**

**Goal:** A running app: native GLES3 renderer inside a Compose editor shell,
with a live, editable scene.

| # | Deliverable | Status |
|---|---|---|
| 1.1 | Multi-module Gradle project (`:app`, `:editor-ui`, `:engine-core`) + version catalog | **DONE** |
| 1.2 | CMake target graph (math / ecs / sim / renderer / engine) + NDK wiring | **DONE** |
| 1.3 | Custom math lib: Vec2/3/4, Mat4 (column-major), perspective/ortho/lookAt/inverse | **DONE** |
| 1.4 | Sparse-set ECS: entities with generations, `View<Ts...>`, core components | **DONE** |
| 1.5 | GLES3 pipeline: shader compiler, VAO/VBO/EBO meshes, white texture, sprite shader | **DONE** |
| 1.6 | Editor camera (ortho) + touch pan/zoom gestures | **DONE** |
| 1.7 | Physics step: gravity, velocity integration, AABB collision, grounded flag | **DONE** |
| 1.8 | JNI bridge (`RegisterNatives`), frame loop on GL thread, world mutex | **DONE** |
| 1.9 | Compose editor: Toolbar, Scene Hierarchy, Inspector (transform edit), Asset Browser placeholder, responsive layout | **DONE** |
| 1.10 | Play/Pause/Stop run states with transform rollback on Stop | **DONE** |
| 1.11 | Host unit tests (math/ECS/physics) + GitHub Actions CI | **DONE** |
| 1.12 | Professional README, ARCHITECTURE, .gitignore, MIT license | **DONE** |

**Acceptance criteria (end of Month 1)**
1. App launches on a phone → editor UI visible → viewport renders the demo
   scene (ground, player, platforms, coin).
2. Tapping an entity in the Hierarchy selects it; Inspector edits position
   live in the viewport.
3. Play button drops the player under gravity; it lands on the ground and the
   `grounded` flag is correct; Stop restores the pre-play transforms.
4. Pinch/pan moves the editor camera.
5. CI green on every push (host tests + `assembleDebug`).

---

## Month 2 — Project Model, Assets & Real Scenes  ← **current phase**

**Goal:** Persist work; bring real art/sound into projects.

**Status (shipped):** project folder system (`filesDir/projects/<name>/`
with `project.json`, `scenes/`, `assets/sprites/`); binary scene
save/load (format v1, magic `ANSC`, bounds-checked) + 60 s autosave;
undo/redo stack (50 steps — value strokes coalesce, delete-undo replays a
snapshot line through `nativeRestoreEntity` with id remapping); SAF import of
PNG/JPEG → vendored `stb_image` decode → GL upload via the render-command
queue → automatic texture assignment to the selected sprite; tap-to-assign
from the Asset Browser; JVM unit tests for the snapshot parser in CI.

**Remaining this month:** audio (AAudio/OpenSL Phase A), Hierarchy 2.0
(reparent/duplicate/multi-select), WebP acceptance via auto-conversion,
import thumbnails, UV trim + sorting order.

* **Project system** — on-device folder layout (`projects/<name>/project.json`,
  `scenes/`, `assets/`, `scripts/`), create/open/save from the app.
* **Scene serialization** — binary + JSON dump of the Registry (every
  component), autosave, undo/redo stack (command pattern, 50-step depth).
* **Asset import (SAF)** — PNG/JPG → decode (stb_image vendored) →
  `Texture` upload via render-command queue; WAV/OGG copied into `assets/`;
  import history + thumbnails in the Asset Browser (replaces placeholder grid).
  WebP ships rejected-with-convert-message in the first cut, auto-converted
  once the transcoder lands.
* **Sprite editing** — per-entity texture assignment, UV rect trim, color tint,
  sorting order layer.
* **Audio (Phase A)** — `anaconda_audio` static lib over AAudio/OpenSL: play
  SFX from scripts and inspector preview button.
* **Hierarchy 2.0** — rename, reparent (local-vs-world transforms), duplicate,
  multi-select, delete with confirm.

**Acceptance:** create project → import 5 sprites → build a scene → kill app →
reopen → scene identical; sprite visible with correct texture and order.

---

## Month 3 — Editor Power Tools: Picking, Gizmos, Prefabs, Physics Tuning

**Goal:** Editing feels like a desktop IDE, on touch.

* **Selection & gizmos** — tap-to-select via CPU ray/AABB picking; drag-move
  handles (screen-space) with snapping; rotation/scale handles.
* **Undo/Redo** — wired to every mutation channel (already command-shaped
  from Month 2).
* **Grid & snapping**, scene navigation (F-frame selected, minimap).
* **Prefab system** — save entity subtree as prefab, instantiate, override
  tracking.
* **Physics tuning** — collider visualization in viewport, material fields
  (restitution/friction), simple broadphase grid, fixed 60 Hz accumulator
  (decoupled from render rate).
* **Profiling HUD** — frame time, draw calls, entity counts, physics ms.

**Acceptance:** pick an entity by tapping it in the viewport; drag it with a
gizmo; undo returns it; prefabs re-spawn with overrides.

---

## Month 4 — Scripting, Hot Reload, & Advanced Rendering

**Goal:** Games become *logic*, not just layout.

* **Lua 5.4 integration** (`anaconda_script`) — sandboxed state per scene,
  API surface: `log`, transform get/set, input (touch/keys), timers, spawn/
  destroy, audio play, math bindings. See [SCRIPTING.md](SCRIPTING.md).
* **Script editor in-app** — syntax-highlighted editor (Kotlin
  `VisualTransformation`), run/current-file diagnostics, error line mapping
  back to a stack trace in the Console panel.
* **Hot reload** — edit → save → re-run `onUpdate` world without scene reset
  (script state preserved for functions that opt in).
* **Rendering 2.0** — texture atlas batching (one draw call per atlas page),
  sprite animation clips, render layers, camera effects (shake/zoom lerp).
* **Scene serializer final format** — stable binary with version header;
  cross-version migration hooks.

**Acceptance:** write `onUpdate` that moves a sprite with touch input, see it
work live, edit while running, hit 60 FPS with 1,000 animated sprites.

---

## Month 5 — Export System: Standalone APK & AAB

**Goal:** Users share games, not projects.

* **Player template** — a lean `app-player` APK shell (no editor code) that
  boots `libanaconda_engine.so`, loads `project.bin` from assets, and runs.
* **On-device APK export** — merge scene+assets into the template's
  `assets/project/`, re-sign with per-user debug/release keystore via the
  `apksig` library, `zipalign`, share sheet (Intent `ACTION_SEND`).
* **AAB path** — Gradle/Bundler pipeline documented for CI (Play Store
  publishing); `bundletool` instructions in docs.
* **Project pack** — export an `.anaconda` project archive (import on another
  device) — sharing *source* too, not just builds.
* **Export wizard UI** — package name, version, icon picker, min-SDK, ABI
  split, keystore manager.

**Acceptance:** build a 30-second game on the phone → Export APK → install on
a second device → game runs fullscreen with no editor present.

---

## Month 6 — Polish, Store Readiness & Public Beta

**Goal:** Something you'd publish on Play Store and GitHub simultaneously.

* **Onboarding** — sample projects ("Pong", "Platformer", "Top-down") that
  double as tutorials.
* **Tablet/foldable layout pass**, large-screen two-pane editor, keyboard &
  mouse support (hardware shortcuts: Ctrl+Z, Ctrl+S, Del).
* **Performance & stability** — startup < 1.5 s cold, memory profiling,
  ANR/crash watchdog, telemetry opt-in.
* **Crash reporting** — Breakpad/crashpad integration → symbolized reports.
* **Docs site** — engine manual (scripting API reference generated from Lua
  annotations), video tutorials.
* **Beta distribution** — GitHub Releases (signed APK), Play Console internal
  track, feedback loop + issue templates.
* **CI/CD complete** — release workflow: tag → tests → assembleRelease →
  sign → GitHub Release + Play upload (fastlane or custom).

**Acceptance (product launch gate):**
1. New user installs app and exports a playable APK within 30 minutes using
   only in-app docs.
2. 60 FPS on a mid-range 2022 device for a 1,000-sprite scene.
3. Zero known crash-on-launch paths (watchdog + crash reporting live).
4. Public repo: docs, CI badges green, sample projects, license, contributing
   guide.

---

## Risk register

| Risk | Impact | Mitigation |
|---|---|---|
| On-device APK signing restrictions (Play App Signing) | Store publishing friction | Ship Play-relevant AAB via CI path; on-device APK targets sideload/share. |
| GLES driver quirks (Adreno/Mali) | Black screens | EGL config fallback chain, `glGetError` spam-logging in debug builds, device test matrix. |
| NDK/AGP version churn | Build breaks | Versions pinned in `libs.versions.toml` + `ndkVersion`; upgrades only in one PR with CI green. |
| Compose ↔ GLSurfaceView lifecycle bugs | Surface leaks on rotate | `configChanges` on activity + explicit lifecycle forwarding (already implemented). |
| Scope creep (3D + 2D + scripting + export at once) | Nothing ships | Phases are strictly ordered; 3D rendering features explicitly land after Month 4. |
