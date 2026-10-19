// ===========================================================================
//  animation/AnimatorSystem.cpp — advance clips, write Sprite frames.
// ===========================================================================
#include "anaconda/animation/AnimatorSystem.hpp"

namespace anaconda::animation {

void AnimatorSystem::update(ecs::Registry& world, float dt) {
    if (dt <= 0.0f) return;
    ecs::view<SpriteComponent, SpriteAnimatorComponent>(
        world, [&](ecs::Entity e, SpriteComponent& sprite, SpriteAnimatorComponent& anim) {
            if (!anim.playing || anim.currentClip.empty()) return;
            const Clip* clip = library.find(anim.currentClip);
            if (clip == nullptr || clip->frameTextures.empty()) return;

            anim.time += dt * anim.speed;
            float frameDur = 1.0f / clip->fps;
            float totalDur = clip->duration();
            if (totalDur <= 0.0f) return;

            if (anim.loop) {
                while (anim.time >= totalDur) anim.time -= totalDur;
                while (anim.time < 0) anim.time += totalDur;
            } else {
                if (anim.time >= totalDur) {
                    anim.time = totalDur - 0.001f;
                    anim.playing = false;
                }
                if (anim.time < 0) anim.time = 0;
            }

            int frame = static_cast<int>(anim.time / frameDur);
            if (frame < 0) frame = 0;
            if (frame >= static_cast<int>(clip->frameTextures.size())) frame = static_cast<int>(clip->frameTextures.size()) - 1;
            anim.currentFrame = frame;

            // Write to sprite: texture handle + UV
            sprite.textureId = clip->frameTextures[frame];
            if (frame < static_cast<int>(clip->frameUVs.size()))
                sprite.uvRect = clip->frameUVs[frame];

            (void)e;
        });
}

}  // namespace anaconda::animation
