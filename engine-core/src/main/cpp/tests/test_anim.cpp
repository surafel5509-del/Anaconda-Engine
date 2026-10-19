// ===========================================================================
//  tests/test_anim.cpp — animator system (host).
// ===========================================================================
#include "anaconda/animation/AnimatorSystem.hpp"
#include "anaconda/ecs/Components.hpp"
#include "test_framework.hpp"

using anaconda::SpriteComponent;
using anaconda::SpriteAnimatorComponent;
namespace ecs = anaconda::ecs;

TEST(animator_advances_frames) {
    ecs::Registry world;
    anaconda::animation::AnimatorSystem anim;
    anim.createSimpleClip("walk", {1,2,3,4}, 10.0f, true);
    ecs::Entity e = world.create();
    world.add(e, anaconda::TagComponent{"Hero"});
    world.add(e, anaconda::TransformComponent{});
    SpriteComponent sp; sp.textureId = 1; world.add(e, sp);
    SpriteAnimatorComponent ac; ac.currentClip = "walk"; ac.playing = true; ac.speed = 1.0f; world.add(e, ac);

    anim.update(world, 0.1f); // 10 fps → 1 frame per 0.1s
    CHECK(world.get<SpriteAnimatorComponent>(e).currentFrame == 1);
    CHECK(world.get<SpriteComponent>(e).textureId == 2u);

    anim.update(world, 0.3f); // +3 frames → frame 0 wrapped? 1+3=4 → wrap to 0
    // total time 0.4s → frame 4 → loop to 0 (4 frames total, duration 0.4s)
    CHECK(world.get<SpriteAnimatorComponent>(e).currentFrame == 0);
}

TEST(animator_one_shot_stops) {
    ecs::Registry world;
    anaconda::animation::AnimatorSystem anim;
    anim.createSimpleClip("jump", {10,20}, 10.0f, false);
    ecs::Entity e = world.create();
    world.add(e, anaconda::TransformComponent{});
    SpriteComponent sp; sp.textureId = 10; world.add(e, sp);
    SpriteAnimatorComponent ac; ac.currentClip = "jump"; ac.loop = false; world.add(e, ac);
    anim.update(world, 0.25f); // 2.5 frames → should clamp to last frame and stop
    CHECK(!world.get<SpriteAnimatorComponent>(e).playing);
    CHECK(world.get<SpriteComponent>(e).textureId == 20u);
}

int main() { return anaconda::test::runAll(); }
