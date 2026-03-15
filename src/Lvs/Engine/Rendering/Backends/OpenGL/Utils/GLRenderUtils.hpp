#pragma once

#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <glad/glad.h>

#include <string>

namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils {

unsigned int CompileShader(unsigned int shaderType, const std::string& source, const char* stageName);
void ApplyCullMode(RHI::CullMode mode);
GLenum ResolveDepthCompare(RHI::DepthCompare compare);

#if !defined(NDEBUG)
void APIENTRY GLDebugMessageCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
);
#endif

} // namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils

