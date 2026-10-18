// ============================================================================
//  anaconda/renderer/Shaders.hpp
//
//  Built-in GLSL sources compiled by renderer/Shader.cpp. Embedded as string
//  literals so the native library has ZERO external shader-file plumbing in
//  Phase 1 (no asset loading needed to render the editor scene).
//
//  GLSL 300 es = OpenGL ES 3.0 shading language.
//
//  Sprite program uniforms:
//      uProj   camera projection (clip space mapping)
//      uView   camera view matrix
//      uModel  entity TRS matrix
//      uTexture sampler — white 1x1 by default, asset textures in Phase 2
//
//  Vertex layout (matches renderer/Mesh.hpp Vertex):
//      location 0 : vec3 aPos
//      location 1 : vec4 aColor
//      location 2 : vec2 aUV
// ============================================================================
#pragma once

namespace anaconda::renderer::shaders {

inline constexpr const char* kSpriteVertex = R"GLSL(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aUV;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;

out vec4 vColor;
out vec2 vUV;

void main() {
    vColor = aColor;
    vUV    = aUV;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
)GLSL";

inline constexpr const char* kSpriteFragment = R"GLSL(#version 300 es
precision mediump float;

in vec4 vColor;
in vec2 vUV;

uniform sampler2D uTexture;
uniform vec4 uTint;      // per-entity SpriteComponent.color

out vec4 fragColor;

void main() {
    // Texture modulation × vertex color × per-entity tint. With the default
    // white texture and white vertex colors this reduces to uTint alone —
    // the Phase-1 pure-color sprite path.
    fragColor = texture(uTexture, vUV) * vColor * uTint;
    if (fragColor.a <= 0.0039) discard;  // cheap alpha cutout (0.1% threshold)
}
)GLSL";

}  // namespace anaconda::renderer::shaders
