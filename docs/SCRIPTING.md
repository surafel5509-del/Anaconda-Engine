# Anaconda-Engine — In-App Scripting Design (Phase 4)

> Goal: let users write **game logic inside the app**, on-device, with instant
> feedback — without leaving the editor.

---

## 1. Options compared

| Criterion | **Lua 5.4** | JavaScript (V8 / JSC) | Visual node graph |
|---|---|---|---|
| Binary size added | **~300 KB** | ~9–20 MB (V8), ~6 MB (JSC) | 0 (own VM) |
| Integration cost | C API, trivial, proven (Roblox LÖVE, Defold, WoW) | Huge embed surface, JIT warmup, sandbox harder | Full custom compiler → VM |
| Sandbox / security | Excellent (strip `io`/`os`, custom alloc, instruction budget) | Weak (need full realm isolation) | Excellent by construction |
| Hot reload | Trivial (reload chunk, re-bind env) | Possible but state serialization painful | Natural (graph diff) |
| Learning curve | Very low; reads like pseudocode | Lowest (ubiquitous) | Lowest for non-coders, **high for us** |
| Perf for gameplay logic | ~1–5M ops/sec (plenty) | Excellent | Depends on VM |
| Mobile on-device editing UX | Small keyboard-friendly syntax | Same | Best for tiny logic, bad for real math/logic |

**Decision: Lua 5.4 as the primary scripting language**, with the **visual
node system as a Phase 6+ layer that *compiles* to the same Lua backend**
(nodes → generated Lua → one runtime, one debugging story).

Rejected for Phase 4: V8 (size + sandbox + build complexity), Hermes (RN
coupling), Wasmtime (overkill for game scripts).

---

## 2. Architecture

```
┌───────────────────── :editor-ui ─────────────────────┐
│ ScriptEditorPanel (highlight, diagnostics, run)      │
│ ConsolePanel (print/log + Lua errors, line mapped)   │
└────────────────────────┬─────────────────────────────┘
                         │ save script bytes / execute commands
┌────────────────────────▼─────────────────────────────┐
│ :engine-core  →  anaconda_script (STATIC, Phase 4)   │
│                                                          │
│  ScriptEngine                                           │
│   ├─ lua_State* (one per scene, opened with restricted  │
│   │   stdlib: base/table/string/math/coroutine only)    │
│   ├─ registry: entity ↔ userdata handles                │
│   ├─ per-entity environment table (_ENV with script     │
│   │   functions: onLoad/onUpdate/onTouch/onDestroy)     │
│   ├─ error handler → C++ trace → JNI callback → console │
│   └─ instruction budget (hook every 1000 ops) → hang    │
│        protection for runaway `while true do end`       │
│                                                          │
│  API surface (bound with a small table-based binder):    │
│   log(msg)                     input.isTouching()       │
│   entity:getPos() / setPos()   input.touchPos()         │
│   entity:getRot()/setScale()   audio.play(handle)       │
│   spawn(prefab, pos)           scene.load(name)         │
│   destroy(entity)              timer.after(sec, fn)     │
│   vec2/vec3 helpers, math.*    ecs.query("Tag")         │
└────────────────────────────────────────────────────────┘
```

### Lifecycle contract (mirrors Unity/Defamiliar patterns)

```lua
-- scripts/player.lua  — one file per entity, attached via ScriptComponent
function onLoad()      -- called once when the scene starts
    speed = 6.0
    rb = entity:getRigidBody()
end

function onUpdate(dt)  -- every fixed physics tick while Play is active
    if input.isTouching() then
        rb:setVelocity(input.touchDelta() * speed)
    end
end

function onTouch(event) ... end
function onDestroy()   ... end
```

---

## 3. C++ ↔ Lua binding sketch (the core of the bridge)

```cpp
// engine-core/src/script/ScriptEngine.cpp  (Phase 4)
namespace anaconda::script {

// ---- lightweight entity userdata -------------------------------------------
static Entity* checkEntity(lua_State* L, int idx) {
    return static_cast<Entity*>(luaL_checkudata(L, idx, "Anaconda.Entity"));
}

// Lua: pos = entity:getPos()  →  returns x, y, z
static int l_entity_getPos(lua_State* L) {
    Entity* e = checkEntity(L, 1);
    auto& t  = g_engine->world().get<TransformComponent>(*e);
    lua_pushnumber(L, t.position.x);
    lua_pushnumber(L, t.position.y);
    lua_pushnumber(L, t.position.z);
    return 3;                       // number of Lua return values
}

// Lua: entity:setPos(x, y, z)
static int l_entity_setPos(lua_State* L) {
    Entity* e = checkEntity(L, 1);
    auto& t  = g_engine->world().get<TransformComponent>(*e);
    t.position = { (float)luaL_checknumber(L, 2),
                   (float)luaL_checknumber(L, 3),
                   (float)luaL_checknumber(L, 4) };
    return 0;
}

static const luaL_Reg kEntityMethods[] = {
    {"getPos",  l_entity_getPos },
    {"setPos",  l_entity_setPos },
    {"getRot",  l_entity_getRot },
    {"destroy", l_entity_destroy},
    {nullptr, nullptr}
};

void ScriptEngine::bindEntityMetatable() {
    luaL_newmetatable(L_, "Anaconda.Entity");     // metatable name
    lua_pushvalue(L_, -1);
    lua_setfield(L_, -2, "__index");              // methods live in the mt
    luaL_setfuncs(L_, kEntityMethods, 0);
    lua_pop(L_, 1);
}

// ---- per-frame driver -------------------------------------------------------
void ScriptEngine::update(Registry& world, float dt) {
    for (auto [e, script] : View<ScriptComponent>(world)) {
        if (!script.enabled) continue;
        pushScriptEnv(e, script.assetPath);       // loads/caches chunk
        lua_getfield(L_, -1, "onUpdate");         // fn on stack
        if (lua_isfunction(L_, -1)) {
            pushEntity(L_, e);                    // self argument
            lua_pushnumber(L_, dt);
            if (lua_pcall(L_, 2, 0, traceHandler_) != LUA_OK)
                reportErrorToEditor(e);           // → JNI → Console panel
        } else lua_pop(L_, 1);
        lua_pop(L_, 1);                           // env
    }
}

} // namespace anaconda::script
```

**Threading rule:** all Lua execution happens on the **GL/frame thread**
inside `Engine::renderFrame()` (after physics, before or after render —
decided as: *script update → physics step → render*, fixed 60 Hz accumulator).
The UI thread never touches `lua_State*`; script editor “Run” actions enqueue
a reload command (same channel as texture uploads, Phase 2+).

---

## 4. Editor-side experience (Phase 4 UI)

1. **Script asset creation** — Asset Browser → `+` → Script → template
   picker (Empty / Player controller / Collectible / Spawner).
2. **Inline editor** — Compose `BasicTextField` with Kotlin
   `VisualTransformation` for Lua syntax highlighting (keyword/string/number/
   comment spans), line numbers gutter, error squiggles fed from Lua
   `traceback`.
3. **Attach** — Inspector → `+ Add Component → Script` → file picker over
   `project/scripts/`.
4. **Console panel** — captures `print()` and runtime errors with entity name
   + line: `Player.lua:14: attempt to index nil (field 'rb') [entity #7]`.
5. **Safety** — scripts run under an instruction hook; infinite loops hit
   `ANA_LUA_BUDGET` (default 1M instructions/frame) → error + auto-disable of
   that script, game keeps running.

---

## 5. Migration / versioning

* Lua source files are committed as text in the project folder → they version
  cleanly with git and survive engine upgrades.
* The binding API is versioned (`apiVersion = 4` inside project.json); the
  ScriptEngine refuses projects written for a newer API with a clear message.
* Hot reload: on script save, re-execute chunk in the *same* environment table
  so user variables persist unless the function set changed (delta detection).

---

## 6. Phasing

| Phase | Content |
|---|---|
| Phase 4 (Month 4) | Lua embed, entity/transform/input API, in-app editor, console, hot reload |
| Phase 5 (Month 5) | Exported player includes Lua runtime (scripts ship pre-compiled to bytecode for size/speed) |
| Phase 6+ (stretch) | Visual node editor compiling to Lua; typed-script linting; debugger (breakpoints via hook) |
