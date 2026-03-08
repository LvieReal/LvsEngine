#pragma once

#include "Lvs/Engine/RenderingV2/RHI/Format.hpp"
#include "Lvs/Engine/RenderingV2/RHI/Types.hpp"

namespace Lvs::Engine::RenderingV2::RHI {

struct Texture {
    u32 width{0};
    u32 height{0};
    Format format{Format::Unknown};
    union {
        void* graphic_handle_ptr;
        int graphic_handle_i;
    };

    Texture()
        : graphic_handle_ptr(nullptr) {}
};

} // namespace Lvs::Engine::RenderingV2::RHI
