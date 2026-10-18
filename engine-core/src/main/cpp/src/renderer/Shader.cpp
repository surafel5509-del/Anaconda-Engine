// ============================================================================
//  renderer/Shader.cpp — compile/link/upload implementation (GL thread only).
// ============================================================================
#include "anaconda/renderer/Shader.hpp"

#include "anaconda/core/Log.hpp"

#include <utility>

namespace anaconda::renderer {

Shader::~Shader() { release(); }

Shader::Shader(Shader&& other) noexcept
    : program_(std::exchange(other.program_, 0)),
      uniformCache_(std::move(other.uniformCache_)) {}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        release();
        program_ = std::exchange(other.program_, 0);
        uniformCache_ = std::move(other.uniformCache_);
    }
    return *this;
}

GLuint Shader::compileStage(GLenum type, const char* src, const char* name) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        ANA_LOGE("[Shader] glCreateShader(%s) failed (no GL context?)", name);
        return 0;
    }

    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        // Log is small; a fixed stack buffer avoids allocation in the failure
        // path (which we want alive even under memory pressure).
        char infoLog[2048] = {0};
        if (logLen > 0) {
            const GLsizei toCopy =
                logLen < static_cast<GLint>(sizeof(infoLog))
                    ? logLen
                    : static_cast<GLint>(sizeof(infoLog)) - 1;
            glGetShaderInfoLog(shader, toCopy, nullptr, infoLog);
        }
        ANA_LOGE("[Shader] %s %s compile error:\n%s",
                 type == GL_VERTEX_SHADER ? "vertex" : "fragment", name, infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::compile(const char* vertexSrc, const char* fragmentSrc,
                     const char* debugName) {
    release();   // re-compiles replace any previous program

    const GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc, debugName);
    if (vs == 0) return false;

    const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc, debugName);
    if (fs == 0) {
        glDeleteShader(vs);
        return false;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    // Shader objects are flagged for deletion once linked into the program —
    // explicit delete keeps driver object tables clean.
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        char infoLog[2048] = {0};
        if (logLen > 0) {
            const GLsizei toCopy =
                logLen < static_cast<GLint>(sizeof(infoLog))
                    ? logLen
                    : static_cast<GLint>(sizeof(infoLog)) - 1;
            glGetProgramInfoLog(program, toCopy, nullptr, infoLog);
        }
        ANA_LOGE("[Shader] %s link error:\n%s", debugName, infoLog);
        glDeleteProgram(program);
        return false;
    }

    program_ = program;
    uniformCache_.clear();
    ANA_LOGI("[Shader] '%s' compiled and linked (program=%u)", debugName,
             program_);
    return true;
}

void Shader::use() const {
    if (program_ != 0) glUseProgram(program_);
}

GLint Shader::location(const char* name) {
    const auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    const GLint loc = glGetUniformLocation(program_, name);
    if (loc < 0) {
        ANA_LOGW("[Shader] uniform '%s' not found (optimized out?)", name);
    }
    uniformCache_.emplace(name, loc);
    return loc;
}

void Shader::setInt(const char* name, int value) {
    const GLint loc = location(name);
    if (loc >= 0) glUniform1i(loc, value);
}
void Shader::setFloat(const char* name, float value) {
    const GLint loc = location(name);
    if (loc >= 0) glUniform1f(loc, value);
}
void Shader::setVec2(const char* name, float x, float y) {
    const GLint loc = location(name);
    if (loc >= 0) glUniform2f(loc, x, y);
}
void Shader::setVec3(const char* name, float x, float y, float z) {
    const GLint loc = location(name);
    if (loc >= 0) glUniform3f(loc, x, y, z);
}
void Shader::setVec4(const char* name, float x, float y, float z, float w) {
    const GLint loc = location(name);
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}
void Shader::setMat4(const char* name, const float* value, int count) {
    const GLint loc = location(name);
    if (loc >= 0 && value != nullptr)
        glUniformMatrix4fv(loc, count, GL_FALSE /* column-major */, value);
}

void Shader::release() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    uniformCache_.clear();
}

}  // namespace anaconda::renderer
