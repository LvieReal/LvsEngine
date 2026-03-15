#pragma once

#include "Lvs/Engine/Rendering/RHI/IBuffer.hpp"
#include "Lvs/Engine/Rendering/RHI/IRenderTarget.hpp"
#include "Lvs/Engine/Rendering/RHI/IResourceSet.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils {

class GLResourceSet final : public RHI::IResourceSet {
public:
    explicit GLResourceSet(RHI::u32 id);
    [[nodiscard]] void* GetNativeHandle() const override;

private:
    RHI::u32 id_{0U};
};

class GLBuffer final : public RHI::IBuffer {
public:
    GLBuffer(unsigned int handle, std::size_t size);
    ~GLBuffer() override;

    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] std::size_t GetSize() const override;

private:
    unsigned int handle_{0U};
    std::size_t size_{0};
};

class GLRenderTarget final : public RHI::IRenderTarget {
public:
    GLRenderTarget(
        RHI::u32 width,
        RHI::u32 height,
        unsigned int drawFramebuffer,
        unsigned int resolveFramebuffer,
        const std::vector<unsigned int>& colorTextures,
        const std::vector<unsigned int>& msaaColorRenderbuffers,
        unsigned int depthRenderbuffer,
        unsigned int depthTexture,
        unsigned int msaaDepthRenderbuffer,
        RHI::Format colorFormat,
        RHI::Format depthFormat,
        RHI::u32 sampleCount,
        std::function<void(unsigned int)> unregisterMsaa
    );
    ~GLRenderTarget() override;

    [[nodiscard]] void* GetRenderPassHandle() const override;
    [[nodiscard]] void* GetFramebufferHandle() const override;
    [[nodiscard]] RHI::u32 GetWidth() const override;
    [[nodiscard]] RHI::u32 GetHeight() const override;
    [[nodiscard]] RHI::u32 GetColorAttachmentCount() const override;
    [[nodiscard]] RHI::u32 GetSampleCount() const override;
    [[nodiscard]] RHI::Texture GetColorTexture(RHI::u32 index) const override;
    [[nodiscard]] bool HasDepth() const override;
    [[nodiscard]] RHI::Texture GetDepthTexture() const override;

private:
    RHI::u32 width_{0U};
    RHI::u32 height_{0U};
    unsigned int drawFramebuffer_{0U};
    unsigned int resolveFramebuffer_{0U};
    std::vector<unsigned int> colorTextures_{};
    std::vector<unsigned int> msaaColorRenderbuffers_{};
    unsigned int depthRenderbuffer_{0U};
    unsigned int depthTexture_{0U};
    unsigned int msaaDepthRenderbuffer_{0U};
    RHI::Format colorFormat_{RHI::Format::R8G8B8A8_UNorm};
    RHI::Format depthFormat_{RHI::Format::D32_Float};
    std::vector<RHI::Texture> colorTextureViews_{};
    bool hasDepthTexture_{false};
    RHI::Texture depthTextureView_{};
    RHI::u32 sampleCount_{1U};
    std::function<void(unsigned int)> unregisterMsaa_{};
};

} // namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils

