#include "Lvs/Engine/Rendering/Backends/OpenGL/Utils/GLRenderUtils.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils {

unsigned int CompileShader(const unsigned int shaderType, const std::string& source, const char* stageName) {
    const unsigned int shader = glCreateShader(shaderType);
    const char* srcPtr = source.c_str();
    const int srcLen = static_cast<int>(source.size());
    glShaderSource(shader, 1, &srcPtr, &srcLen);
    glCompileShader(shader);
    int status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status != 0) {
        return shader;
    }

    int logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    std::string log(static_cast<std::size_t>(std::max(logLen, 1)), '\0');
    int written = 0;
    glGetShaderInfoLog(shader, static_cast<int>(log.size()), &written, log.data());
    glDeleteShader(shader);
    throw std::runtime_error(std::string("OpenGL shader compile failed (") + stageName + "): " + log);
}

void ApplyCullMode(const RHI::CullMode mode) {
    switch (mode) {
        case RHI::CullMode::None:
            glDisable(GL_CULL_FACE);
            break;
        case RHI::CullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glFrontFace(GL_CCW);
            break;
        case RHI::CullMode::Back:
        default:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
            break;
    }
}

GLenum ResolveDepthCompare(const RHI::DepthCompare compare) {
    switch (compare) {
        case RHI::DepthCompare::Always:
            return GL_ALWAYS;
        case RHI::DepthCompare::Equal:
            return GL_EQUAL;
        case RHI::DepthCompare::NotEqual:
            return GL_NOTEQUAL;
        case RHI::DepthCompare::Less:
            return GL_LESS;
        case RHI::DepthCompare::Greater:
            return GL_GREATER;
        case RHI::DepthCompare::LessOrEqual:
            return GL_LEQUAL;
        case RHI::DepthCompare::GreaterOrEqual:
        default:
            return GL_GEQUAL;
    }
}

#if !defined(NDEBUG)
void APIENTRY GLDebugMessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
) {
    static_cast<void>(source);
    static_cast<void>(type);
    static_cast<void>(id);
    static_cast<void>(length);
    static_cast<void>(userParam);
    const char* prefix = "[OpenGL][Info]";
    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        prefix = "[OpenGL][Error]";
    } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
        prefix = "[OpenGL][Warning]";
    } else if (severity == GL_DEBUG_SEVERITY_LOW) {
        prefix = "[OpenGL][Low]";
    } else if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) {
        prefix = "[OpenGL][Verbose]";
    }
    std::cerr << prefix << " " << (message != nullptr ? message : "Unknown message") << std::endl;
}
#endif

} // namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils

