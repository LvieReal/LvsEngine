#pragma once

#include "Lvs/Engine/RenderingV2/RHI/Types.hpp"

namespace Lvs::Engine::RenderingV2::Backends::OpenGL {

struct GLApi {
    using ActiveTextureFn = void (*)(unsigned int);
    using BindTextureFn = void (*)(unsigned int, unsigned int);
    using DrawElementsFn = void (*)(unsigned int, int, unsigned int, const void*);
    using UseProgramFn = void (*)(unsigned int);

    ActiveTextureFn ActiveTexture{nullptr};
    BindTextureFn BindTexture{nullptr};
    DrawElementsFn DrawElements{nullptr};
    UseProgramFn UseProgram{nullptr};
    unsigned int Texture0Enum{0x84C0};   // GL_TEXTURE0
    unsigned int Texture2DEnum{0x0DE1};  // GL_TEXTURE_2D
    unsigned int TrianglesEnum{0x0004};  // GL_TRIANGLES
    unsigned int UnsignedIntEnum{0x1405}; // GL_UNSIGNED_INT
};

} // namespace Lvs::Engine::RenderingV2::Backends::OpenGL
