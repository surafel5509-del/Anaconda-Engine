#include "anaconda/script/ScriptEngine.hpp"
#include "anaconda/ecs/Components.hpp"
#include "anaconda/core/Log.hpp"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace anaconda::script {

namespace {
int l_log(lua_State* L) {
    int n = lua_gettop(L);
    std::string msg;
    for (int i = 1; i <= n; ++i) {
        if (i > 1) msg += "\t";
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        msg.append(s, len);
        lua_pop(L, 1);
    }
    ANA_LOGI("[Lua] %s", msg.c_str());
    return 0;
}
int traceback(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (msg) luaL_traceback(L, L, msg, 1);
    else lua_pushliteral(L, "(no error)");
    return 1;
}
void hook(lua_State* L, lua_Debug*) { luaL_error(L, "instruction budget exceeded"); }
}

ScriptEngine::ScriptEngine() = default;
ScriptEngine::~ScriptEngine() { shutdown(); }

bool ScriptEngine::initialize(std::string* error) {
    if (L_) return true;
    L_ = luaL_newstate();
    if (!L_) { if (error) *error = "luaL_newstate failed"; return false; }
    luaL_requiref(L_, "_G", luaopen_base, 1); lua_pop(L_, 1);
    luaL_requiref(L_, "table", luaopen_table, 1); lua_pop(L_, 1);
    luaL_requiref(L_, "string", luaopen_string, 1); lua_pop(L_, 1);
    luaL_requiref(L_, "math", luaopen_math, 1); lua_pop(L_, 1);
    luaL_requiref(L_, "coroutine", luaopen_coroutine, 1); lua_pop(L_, 1);
    luaL_requiref(L_, "utf8", luaopen_utf8, 1); lua_pop(L_, 1);
    lua_pushnil(L_); lua_setglobal(L_, "io");
    lua_pushnil(L_); lua_setglobal(L_, "os");
    lua_pushnil(L_); lua_setglobal(L_, "debug");
    lua_pushcfunction(L_, l_log); lua_setglobal(L_, "log");
    lua_pushcfunction(L_, l_log); lua_setglobal(L_, "print");
    lua_pushcfunction(L_, traceback);
    traceHandlerRef_ = luaL_ref(L_, LUA_REGISTRYINDEX);
    ANA_LOGI("[Script] Lua %s sandbox ready", LUA_VERSION);
    return true;
}
void ScriptEngine::shutdown() {
    if (L_) { lua_close(L_); L_ = nullptr; traceHandlerRef_ = 0; }
    fileCache_.clear();
    entityEnvRefs_.clear();
}
bool ScriptEngine::loadFile(const std::string& path, const std::string& src, std::string* error) {
    fileCache_[path] = src;
    if (!L_) { if (error) *error = "not initialized"; return false; }
    int rc = luaL_loadstring(L_, src.c_str());
    if (rc != LUA_OK) { if (error) *error = lua_tostring(L_, -1); lua_pop(L_, 1); return false; }
    lua_pop(L_, 1);
    return true;
}
bool ScriptEngine::bindEntity(ecs::Entity e, const std::string& path, std::string* error) {
    if (!L_) { if (error) *error = "not init"; return false; }
    auto it = fileCache_.find(path);
    if (it == fileCache_.end()) { if (error) *error = "file not loaded: " + path; return false; }
    // Create env table
    lua_newtable(L_); // env
    lua_pushglobaltable(L_); lua_setmetatable(L_, -2);
    // Load chunk
    int rc = luaL_loadstring(L_, it->second.c_str());
    if (rc != LUA_OK) { if (error) *error = lua_tostring(L_, -1); lua_pop(L_, 2); return false; }
    // stack: env, chunk
    lua_pushvalue(L_, -2); // dup env -> env, chunk, env_copy
    lua_setupvalue(L_, -2, 1); // set chunk's _ENV, pops env_copy -> env, chunk
    // pcall chunk
    rc = lua_pcall(L_, 0, 0, 0);
    if (rc != LUA_OK) { if (error) *error = lua_tostring(L_, -1); lua_pop(L_, 2); return false; }
    // stack: env (now contains onUpdate etc.)
    auto eit = entityEnvRefs_.find(e);
    if (eit != entityEnvRefs_.end()) luaL_unref(L_, LUA_REGISTRYINDEX, eit->second);
    int ref = luaL_ref(L_, LUA_REGISTRYINDEX); // pops env
    entityEnvRefs_[e] = ref;
    return true;
}
void ScriptEngine::update(ecs::Registry& world, float dt) {
    if (!L_) return;
    for (ecs::Entity e : world.aliveEntities()) {
        auto* sc = world.tryGet<ScriptComponent>(e);
        if (!sc || !sc->enabled || sc->assetPath.empty()) continue;
        auto it = entityEnvRefs_.find(e);
        if (it == entityEnvRefs_.end()) {
            std::string err;
            if (!bindEntity(e, sc->assetPath, &err)) { lastError_ = {err, sc->assetPath, 0, e}; sc->lastError = err; continue; }
            it = entityEnvRefs_.find(e);
        }
        lua_rawgeti(L_, LUA_REGISTRYINDEX, it->second); // env
        lua_getfield(L_, -1, "onUpdate");
        if (!lua_isfunction(L_, -1)) { lua_pop(L_, 2); continue; }
        lua_pushnumber(L_, dt);
        lua_sethook(L_, hook, LUA_MASKCOUNT, 1000);
        int rc = lua_pcall(L_, 1, 0, 0);
        lua_sethook(L_, nullptr, 0, 0);
        if (rc != LUA_OK) {
            const char* msg = lua_tostring(L_, -1);
            lastError_ = {msg ? msg : "unknown", sc->assetPath, 0, e};
            sc->lastError = lastError_.message;
            lua_pop(L_, 1);
        }
        lua_pop(L_, 1); // env
    }
}
bool ScriptEngine::hotReload(ecs::Entity e, const std::string& src, std::string* error) {
    std::string path = "hotreload_" + std::to_string(static_cast<uint32_t>(e));
    fileCache_[path] = src;
    auto it = entityEnvRefs_.find(e);
    if (it != entityEnvRefs_.end()) { luaL_unref(L_, LUA_REGISTRYINDEX, it->second); entityEnvRefs_.erase(it); }
    return bindEntity(e, path, error);
}
void ScriptEngine::pushScriptEnv(ecs::Entity, const std::string&) {}
void ScriptEngine::reportError(ecs::Entity) {}
void ScriptEngine::bindApi() {}
bool ScriptEngine::setupSandbox() { return true; }

}
