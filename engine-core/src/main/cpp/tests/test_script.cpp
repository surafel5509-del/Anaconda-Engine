// ===========================================================================
//  tests/test_script.cpp — Lua sandbox (host).
// ===========================================================================
#include "anaconda/script/ScriptEngine.hpp"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#include "anaconda/ecs/Components.hpp"
#include "test_framework.hpp"

using anaconda::ScriptComponent;
namespace ecs = anaconda::ecs;

TEST(script_basic_load_and_update) {
    anaconda::script::ScriptEngine engine;
    CHECK(engine.initialize());
    ecs::Registry world;
    ecs::Entity e = world.create();
    ScriptComponent sc; sc.assetPath = "test.lua"; sc.enabled = true; world.add(e, sc);
    std::string src = "value = 0\nfunction onUpdate(dt) value = value + dt end";
    CHECK(engine.loadFile("test.lua", src));
    CHECK(engine.bindEntity(e, "test.lua"));
    engine.update(world, 0.5f);
    CHECK(engine.lastError().message.empty());
    engine.update(world, 0.5f);
    CHECK(engine.lastError().message.empty());
    engine.shutdown();
}

TEST(script_syntax_error_reported) {
    anaconda::script::ScriptEngine engine;
    engine.initialize();
    std::string err;
    CHECK(!engine.loadFile("bad.lua", "this is not lua [[[", &err));
    CHECK(!err.empty());
    engine.shutdown();
}

TEST(script_sandbox_blocks_io) {
    anaconda::script::ScriptEngine engine;
    engine.initialize();
    ecs::Registry world;
    ecs::Entity e = world.create();
    ScriptComponent sc; sc.assetPath = "io.lua"; world.add(e, sc);
    std::string src = "if io ~= nil then error('io not nil') end";
    CHECK(engine.loadFile("io.lua", src));
    std::string err;
    bool ok = engine.bindEntity(e, "io.lua", &err);
    CHECK(ok);
    if (!ok) { CHECK(err.empty()); }
    engine.shutdown();
}

int main() { return anaconda::test::runAll(); }
