# Contributing to Anaconda-Engine

Thanks for wanting to make the mobile game engine better! 🐍

## Ground rules

1. **Branch from `main`**, open a PR back into `main`.
2. **Native code stays warning-clean** (`-Wall -Wextra`). New math/ECS/physics
   behavior needs a test in `engine-core/src/main/cpp/tests/`.
3. **Keep the layering** — renderer must not include physics headers, Kotlin
   must not grow engine logic (the façade in `NativeEngine.kt` stays thin).
4. **Update the docs** if you change architecture, threading, or the JNI
   contract (`docs/ARCHITECTURE.md` + the KDoc in `NativeEngine.kt` and the
   registration table in `JniBridge.cpp` must stay in sync).

## PR checklist

- [ ] `gradle :app:assembleDebug` succeeds (or CI is green)
- [ ] Host tests pass: `cmake -S engine-core/src/main/cpp -B build-host -DANACONDA_BUILD_TESTS=ON && cmake --build build-host && ctest --test-dir build-host`
- [ ] New files carry a header explaining *why* they exist
- [ ] No secrets, keystores, or `local.properties` committed

## Reporting bugs

Include:

- Device model + GPU (Settings ▸ About phone) and Android version
- `adb logcat -s Anaconda` output around the failure
- Steps to reproduce; for rendering bugs, a screenshot/video helps enormously

## Good first issues

- `good first issue` label in the tracker (device test matrix, docs polish,
  sample content)
- Month-1 polish items in [docs/ROADMAP.md](docs/ROADMAP.md)

## License

By contributing you agree your work is released under the [MIT License](LICENSE).
