// ============================================================================
//  anaconda/renderer/Mesh.hpp
//
//  VAO + VBO + EBO owner with the engine's canonical interleaved vertex:
//
//      struct Vertex {
//          vec3 position;   // local space
//          vec4 color;      // tint (white = texture only)
//          vec2 uv;         // texture coordinates
//      }   → 36 bytes, GPU-friendly interleaving (single stream upload)
//
//  Built-in factories: createQuad() (unit quad centered on origin) and
//  createCube() (unit cube) — enough for Phase 1 sprites + Phase-3 gizmos.
//
//  Like all renderer classes: construct inert, call create() ONCE on the GL
//  thread with a current context, destroy with release() on the same thread.
// ============================================================================
#pragma once

#include <GLES3/gl3.h>

#include <cstdint>
#include <vector>

namespace anaconda::renderer {

struct Vertex {
    float px = 0.0f, py = 0.0f, pz = 0.0f;   // location 0 — aPos
    float cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;  // location 1 — aColor
    float u = 0.0f, v = 0.0f;                // location 2 — aUV
};

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /// Upload vertex/index data and build the VAO. false if not on GL thread.
    bool create(const std::vector<Vertex>& vertices,
                const std::vector<std::uint32_t>& indices);

    /// Bind + issue the draw call (glDrawElements, GL_TRIANGLES).
    void draw() const;

    void release();
    [[nodiscard]] bool valid() const noexcept { return vao_ != 0; }
    [[nodiscard]] std::uint32_t indexCount() const noexcept { return indexCount_; }
    /// Debug handle (the VAO name) — for logs only; never bind directly.
    [[nodiscard]] unsigned int id() const noexcept { return vao_; }

    /// Unit quad in the XY plane, centered at origin, UVs (0,0) top-left.
    static Mesh createQuad();

    /// Unit cube centered at origin (Phase-3 3D preview / gizmo handles).
    static Mesh createCube();

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    std::uint32_t indexCount_ = 0;
};

}  // namespace anaconda::renderer
