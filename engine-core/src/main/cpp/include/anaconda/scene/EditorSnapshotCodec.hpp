// ============================================================================
//  anaconda/scene/EditorSnapshotCodec.hpp
//
//  The editor's line protocol — NOW A FIRST-CLASS, HOST-TESTABLE CODEC.
//
//  One line per entity:
//      id|name|px,py,pz|rx,ry,rz|sx,sy,sz|flags|cr,cg,cb,ca|textureHandle|renderLayer
//      flags: bit0 sprite · bit1 rigidbody · bit2 box collider
//  renderLayer (9th column) defaults to 0 for backward compatibility with
//  Month-2 8-column snapshots.
//
//  Used by BOTH directions:
//    * Engine::sceneSnapshot()      → writeEntity()  (engine → UI)
//    * Engine::restoreEntity(line)  → parseEntity()  (UI → engine)
//      which powers undo-of-delete: the Inspector/Hierarchy already holds
//      the exact line, so restoration rebuilds EVERY component the line
//      describes — no separate "addComponent" API needed.
//
//  Rotation: RADIANS (engine canonical). Colors: [0,1]. Texture field is the
//  AssetManager handle (0 = white).
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

#include "anaconda/ecs/Ecs.hpp"

namespace anaconda::scene {

class EditorSnapshotCodec {
public:
    /// Serialize one live entity. Returns "" when the handle is invalid or
    /// the entity lacks a Transform (renderables without transforms are
    /// invisible to the editor protocol by design).
    static std::string writeEntity(const ecs::Registry& world,
                                   ecs::Entity entity);

    /// Recreate an entity from a line written by writeEntity(). The line's
    /// id field is IGNORED (a new id is minted — restoration never collides
    /// with live handles). On failure returns kNullEntity and sets `error`.
    static ecs::Entity parseEntity(ecs::Registry& world, const std::string& line,
                                   std::string* error = nullptr);
};

}  // namespace anaconda::scene
