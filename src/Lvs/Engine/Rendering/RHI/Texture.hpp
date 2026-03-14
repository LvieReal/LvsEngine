#pragma once

#include "Lvs/Engine/Rendering/RHI/Format.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace Lvs::Engine::Rendering::RHI {

enum class TextureType {
    Texture2D,
    Texture3D,
    TextureCube
};

struct CubemapDesc {
    u32 width{0};
    u32 height{0};
    Format format{Format::R8G8B8A8_UNorm};
    bool linearFiltering{true};
    std::array<std::vector<std::uint8_t>, 6> faces{};
};

struct Texture2DDesc {
    u32 width{0};
    u32 height{0};
    Format format{Format::R8G8B8A8_UNorm};
    bool linearFiltering{true};
    bool generateMipmaps{false};
    std::vector<std::uint8_t> pixels{};
};

struct Texture3DDesc {
    u32 width{0};
    u32 height{0};
    u32 depth{0};
    Format format{Format::R8G8B8A8_UNorm};
    bool linearFiltering{false};
    bool repeat{true};
    std::vector<std::uint8_t> pixels{};
};

struct Texture {
    u32 width{0};
    u32 height{0};
    u32 depth{1};
    Format format{Format::Unknown};
    TextureType type{TextureType::Texture2D};
    union {
        void* graphic_handle_ptr;
        int graphic_handle_i;
    };
    void* sampler_handle_ptr{nullptr};

    Texture()
        : graphic_handle_ptr(nullptr) {}
};

} // namespace Lvs::Engine::Rendering::RHI
