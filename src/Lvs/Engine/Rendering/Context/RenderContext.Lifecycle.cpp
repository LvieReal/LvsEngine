#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"
#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"
#include "Lvs/Engine/Core/ExternalMetadata.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"

#include <cstring>

namespace Lvs::Engine::Rendering {

RenderContext::RenderContext(const RenderApi preferredApi)
    : preferredApi_(preferredApi),
      activeApi_(Context::ResolveApi(preferredApi_)) {}

RenderContext::~RenderContext() {
    WaitForBackendIdle();
    ReleaseGpuResources();
    vkBackend_.reset();
    glBackend_.reset();
}

void RenderContext::Initialize(const RHI::u32 width, const RHI::u32 height) {
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (nativeWindowHandle_ == nullptr) {
        return;
    }
    EnsureBackend();
    if (vkBackend_ != nullptr) {
        vkBackend_->Initialize(width, height);
    } else if (glBackend_ != nullptr) {
        glBackend_->Initialize(width, height);
    } else {
        throw RenderingInitializationError("No render backend available");
    }
    InitializeGeometryBuffers();
}

void RenderContext::AttachToNativeWindow(void* nativeWindowHandle, const RHI::u32 width, const RHI::u32 height) {
    WaitForBackendIdle();
    ReleaseGpuResources();
    nativeWindowHandle_ = nativeWindowHandle;
    vkApi_.NativeWindowHandle = nativeWindowHandle_;
    glApi_.NativeWindowHandle = nativeWindowHandle_;
    glApi_.ContextHandle = nullptr;
    vkBackend_.reset();
    glBackend_.reset();
    Initialize(width, height);
}

void RenderContext::Resize(const RHI::u32 width, const RHI::u32 height) {
    surfaceWidth_ = width;
    surfaceHeight_ = height;
    if (nativeWindowHandle_ == nullptr) {
        return;
    }
    EnsureBackend();
    if (vkBackend_ != nullptr) {
        vkBackend_->Resize(width, height);
    } else if (glBackend_ != nullptr) {
        glBackend_->Resize(width, height);
    }
}

void RenderContext::SetClearColor(const float r, const float g, const float b, const float a) {
    clearColor_[0] = r;
    clearColor_[1] = g;
    clearColor_[2] = b;
    clearColor_[3] = a;
}

void RenderContext::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    if (metadataRoot_ != nullptr) {
        Core::ExternalMetadata::Get().UnregisterRoot(metadataRoot_);
        metadataRoot_.reset();
    }
    place_ = place;
    if (place_ != nullptr) {
        metadataRoot_ = place_->GetDataModel();
        if (metadataRoot_ != nullptr) {
            Core::ExternalMetadata::Get().RegisterRoot(metadataRoot_);
        }
    }
    ClearGeometryCache();
}

void RenderContext::Unbind() {
    if (metadataRoot_ != nullptr) {
        Core::ExternalMetadata::Get().UnregisterRoot(metadataRoot_);
        metadataRoot_.reset();
    }
    place_.reset();
    overlayPrimitives_.clear();
    image3dPrimitives_.clear();
    ClearGeometryCache();
}

void RenderContext::SetOverlayPrimitives(std::vector<Common::OverlayPrimitive> primitives) {
    LVS_BENCH_SCOPE("RenderContext::SetOverlayPrimitives");
    overlayPrimitives_ = std::move(primitives);

    auto hashCombine = [](std::size_t& seed, const std::size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    };

    std::size_t key = 0U;
    {
        LVS_BENCH_SCOPE("RenderContext::SetOverlayPrimitives(Hash)");
        hashCombine(key, overlayPrimitives_.size());
        for (const auto& overlay : overlayPrimitives_) {
            const auto rows = overlay.Model.Rows();
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    const float v = static_cast<float>(rows[r][c]);
                    std::uint32_t bits = 0U;
                    static_assert(sizeof(bits) == sizeof(v));
                    std::memcpy(&bits, &v, sizeof(bits));
                    hashCombine(key, static_cast<std::size_t>(bits));
                }
            }
            hashCombine(key, static_cast<std::size_t>(overlay.Shape));
            hashCombine(key, static_cast<std::size_t>(overlay.AlwaysOnTop));
            hashCombine(key, static_cast<std::size_t>(overlay.IgnoreLighting));

            const float cr = static_cast<float>(overlay.Color.r);
            const float cg = static_cast<float>(overlay.Color.g);
            const float cb = static_cast<float>(overlay.Color.b);
            std::uint32_t cBits = 0U;
            std::memcpy(&cBits, &cr, sizeof(cBits));
            hashCombine(key, static_cast<std::size_t>(cBits));
            std::memcpy(&cBits, &cg, sizeof(cBits));
            hashCombine(key, static_cast<std::size_t>(cBits));
            std::memcpy(&cBits, &cb, sizeof(cBits));
            hashCombine(key, static_cast<std::size_t>(cBits));

            const float metalness = overlay.Metalness;
            const float roughness = overlay.Roughness;
            const float emissive = overlay.Emissive;
            std::uint32_t mBits = 0U;
            std::memcpy(&mBits, &metalness, sizeof(mBits));
            hashCombine(key, static_cast<std::size_t>(mBits));
            std::memcpy(&mBits, &roughness, sizeof(mBits));
            hashCombine(key, static_cast<std::size_t>(mBits));
            std::memcpy(&mBits, &emissive, sizeof(mBits));
            hashCombine(key, static_cast<std::size_t>(mBits));

            const float alpha = overlay.Alpha;
            std::uint32_t alphaBits = 0U;
            std::memcpy(&alphaBits, &alpha, sizeof(alphaBits));
            hashCombine(key, static_cast<std::size_t>(alphaBits));
        }
    }

    if (key != overlayCacheKey_) {
        overlayCacheKey_ = key;
        overlayDirty_ = true;
        instanceBufferDirty_ = true;
    }
}

void RenderContext::SetImage3DPrimitives(std::vector<Common::Image3DPrimitive> primitives) {
    LVS_BENCH_SCOPE("RenderContext::SetImage3DPrimitives");
    image3dPrimitives_ = std::move(primitives);

    auto hashCombine = [](std::size_t& seed, const std::size_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    };

    auto hashFloat = [&hashCombine](std::size_t& seed, const float v) {
        std::uint32_t bits = 0U;
        static_assert(sizeof(bits) == sizeof(v));
        std::memcpy(&bits, &v, sizeof(bits));
        hashCombine(seed, static_cast<std::size_t>(bits));
    };

    std::size_t key = 0U;
    hashCombine(key, image3dPrimitives_.size());
    for (const auto& img : image3dPrimitives_) {
        hashFloat(key, static_cast<float>(img.Position.x));
        hashFloat(key, static_cast<float>(img.Position.y));
        hashFloat(key, static_cast<float>(img.Position.z));
        hashFloat(key, static_cast<float>(img.Size));
        hashFloat(key, static_cast<float>(img.Tint.r));
        hashFloat(key, static_cast<float>(img.Tint.g));
        hashFloat(key, static_cast<float>(img.Tint.b));
        hashFloat(key, img.Alpha);
        hashCombine(key, std::hash<std::string>{}(img.ContentId));
        hashCombine(key, static_cast<std::size_t>(img.ResolutionCap));
        hashCombine(key, img.FollowCamera ? 1U : 0U);
        hashCombine(key, img.ConstantSize ? 1U : 0U);
        hashCombine(key, img.AlwaysOnTop ? 1U : 0U);
        hashFloat(key, static_cast<float>(img.MaxDistance));
    }

    if (key != image3dCacheKey_) {
        image3dCacheKey_ = key;
        image3dDirty_ = true;
    }
}

void RenderContext::RefreshShaders() {
    refreshShadersRequested_ = true;
}

} // namespace Lvs::Engine::Rendering
