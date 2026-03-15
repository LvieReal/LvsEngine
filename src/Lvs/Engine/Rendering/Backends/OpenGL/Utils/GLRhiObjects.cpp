#include "Lvs/Engine/Rendering/Backends/OpenGL/Utils/GLRhiObjects.hpp"

#include <glad/glad.h>

#include <utility>

namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils {

GLResourceSet::GLResourceSet(const RHI::u32 id)
    : id_(id) {}

void* GLResourceSet::GetNativeHandle() const {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id_));
}

GLBuffer::GLBuffer(const unsigned int handle, const std::size_t size)
    : handle_(handle),
      size_(size) {}

GLBuffer::~GLBuffer() {
    if (handle_ != 0U) {
        glDeleteBuffers(1, &handle_);
    }
}

void* GLBuffer::GetNativeHandle() const {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(handle_));
}

std::size_t GLBuffer::GetSize() const {
    return size_;
}

GLRenderTarget::GLRenderTarget(
    const RHI::u32 width,
    const RHI::u32 height,
    const unsigned int drawFramebuffer,
    const unsigned int resolveFramebuffer,
    const std::vector<unsigned int>& colorTextures,
    const std::vector<unsigned int>& msaaColorRenderbuffers,
    const unsigned int depthRenderbuffer,
    const unsigned int depthTexture,
    const unsigned int msaaDepthRenderbuffer,
    const RHI::Format colorFormat,
    const RHI::Format depthFormat,
    const RHI::u32 sampleCount,
    std::function<void(unsigned int)> unregisterMsaa
)
    : width_(width),
      height_(height),
      drawFramebuffer_(drawFramebuffer),
      resolveFramebuffer_(resolveFramebuffer),
      colorTextures_(colorTextures),
      msaaColorRenderbuffers_(msaaColorRenderbuffers),
      depthRenderbuffer_(depthRenderbuffer),
      depthTexture_(depthTexture),
      msaaDepthRenderbuffer_(msaaDepthRenderbuffer),
      colorFormat_(colorFormat),
      depthFormat_(depthFormat),
      sampleCount_(sampleCount),
      unregisterMsaa_(std::move(unregisterMsaa)) {
    colorTextureViews_.reserve(colorTextures_.size());
    for (const auto handle : colorTextures_) {
        RHI::Texture texture{};
        texture.width = width_;
        texture.height = height_;
        texture.format = colorFormat_;
        texture.type = RHI::TextureType::Texture2D;
        texture.graphic_handle_i = static_cast<int>(handle);
        texture.sampler_handle_ptr = nullptr;
        colorTextureViews_.push_back(texture);
    }

    if (depthTexture_ != 0U) {
        depthTextureView_.width = width_;
        depthTextureView_.height = height_;
        depthTextureView_.format = depthFormat_;
        depthTextureView_.type = RHI::TextureType::Texture2D;
        depthTextureView_.graphic_handle_i = static_cast<int>(depthTexture_);
        depthTextureView_.sampler_handle_ptr = nullptr;
        hasDepthTexture_ = true;
    }
}

GLRenderTarget::~GLRenderTarget() {
    if (depthTexture_ != 0U) {
        glDeleteTextures(1, &depthTexture_);
    }
    if (depthRenderbuffer_ != 0U) {
        glDeleteRenderbuffers(1, &depthRenderbuffer_);
    }
    if (msaaDepthRenderbuffer_ != 0U) {
        glDeleteRenderbuffers(1, &msaaDepthRenderbuffer_);
    }
    if (!msaaColorRenderbuffers_.empty()) {
        glDeleteRenderbuffers(static_cast<GLsizei>(msaaColorRenderbuffers_.size()), msaaColorRenderbuffers_.data());
    }
    if (!colorTextures_.empty()) {
        glDeleteTextures(static_cast<GLsizei>(colorTextures_.size()), colorTextures_.data());
    }
    if (resolveFramebuffer_ != 0U) {
        glDeleteFramebuffers(1, &resolveFramebuffer_);
    }
    if (drawFramebuffer_ != 0U && drawFramebuffer_ != resolveFramebuffer_) {
        glDeleteFramebuffers(1, &drawFramebuffer_);
    }
    if (unregisterMsaa_ != nullptr && drawFramebuffer_ != resolveFramebuffer_) {
        unregisterMsaa_(drawFramebuffer_);
    }
}

void* GLRenderTarget::GetRenderPassHandle() const {
    return nullptr;
}

void* GLRenderTarget::GetFramebufferHandle() const {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(drawFramebuffer_));
}

RHI::u32 GLRenderTarget::GetWidth() const {
    return width_;
}

RHI::u32 GLRenderTarget::GetHeight() const {
    return height_;
}

RHI::u32 GLRenderTarget::GetColorAttachmentCount() const {
    return static_cast<RHI::u32>(colorTextureViews_.size());
}

RHI::u32 GLRenderTarget::GetSampleCount() const {
    return sampleCount_;
}

RHI::Texture GLRenderTarget::GetColorTexture(const RHI::u32 index) const {
    if (index >= colorTextureViews_.size()) {
        return {};
    }
    return colorTextureViews_[index];
}

bool GLRenderTarget::HasDepth() const {
    return depthRenderbuffer_ != 0U || depthTexture_ != 0U || msaaDepthRenderbuffer_ != 0U;
}

RHI::Texture GLRenderTarget::GetDepthTexture() const {
    return hasDepthTexture_ ? depthTextureView_ : RHI::Texture{};
}

} // namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils

