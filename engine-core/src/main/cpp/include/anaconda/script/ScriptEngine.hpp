// ===========================================================================
//  anaconda/script/ScriptEngine.hpp — Lua 5.4 sandbox (Month 4).
//
//  One lua_State per scene, opened with restricted stdlib (base/table/string/
//  math/coroutine only — io/os/debug stripped). API surface is table-based
//  binder: log, transform get/set, time, spawn/destroy, input, audio, timers,
//  math helpers, ecs.query. Instruction budget via hook every 1000 ops.
//
//  Threading: all Lua execution on GL/frame thread (Engine::renderFrame).
//  UI thread never touches lua_State*; script saves enqueue reload command.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Ecs.hpp"

#include <functional>
#include <string>
#include <unordered_map>

struct lua_State;

namespace anaconda::script {

struct ScriptError {
    std::string message;
    std::string file;
    int line = 0;
    ecs::Entity entity = ecs::kNullEntity;
};

// Lightweight entity userdata tag.
struct ScriptEntityRef {
    ecs::Entity id = ecs::kNullEntity;
};

class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    // Lifecycle — called from Engine (GL thread).
    bool initialize(std::string* error = nullptr);
    void shutdown();
    bool isInitialized() const { return L_ != nullptr; }

    // Load a script file's source into the engine's cache (no execution yet).
    bool loadFile(const std::string& assetPath, const std::string& source,
                  std::string* error = nullptr);

    // Bind a script to an entity (creates per-entity env table, runs chunk).
    bool bindEntity(ecs::Entity e, const std::string& assetPath,
                    std::string* error = nullptr);

    // Per-frame: call onUpdate(dt) for each enabled ScriptComponent.
    void update(ecs::Registry& world, float dt);

    // Hot reload: re-execute chunk in existing env table (vars persist).
    bool hotReload(ecs::Entity e, const std::string& newSource,
                   std::string* error = nullptr);

    // Error handling: last error for console panel.
    const ScriptError& lastError() const { return lastError_; }
    void clearError() { lastError_ = {}; }

    // API bound to Lua (implemented in .cpp).
    void bindApi();

    // Direct Lua state access for advanced embedding / tests.
    lua_State* state() { return L_; }

    // Instruction budget (default 1M ops/frame per script).
    void setInstructionBudget(int ops) { instructionBudget_ = ops; }

    // Console capture: Lua print/log → C++ callback → JNI → UI console.
    using LogCallback = std::function<void(const std::string& msg)>;
    void setLogCallback(LogCallback cb) { logCallback_ = std::move(cb); }

private:
    bool setupSandbox();
    void pushScriptEnv(ecs::Entity e, const std::string& assetPath);
    void reportError(ecs::Entity e);

    lua_State* L_ = nullptr;
    int traceHandlerRef_ = 0;
    int instructionBudget_ = 1'000'000;
    ScriptError lastError_;
    LogCallback logCallback_;
    std::unordered_map<std::string, std::string> fileCache_; // path → source
    std::unordered_map<ecs::Entity, int> entityEnvRefs_;     // entity → LUA_REGISTRYINDEX ref
};

}  // namespace anaconda::script
