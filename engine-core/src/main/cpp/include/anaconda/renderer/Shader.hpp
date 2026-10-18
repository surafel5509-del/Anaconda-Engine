// ============================================================================
//  anaconda/renderer/Shader.hpp
//
//  RAII wrapper around a GLES3 shader PROGRAM (vertex + fragment).
//
//  * Move-only — a program is a GL object owned by exactly one owner.
//  * Compile/link failures log the full driver info log (logcat tag
//    "Anaconda") instead of failing silently with program id 0.
//  * Uniform locations are cached: glGetUniformLocation is a string lookup
//    inside most drivers — it is NOT free in tight per-entity loops.
//  * release() detaches ownership without deleting (used before EGL context
//    loss so we don't glDelete against a dead context).
//
//  GL calls here require a current EGL context on the calling thread —
//  the Engine guarantees this by only invoking the renderer from the
//  GLSurfaceView GL thread.
// ============================================================================
#pragma once

#include <GLES3/gl3.h>

#include <string>
#include <unordered_map>

namespace anaconda::renderer {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    /// Compile + link. Returns false on any failure (details in logcat).
    bool compile(const char* vertexSrc, const char* fragmentSrc,
                 const char* debugName = "shader");

    /// Bind for the current draw calls.
    void use() const;

    /// Uniform setters (locations cached; -1 quietly ignored for robustness).
    void setInt(const char* name, int value);
    void setFloat(const char* name, float value);
    void setVec2(const char* name, float x, float y);
    void setVec3(const char* name, float x, float y, float z);
    void setVec4(const char* name, float x, float y, float z, float w);
    /// count = 1 for a single mat4; GL_FALSE transpose (column-major input).
    void setMat4(const char* name, const float* value, int count = 1);

    [[nodiscard]] GLuint id() const noexcept { return program_; }
    [[nodiscard]] bool valid() const noexcept { return program_ != 0; }

    /// Delete GL object (if any) and drop cached locations.
    void release();

private:
    GLint location(const char* name);
    static GLuint compileStage(GLenum type, const char* src, const char* name);

    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> uniformCache_;
};

}  // namespace anaconda::renderer
