# Anaconda-Engine — Asset Pipeline & Export System

> How raw files on a phone become engine assets, and how a scene becomes a
> **standalone, shareable APK/AAB**. Targets: Phase 2 (import) and Phase 5
> (export).

---

## Part A — Asset Pipeline (Phase 2)

### A.1 On-device project layout

```
/sdroid/Android/data/com.anaconda.engine/files/projects/<ProjectName>/
├── project.json            # manifest: name, version, apiLevel, entry scene
├── scenes/
│   └── main.scene          # serialized Registry (binary + JSON debug twin)
├── assets/
│   ├── sprites/
│   │   ├── hero.png
│   │   └── hero.meta.json  # importer settings: filter, wrap, pivot, atlas tag
│   ├── audio/
│   │   ├── jump.ogg
│   │   └── jump.meta.json  # loop, volume, category
│   ├── fonts/
│   └── scripts/
│       └── player.lua
├── settings/
│   └── input.json
└── export/
    └── mygame-1.0.apk      # builds land here
```

* App-private storage by default (no permissions needed); **explicit “link
  external file” mode** for big assets via SAF (Storage Access Framework)
  tree grants.
* Everything the engine loads goes through **one importer funnel** so paths,
  metadata and re-import stay consistent.

### A.2 Import flows

```
   Gallery / Files (SAF picker)                    In-app Asset Browser [+ Import]
            │                                               │
            ▼                                               ▼
   ACTION_OPEN_DOCUMENT (image/*, audio/*, application/octet-stream)
            │
            ▼
   Copy stream → assets/<cat>/<uuid-ified-name>   (dedup by content hash, SHA-256)
            │
            ▼
   ┌─ Importer registry (by MIME + extension) ─────────────────────┐
   │  ImageImporter : stb_image decode → RGBA8                      │
   │      ├─ power-of-two policy, max-size clamp (2048 default)     │
   │      ├─ premultiply-alpha option, color space tag (sRGB)       │
   │      └─ GL upload QUEUED to render thread (CommandQueue)       │
   │  AudioImporter: header probe → wav/ogg metadata (Phase 2A)     │
   │  ScriptImporter: text copy + syntax parse check (Phase 4)      │
   │  SceneImporter : .scene → Registry (Phase 5 cross-device)      │
   └────────────────────────────────────────────────────────────────┘
            │
            ▼
   meta.json written next to file  →  Asset Browser refresh (thumbnail job)
```

Key rules:

1. **No GL calls off the GL thread.** The import thread decodes pixels; a
   `RenderCommand` (`uploadTexture(id, pixels, w, h)`) is appended to the
   queue drained at the top of `Engine::renderFrame()`. This is exactly the
   Phase-2 upgrade to the threading model described in ARCHITECTURE §3.
2. **Stable asset IDs** — content hash + path; scene files reference assets
   by ID, not path, so renames don't break scenes.
3. **Thumbnails** — 128 px cached in `assets/.thumbs/`, generated off the UI
   thread (Coroutines + `BitmapFactory` in Kotlin), never by the engine.
4. **Budgets** — importer reports progress via `Flow<ImportProgress>`; large
   files stream-copy to avoid OOM.

### A.3 Asset Browser (editor)

* Categories: All / Sprites / Audio / Scripts / Scenes / Prefabs.
* Drag-and-drop onto viewport → “assign texture to selected sprite” intent.
* Long-press context menu: rename, reimport, delete (with usage check —
  refuses to delete an asset used by any scene without force flag).

---

## Part B — Export System (Phase 5)

### B.1 Two export targets

| Target | Audience | How |
|---|---|---|
| **APK (sideload/share)** | Friends, itch.io, QR-share, testing | **On-device** template merge + re-sign (below) |
| **AAB** | Google Play | **CI/CLI path**: same project pack + `bundletool` on a desktop/Actions runner (documented wizard generates the exact commands) |

Why not full aapt2/d8/zipalign toolchain on the device? It works in principle
(unbundled `aapt2` + `d8` jars can be shipped), but it's ~80 MB of tooling,
slow on phones, and fragile across Android versions. The template approach
below gets a valid, installable APK with ~15 MB of assets and one pure-Java
dependency.

### B.2 On-device APK export pipeline

```
 EXPORT WIZARD (UI)
   ├─ inputs: packageName, versionName/Code, icon, minSdk, ABI(s), keystore
   └─ validates reverse-DNS, not com.anaconda.* (reserved), size budgets
        │
        ▼
 1. BUILD PROJECT PACK
      scene binary + assets → export_tmp/assets/project/  (compressed)
        │
        ▼
 2. OBTAIN TEMPLATE  (shipped in APK as assets/templates/player.apk)
      • prebuilt minimal player shell: MainActivity → EngineActivity
      • already contains libanaconda_engine.so for all ABIs
      • signed once in CI with the Anaconda release key (verified on import)
        │
        ▼
 3. REPLACE PAYLOAD
      rewrite the APK zip: delete assets/project/*, add new entries
      (stored/deflated per entry, keep lib/* alignment for mmap)
        │
        ▼
 4. RE-SIGN
      apksig library (com.android.tools.build:apksig — pure Java):
        • generate or reuse user keystore (AndroidKeyStore-backed, exportable)
        • v2+v3 signature scheme; v1 fallback for minSdk < 24 not needed (24+)
        • block-size aligned signatures computed over the modified zip
        │
        ▼
 5. ALIGN + VERIFY
      zip alignment via apksig's verification pass (and/or bundled zipalign
      binary for arm page alignment of .so entries, 16 KB page-ready)
        │
        ▼
 6. DELIVER
      output: export/<name>-<version>.apk
      → FileProvider URI + ACTION_SEND (share sheet)
      → “Install” button (REQUEST_INSTALL_PACKAGES permission flow)
      → QR code for cross-device install on LAN
```

### B.3 Player shell contract

The exported APK does **not** embed editor code. Boot sequence:

```
PlayerActivity.onCreate
  ├─ read assets/project/project.json  (validate apiVersion, entry scene)
  ├─ Engine.nativeInitPlayer(projectBytes)   // no editor snapshot UI
  ├─ GLSurfaceView → onSurfaceCreated → load scene → RunState::Play
  └─ exports: System.exit on fatal engine error + writes to logcat tag
              "AnacondaPlayer" for support
```

Size target: engine `.so` (~3–6 MB across ABIs) + project assets. ABI splits
in the wizard let exporters ship arm64-only APKs when they don't need legacy.

### B.4 Play Store (AAB) path

1. Wizard produces an **`.anaconda-pack`** (project pack, identical content
   to `assets/project/`).
2. Docs/CI generate a minimal **player Gradle project** on the desktop that
   consumes the pack and runs `bundletool build-bundles` → `.aab`.
3. Alternatively GitHub Actions template repo “anaconda-export” accepts the
   pack as an artifact and publishes the AAB as a workflow artifact ready for
   Play Console upload (Play App Signing handles distribution signing).

This split is deliberate: **Play requires server-side signing anyway** —
on-device signing only ever matters for sideload, which is exactly what APK
export targets.

### B.5 Legal/safety notes surfaced in the wizard

* Exported games include the MIT-licensed engine — attribution screen
  template provided (optional toggle).
* `REQUEST_INSTALL_PACKAGES` is declared only in debug/editor builds; the
  install flow defers to the system picker (`ACTION_VIEW` with FileProvider)
  which does **not** require the permission on modern Android when using
  `ACTION_INSTALL_PACKAGE` intents from user gesture.

---

## Part C — Milestone mapping

| Capability | Month |
|---|---|
| SAF import (images), texture upload queue, thumbnails | 2 |
| Audio import + playback | 2 |
| Scene save/load + autosave | 2 |
| Prefab serialization | 3 |
| Script assets (Lua) in pipeline | 4 |
| `.anaconda-pack` export format | 5 |
| On-device APK export + wizard UI | 5 |
| AAB/CI export path | 5 |
| Store-ready signing flows, QR install | 6 |
