// ============================================================================
//  renderer/Mesh.cpp — VAO/VBO/EBO management (GL thread only).
// ============================================================================
#include "anaconda/renderer/Mesh.hpp"

#include "anaconda/core/Log.hpp"

#include <cstddef>
#include <utility>

namespace anaconda::renderer {

Mesh::~Mesh() { release(); }

Mesh::Mesh(Mesh&& other) noexcept
    : vao_(std::exchange(other.vao_, 0)),
      vbo_(std::exchange(other.vbo_, 0)),
      ebo_(std::exchange(other.ebo_, 0)),
      indexCount_(std::exchange(other.indexCount_, 0)) {}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        vao_ = std::exchange(other.vao_, 0);
        vbo_ = std::exchange(other.vbo_, 0);
        ebo_ = std::exchange(other.ebo_, 0);
        indexCount_ = std::exchange(other.indexCount_, 0);
    }
    return *this;
}

bool Mesh::create(const std::vector<Vertex>& vertices,
                  const std::vector<std::uint32_t>& indices) {
    release();

    if (vertices.empty() || indices.empty()) {
        ANA_LOGE("[Mesh] create() called with empty buffers");
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    // ---- vertex buffer (single interleaved stream) ------------------------
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(), GL_STATIC_DRAW);

    // ---- index buffer ------------------------------------------------------
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(), GL_STATIC_DRAW);

    // ---- attribute layout (must match Shaders.hpp) ------------------------
    // location 0: aPos   vec3 @ 0 bytes
    // location 1: aColor vec4 @ 12 bytes
    // location 2: aUV    vec2 @ 28 bytes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, cr)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, u)));

    glBindVertexArray(0);   // unbind; EBO binding lives on inside the VAO

    indexCount_ = static_cast<std::uint32_t>(indices.size());
    ANA_LOGI("[Mesh] created VAO=%u VBO=%u EBO=%u (%zu verts, %u indices)", vao_,
             vbo_, ebo_, vertices.size(), indexCount_);
    return true;
}

void Mesh::draw() const {
    if (vao_ == 0 || indexCount_ == 0) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::release() {
    if (ebo_ != 0) glDeleteBuffers(1, &ebo_);
    if (vbo_ != 0) glDeleteBuffers(1, &vbo_);
    if (vao_ != 0) glDeleteVertexArrays(1, &vao_);
    ebo_ = vbo_ = vao_ = 0;
    indexCount_ = 0;
}

Mesh Mesh::createQuad() {
    Mesh mesh;
    // Unit quad centered at origin. UV origin = TOP-LEFT (image convention):
    //   (-0.5, -0.5) is bottom-left of the sprite → uv (0, 1)
    const std::vector<Vertex> vertices = {
        //  px     py     pz     cr  cg  cb  ca   u    v
        {-0.5f, -0.5f, 0.0f,  1, 1, 1, 1,  0.0f, 1.0f},   // 0 bottom-left
        { 0.5f, -0.5f, 0.0f,  1, 1, 1, 1,  1.0f, 1.0f},   // 1 bottom-right
        {-0.5f,  0.5f, 0.0f,  1, 1, 1, 1,  0.0f, 0.0f},   // 2 top-left
        { 0.5f,  0.5f, 0.0f,  1, 1, 1, 1,  1.0f, 0.0f},   // 3 top-right
    };
    const std::vector<std::uint32_t> indices = {
        0, 1, 3,   // lower-right triangle
        0, 3, 2,   // upper-left triangle
    };
    mesh.create(vertices, indices);
    return mesh;
}

Mesh Mesh::createCube() {
    Mesh mesh;
    const float h = 0.5f;
    // 24 vertices (per-face normals/uvs ready for Phase-4 lighting).
    const std::vector<Vertex> v = {
        // +Z front
        {-h, -h,  h, 1,1,1,1, 0,1}, { h, -h,  h, 1,1,1,1, 1,1},
        { h,  h,  h, 1,1,1,1, 1,0}, {-h,  h,  h, 1,1,1,1, 0,0},
        // -Z back
        { h, -h, -h, 1,1,1,1, 0,1}, {-h, -h, -h, 1,1,1,1, 1,1},
        {-h,  h, -h, 1,1,1,1, 1,0}, { h,  h, -h, 1,1,1,1, 0,0},
        // +X right
        { h, -h,  h, 1,1,1,1, 0,1}, { h, -h, -h, 1,1,1,1, 1,1},
        { h,  h, -h, 1,1,1,1, 1,0}, { h,  h,  h, 1,1,1,1, 0,0},
        // -X left
        {-h, -h, -h, 1,1,1,1, 0,1}, {-h, -h,  h, 1,1,1,1, 1,1},
        {-h,  h,  h, 1,1,1,1, 1,0}, {-h,  h, -h, 1,1,1,1, 0,0},
        // +Y top
        {-h,  h,  h, 1,1,1,1, 0,1}, { h,  h,  h, 1,1,1,1, 1,1},
        { h,  h, -h, 1,1,1,1, 1,0}, {-h,  h, -h, 1,1,1,1, 0,0},
        // -Y bottom
        {-h, -h, -h, 1,1,1,1, 0,1}, { h, -h, -h, 1,1,1,1, 1,1},
        { h, -h,  h, 1,1,1,1, 1,0}, {-h, -h,  h, 1,1,1,1, 0,0},
    };
    std::vector<std::uint32_t> indices;
    indices.reserve(36);
    for (std::uint32_t face = 0; face < 6; ++face) {
        const std::uint32_t b = face * 4;
        indices.insert(indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    mesh.create(v, indices);
    return mesh;
}

}  // namespace anaconda::renderer
