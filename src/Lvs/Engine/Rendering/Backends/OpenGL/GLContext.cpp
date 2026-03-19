#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLCommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/GLPipeline.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/Utils/GLRhiObjects.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/Utils/GLRenderUtils.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/Utils/GLWin32Utils.hpp"
#include "Lvs/Engine/Rendering/Common/SceneUniformData.hpp"
#include "Lvs/Engine/Rendering/ShaderLoader.hpp"

#ifdef _WIN32
#include <windows.h>
#endif
#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace Lvs::Engine::Rendering::Backends::OpenGL {

// Utility helpers live in Backends/OpenGL/Utils.

GLContext::GLContext(const GLApi api)
    : api_(api) {}

GLContext::~GLContext() {
    renderer_.reset();
    cmdBuffer_.reset();
    if (api_.GladLoaded) {
        for (const auto& [handle, type] : ownedTextures_) {
            static_cast<void>(type);
            if (handle != 0U) {
                glDeleteTextures(1, &handle);
            }
        }
        ownedTextures_.clear();
    }
    if (api_.GladLoaded && defaultVao_ != 0U) {
        glDeleteVertexArrays(1, &defaultVao_);
        defaultVao_ = 0U;
    }
    if (api_.GladLoaded && pushConstantBuffer_ != 0U) {
        glDeleteBuffers(1, &pushConstantBuffer_);
        pushConstantBuffer_ = 0U;
    }
    DestroyNativeContext();
}

std::unique_ptr<RHI::ICommandBuffer> GLContext::AllocateCommandBuffer() {
    return std::make_unique<GLCommandBuffer>(*this);
}

std::unique_ptr<RHI::IPipeline> GLContext::CreatePipeline(const RHI::PipelineDesc& desc) {
    if (!api_.GladLoaded) {
        return std::make_unique<GLPipeline>(desc, 0U, nullptr);
    }

    const std::string vertSrc = ShaderLoader::LoadGLSL(desc.pipelineId, "vert");
    const std::string fragSrc = ShaderLoader::LoadGLSL(desc.pipelineId, "frag");
    const unsigned int vertShader = Utils::CompileShader(GL_VERTEX_SHADER, vertSrc, "vertex");
    const unsigned int fragShader = Utils::CompileShader(GL_FRAGMENT_SHADER, fragSrc, "fragment");

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    int linkStatus = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    if (linkStatus == 0) {
        int logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log(static_cast<std::size_t>(std::max(logLen, 1)), '\0');
        int written = 0;
        glGetProgramInfoLog(program, static_cast<int>(log.size()), &written, log.data());
        glDeleteProgram(program);
        throw std::runtime_error("OpenGL program link failed: " + log);
    }

    return std::make_unique<GLPipeline>(desc, program, [loaded = api_.GladLoaded](const unsigned int programHandle) {
        if (loaded && programHandle != 0U) {
            glDeleteProgram(programHandle);
        }
    });
}

std::unique_ptr<RHI::IRenderTarget> GLContext::CreateRenderTarget(const RHI::RenderTargetDesc& desc) {
    if (!api_.GladLoaded || desc.width == 0U || desc.height == 0U || (desc.colorAttachmentCount == 0U && !desc.hasDepth)) {
        return nullptr;
    }

    RHI::u32 sampleCount = std::max<RHI::u32>(1U, desc.sampleCount);
    if (desc.depthTexture && sampleCount > 1U) {
        sampleCount = 1U;
    }
    if (sampleCount > 1U) {
        GLint maxSamples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        if (maxSamples > 0) {
            sampleCount = std::min(sampleCount, static_cast<RHI::u32>(maxSamples));
        }
    }

    const bool useMsaa = sampleCount > 1U;
    unsigned int drawFramebuffer = 0U;
    unsigned int resolveFramebuffer = 0U;
    glGenFramebuffers(1, &drawFramebuffer);
    resolveFramebuffer = drawFramebuffer;
    if (useMsaa) {
        glGenFramebuffers(1, &resolveFramebuffer);
    }

    std::vector<unsigned int> colorTextures{};
    std::vector<unsigned int> msaaColorRenderbuffers{};
    if (desc.colorAttachmentCount > 0U) {
        colorTextures.resize(desc.colorAttachmentCount, 0U);
        glGenTextures(static_cast<GLsizei>(colorTextures.size()), colorTextures.data());

        glBindFramebuffer(GL_FRAMEBUFFER, resolveFramebuffer);
        for (RHI::u32 index = 0; index < desc.colorAttachmentCount; ++index) {
            const auto handle = colorTextures[index];
            glBindTexture(GL_TEXTURE_2D, handle);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA16F,
                static_cast<GLsizei>(desc.width),
                static_cast<GLsizei>(desc.height),
                0,
                GL_RGBA,
                GL_FLOAT,
                nullptr
            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, handle, 0);
        }

        std::vector<GLenum> drawBuffers;
        drawBuffers.reserve(desc.colorAttachmentCount);
        for (RHI::u32 index = 0; index < desc.colorAttachmentCount; ++index) {
            drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + index);
        }
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());

        if (useMsaa) {
            msaaColorRenderbuffers.resize(desc.colorAttachmentCount, 0U);
            glGenRenderbuffers(static_cast<GLsizei>(msaaColorRenderbuffers.size()), msaaColorRenderbuffers.data());
            glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer);
            for (RHI::u32 index = 0; index < desc.colorAttachmentCount; ++index) {
                glBindRenderbuffer(GL_RENDERBUFFER, msaaColorRenderbuffers[index]);
                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER,
                    static_cast<GLsizei>(sampleCount),
                    GL_RGBA16F,
                    static_cast<GLsizei>(desc.width),
                    static_cast<GLsizei>(desc.height)
                );
                glFramebufferRenderbuffer(
                    GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0 + index,
                    GL_RENDERBUFFER,
                    msaaColorRenderbuffers[index]
                );
            }
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        }
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, resolveFramebuffer);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (useMsaa) {
            glBindFramebuffer(GL_FRAMEBUFFER, drawFramebuffer);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
    }

    unsigned int depthRenderbuffer = 0U;
    unsigned int depthTexture = 0U;
    unsigned int msaaDepthRenderbuffer = 0U;
    if (desc.hasDepth) {
        if (desc.depthTexture) {
            glBindFramebuffer(GL_FRAMEBUFFER, resolveFramebuffer);
            glGenTextures(1, &depthTexture);
            glBindTexture(GL_TEXTURE_2D, depthTexture);
            if (desc.depthFormat == RHI::Format::D24S8) {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_DEPTH24_STENCIL8,
                    static_cast<GLsizei>(desc.width),
                    static_cast<GLsizei>(desc.height),
                    0,
                    GL_DEPTH_STENCIL,
                    GL_UNSIGNED_INT_24_8,
                    nullptr
                );
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
            } else {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_DEPTH_COMPONENT32F,
                    static_cast<GLsizei>(desc.width),
                    static_cast<GLsizei>(desc.height),
                    0,
                    GL_DEPTH_COMPONENT,
                    GL_FLOAT,
                    nullptr
                );
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        } else {
            const GLenum depthAttachment = (desc.depthFormat == RHI::Format::D24S8) ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                                    : GL_DEPTH_ATTACHMENT;
            const GLenum depthInternal = (desc.depthFormat == RHI::Format::D24S8) ? GL_DEPTH24_STENCIL8
                                                                                  : GL_DEPTH_COMPONENT24;
            glBindFramebuffer(GL_FRAMEBUFFER, useMsaa ? drawFramebuffer : resolveFramebuffer);
            glGenRenderbuffers(1, &depthRenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
            if (useMsaa) {
                glRenderbufferStorageMultisample(
                    GL_RENDERBUFFER,
                    static_cast<GLsizei>(sampleCount),
                    depthInternal,
                    static_cast<GLsizei>(desc.width),
                    static_cast<GLsizei>(desc.height)
                );
                msaaDepthRenderbuffer = depthRenderbuffer;
                depthRenderbuffer = 0U;
            } else {
                glRenderbufferStorage(
                    GL_RENDERBUFFER,
                    depthInternal,
                    static_cast<GLsizei>(desc.width),
                    static_cast<GLsizei>(desc.height)
                );
            }
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, depthAttachment, GL_RENDERBUFFER, useMsaa ? msaaDepthRenderbuffer : depthRenderbuffer);
        }
    }

    const auto checkFramebuffer = [this](const unsigned int framebuffer) -> bool {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    };

    const bool drawComplete = checkFramebuffer(drawFramebuffer);
    const bool resolveComplete = (resolveFramebuffer == drawFramebuffer) ? drawComplete : checkFramebuffer(resolveFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, api_.DefaultFramebuffer);
    if (!drawComplete || !resolveComplete) {
        if (depthRenderbuffer != 0U) {
            glDeleteRenderbuffers(1, &depthRenderbuffer);
        }
        if (msaaDepthRenderbuffer != 0U) {
            glDeleteRenderbuffers(1, &msaaDepthRenderbuffer);
        }
        if (depthTexture != 0U) {
            glDeleteTextures(1, &depthTexture);
        }
        if (!msaaColorRenderbuffers.empty()) {
            glDeleteRenderbuffers(static_cast<GLsizei>(msaaColorRenderbuffers.size()), msaaColorRenderbuffers.data());
        }
        if (!colorTextures.empty()) {
            glDeleteTextures(static_cast<GLsizei>(colorTextures.size()), colorTextures.data());
        }
        if (resolveFramebuffer != 0U) {
            glDeleteFramebuffers(1, &resolveFramebuffer);
        }
        if (drawFramebuffer != 0U && drawFramebuffer != resolveFramebuffer) {
            glDeleteFramebuffers(1, &drawFramebuffer);
        }
        return nullptr;
    }

    if (useMsaa) {
        msaaResolveTargets_[drawFramebuffer] = MsaaResolveInfo{
            .drawFramebuffer = drawFramebuffer,
            .resolveFramebuffer = resolveFramebuffer,
            .width = desc.width,
            .height = desc.height,
            .colorAttachmentCount = desc.colorAttachmentCount
        };
    }

    return std::make_unique<Utils::GLRenderTarget>(
        desc.width,
        desc.height,
        drawFramebuffer,
        resolveFramebuffer,
        colorTextures,
        msaaColorRenderbuffers,
        depthRenderbuffer,
        depthTexture,
        msaaDepthRenderbuffer,
        desc.colorAttachmentCount > 0U ? RHI::Format::R16G16B16A16_Float : RHI::Format::Unknown,
        desc.depthFormat,
        sampleCount,
        [this](const unsigned int framebufferId) {
            msaaResolveTargets_.erase(framebufferId);
        }
    );
}

std::unique_ptr<RHI::IBuffer> GLContext::CreateBuffer(const RHI::BufferDesc& desc) {
    if (!api_.GladLoaded || desc.size == 0) {
        return std::make_unique<Utils::GLBuffer>(0U, desc.size);
    }

    unsigned int handle = 0U;
    glGenBuffers(1, &handle);

    const GLenum target = desc.type == RHI::BufferType::Index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    const GLenum usage = desc.usage == RHI::BufferUsage::Dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glBindBuffer(target, handle);
    glBufferData(target, static_cast<GLsizeiptr>(desc.size), desc.initialData, usage);

    return std::make_unique<Utils::GLBuffer>(handle, desc.size);
}

std::unique_ptr<RHI::IResourceSet> GLContext::CreateResourceSet(const RHI::ResourceSetDesc& desc) {
    static RHI::u32 nextId = 1U;
    const RHI::u32 id = nextId++;
    std::vector<RHI::ResourceBinding> bindings;
    bindings.reserve(desc.bindingCount);
    for (RHI::u32 i = 0; i < desc.bindingCount; ++i) {
        bindings.push_back(desc.bindings[i]);
    }
    resourceSetTextures_[id] = std::move(bindings);
    return std::make_unique<Utils::GLResourceSet>(id);
}

RHI::Texture GLContext::CreateTexture2D(const RHI::Texture2DDesc& desc) {
    if (!api_.GladLoaded || desc.width == 0U || desc.height == 0U || desc.pixels.empty()) {
        return {};
    }
    unsigned int textureHandle = 0U;
    glGenTextures(1, &textureHandle);
    if (textureHandle == 0U) {
        return {};
    }
    glBindTexture(GL_TEXTURE_2D, textureHandle);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(desc.width),
        static_cast<GLsizei>(desc.height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        desc.pixels.data()
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    const GLint minFilter = desc.generateMipmaps
                                ? (desc.linearFiltering ? static_cast<GLint>(GL_LINEAR_MIPMAP_LINEAR)
                                                        : static_cast<GLint>(GL_NEAREST_MIPMAP_NEAREST))
                                : (desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST));
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        minFilter
    );
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
    if (desc.generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    ownedTextures_[textureHandle] = RHI::TextureType::Texture2D;

    RHI::Texture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.format = desc.format;
    texture.type = RHI::TextureType::Texture2D;
    texture.graphic_handle_i = static_cast<int>(textureHandle);
    texture.sampler_handle_ptr = nullptr;
    return texture;
}

RHI::Texture GLContext::CreateTexture3D(const RHI::Texture3DDesc& desc) {
    if (!api_.GladLoaded || desc.width == 0U || desc.height == 0U || desc.depth == 0U || desc.pixels.empty()) {
        return {};
    }
    unsigned int textureHandle = 0U;
    glGenTextures(1, &textureHandle);
    if (textureHandle == 0U) {
        return {};
    }
    glBindTexture(GL_TEXTURE_3D, textureHandle);
    glTexImage3D(
        GL_TEXTURE_3D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(desc.width),
        static_cast<GLsizei>(desc.height),
        static_cast<GLsizei>(desc.depth),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        desc.pixels.data()
    );
    const GLint wrap = desc.repeat ? static_cast<GLint>(GL_REPEAT) : static_cast<GLint>(GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, wrap);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, wrap);
    glTexParameteri(
        GL_TEXTURE_3D,
        GL_TEXTURE_MIN_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
    glTexParameteri(
        GL_TEXTURE_3D,
        GL_TEXTURE_MAG_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
    ownedTextures_[textureHandle] = RHI::TextureType::Texture3D;

    RHI::Texture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.depth = desc.depth;
    texture.format = desc.format;
    texture.type = RHI::TextureType::Texture3D;
    texture.graphic_handle_i = static_cast<int>(textureHandle);
    texture.sampler_handle_ptr = nullptr;
    return texture;
}

RHI::Texture GLContext::CreateTextureCube(const RHI::CubemapDesc& desc) {
    if (!api_.GladLoaded || desc.width == 0U || desc.height == 0U) {
        return {};
    }
    unsigned int textureHandle = 0U;
    glGenTextures(1, &textureHandle);
    if (textureHandle == 0U) {
        return {};
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureHandle);
    for (int face = 0; face < 6; ++face) {
        const auto& facePixels = desc.faces[static_cast<std::size_t>(face)];
        if (facePixels.empty()) {
            continue;
        }
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            facePixels.data()
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_MIN_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
    glTexParameteri(
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_MAG_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
    ownedTextures_[textureHandle] = RHI::TextureType::TextureCube;

    RHI::Texture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.format = desc.format;
    texture.type = RHI::TextureType::TextureCube;
    texture.graphic_handle_i = static_cast<int>(textureHandle);
    texture.sampler_handle_ptr = nullptr;
    return texture;
}

void GLContext::DestroyTexture(RHI::Texture& texture) {
    if (!api_.GladLoaded) {
        texture = {};
        return;
    }
    const auto handle = static_cast<unsigned int>(texture.graphic_handle_i);
    const auto it = ownedTextures_.find(handle);
    if (it != ownedTextures_.end() && handle != 0U) {
        glDeleteTextures(1, &handle);
        ownedTextures_.erase(it);
    }
    texture = {};
}

void GLContext::BindTexture(const RHI::u32 slot, const RHI::Texture& texture) {
    if (!api_.GladLoaded) {
        return;
    }
    textureSlots_[slot] = texture;
    glActiveTexture(GL_TEXTURE0 + slot);
    GLenum target = GL_TEXTURE_2D;
    if (texture.type == RHI::TextureType::TextureCube) {
        target = GL_TEXTURE_CUBE_MAP;
    } else if (texture.type == RHI::TextureType::Texture3D) {
        target = GL_TEXTURE_3D;
    }
    glBindTexture(target, static_cast<unsigned int>(texture.graphic_handle_i));
}

void* GLContext::GetDefaultRenderPassHandle() const {
    return nullptr;
}

void* GLContext::GetDefaultFramebufferHandle() const {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(0U));
}

bool GLContext::EnsureNativeContext() {
#ifdef _WIN32
    if (api_.NativeWindowHandle == nullptr) {
        return false;
    }

    auto* hwnd = reinterpret_cast<HWND>(api_.NativeWindowHandle);
    auto* hdc = reinterpret_cast<HDC>(deviceContext_);
    auto* hglrc = reinterpret_cast<HGLRC>(api_.ContextHandle);

    if (hdc != nullptr && hglrc != nullptr) {
        return wglMakeCurrent(hdc, hglrc) == TRUE;
    }

    hdc = GetDC(hwnd);
    if (hdc == nullptr) {
        return false;
    }

    if (GetPixelFormat(hdc) == 0) {
        const PIXELFORMATDESCRIPTOR pfd{
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,
            32,
            0, 0, 0, 0, 0, 0,
            0,
            0,
            0,
            0, 0, 0, 0,
            24,
            8,
            0,
            PFD_MAIN_PLANE,
            0,
            0, 0, 0
        };
        const int pixelFormat = ChoosePixelFormat(hdc, &pfd);
        if (pixelFormat == 0 || SetPixelFormat(hdc, pixelFormat, &pfd) == FALSE) {
            ReleaseDC(hwnd, hdc);
            return false;
        }
    }

    unsigned int majorVersion = 0U;
    unsigned int minorVersion = 0U;
    hglrc = Utils::CreateBestOpenGLContextForDevice(hdc, majorVersion, minorVersion);
    if (hglrc == nullptr) {
        ReleaseDC(hwnd, hdc);
        return false;
    }

    deviceContext_ = hdc;
    api_.ContextHandle = hglrc;
    api_.MajorVersion = majorVersion;
    api_.MinorVersion = minorVersion;
    return true;
#else
    return false;
#endif
}

void GLContext::DestroyNativeContext() {
#ifdef _WIN32
    auto* hwnd = reinterpret_cast<HWND>(api_.NativeWindowHandle);
    auto* hdc = reinterpret_cast<HDC>(deviceContext_);
    auto* hglrc = reinterpret_cast<HGLRC>(api_.ContextHandle);

    if (hglrc != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(hglrc);
    }
    if (hwnd != nullptr && hdc != nullptr) {
        ReleaseDC(hwnd, hdc);
    }
    deviceContext_ = nullptr;
    api_.ContextHandle = nullptr;
    api_.GladLoaded = false;
    api_.MajorVersion = 0U;
    api_.MinorVersion = 0U;
#endif
}

void GLContext::Initialize(const RHI::u32 width, const RHI::u32 height) {
    if (!EnsureNativeContext()) {
        return;
    }
#ifdef _WIN32
    Utils::DisableVSyncIfAvailable();
#endif
    if (!api_.GladLoaded) {
#ifdef _WIN32
        api_.GladLoaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(Utils::GetGLProcAddress)) != 0;
#else
        api_.GladLoaded = gladLoadGL() != 0;
#endif
    }
    if (api_.GladLoaded) {
        GLint majorVersion = 0;
        GLint minorVersion = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
        glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
        if (majorVersion > 0) {
            api_.MajorVersion = static_cast<unsigned int>(majorVersion);
            api_.MinorVersion = static_cast<unsigned int>(minorVersion);
        }
        GLint fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        api_.DefaultFramebuffer = static_cast<unsigned int>(fbo);
        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDepthFunc(GL_GEQUAL);
        glClearDepthf(0.0F);
#if !defined(NDEBUG)
        if (GLAD_GL_VERSION_4_3 || GLAD_GL_KHR_debug) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(Utils::GLDebugMessageCallback, nullptr);
            glDebugMessageControl(
                GL_DONT_CARE,
                GL_DONT_CARE,
                GL_DEBUG_SEVERITY_NOTIFICATION,
                0,
                nullptr,
                GL_FALSE
            );
        }
#endif
    }
    if (api_.GladLoaded && defaultVao_ == 0U) {
        glGenVertexArrays(1, &defaultVao_);
    }
    if (api_.GladLoaded && pushConstantBuffer_ == 0U) {
        glGenBuffers(1, &pushConstantBuffer_);
    }
    renderer_ = std::make_unique<::Lvs::Engine::Rendering::Renderer>();
    renderer_->Initialize(*this, ::Lvs::Engine::Rendering::RenderSurface{width, height});
    frameIndex_ = 0;
}

void GLContext::Resize(const RHI::u32 width, const RHI::u32 height) {
    if (renderer_ == nullptr) {
        Initialize(width, height);
        return;
    }
    renderer_->Initialize(*this, ::Lvs::Engine::Rendering::RenderSurface{width, height});
}

void GLContext::Render(const ::Lvs::Engine::Rendering::SceneData& sceneData) {
    if (renderer_ == nullptr || !EnsureNativeContext()) {
        return;
    }

    auto cmd = AllocateCommandBuffer();
    auto* glCmd = dynamic_cast<GLCommandBuffer*>(cmd.release());
    if (glCmd == nullptr) {
        return;
    }
    cmdBuffer_.reset(glCmd);
    renderer_->RecordFrameCommands(*this, *cmdBuffer_, sceneData, frameIndex_++);
#ifdef _WIN32
    auto* hdc = reinterpret_cast<HDC>(deviceContext_);
    if (hdc != nullptr) {
        SwapBuffers(hdc);
    }
#endif
}

void GLContext::WaitIdle() {
    if (!EnsureNativeContext() || !api_.GladLoaded) {
        return;
    }
    glFinish();
}

void GLContext::RefreshShaders() {
    if (renderer_ != nullptr) {
        renderer_->InvalidatePipelines();
    }
}

void GLContext::BeginRenderPass(const RHI::RenderPassInfo& info) {
    if (!api_.GladLoaded) {
        return;
    }
    const unsigned int framebuffer = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(info.framebufferHandle));
    const bool useDefaultFramebuffer = framebuffer == 0U || framebuffer == api_.DefaultFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, useDefaultFramebuffer ? api_.DefaultFramebuffer : framebuffer);
    currentMsaaResolve_ = nullptr;
    if (!useDefaultFramebuffer) {
        const auto it = msaaResolveTargets_.find(framebuffer);
        if (it != msaaResolveTargets_.end()) {
            currentMsaaResolve_ = &it->second;
        }
    }
    if (useDefaultFramebuffer) {
        glDrawBuffer(GL_BACK);
    } else {
        std::vector<GLenum> drawBuffers;
        drawBuffers.reserve(info.colorAttachmentCount);
        for (RHI::u32 colorIndex = 0; colorIndex < info.colorAttachmentCount; ++colorIndex) {
            drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + colorIndex);
        }
        if (drawBuffers.empty()) {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        } else {
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        }
    }
    if (defaultVao_ != 0U) {
        glBindVertexArray(defaultVao_);
    }
    glViewport(0, 0, static_cast<int>(info.width), static_cast<int>(info.height));
    glScissor(0, 0, static_cast<int>(info.width), static_cast<int>(info.height));
    unsigned int clearMask = 0U;
    if (info.clearColor && (useDefaultFramebuffer || info.colorAttachmentCount > 0U)) {
        glClearColor(info.clearColorValue[0], info.clearColorValue[1], info.clearColorValue[2], info.clearColorValue[3]);
        clearMask |= GL_COLOR_BUFFER_BIT;
    }
    if (info.clearDepth) {
        glClearDepthf(info.clearDepthValue);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (clearMask != 0U) {
        // Previous passes may leave write-masks disabled (e.g. sky depth write off).
        // Force full clear write access before issuing glClear.
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        glClear(clearMask);
        if (info.clearColor && info.colorAttachmentCount > 1U) {
            const float black[4]{0.0F, 0.0F, 0.0F, 0.0F};
            for (RHI::u32 colorIndex = 1; colorIndex < info.colorAttachmentCount; ++colorIndex) {
                glClearBufferfv(GL_COLOR, static_cast<GLint>(colorIndex), black);
            }
        }
    }
}

void GLContext::EndRenderPass() {
    if (!api_.GladLoaded) {
        return;
    }
    if (currentMsaaResolve_ != nullptr) {
        const MsaaResolveInfo resolve = *currentMsaaResolve_;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(resolve.drawFramebuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(resolve.resolveFramebuffer));
        const GLint width = static_cast<GLint>(resolve.width);
        const GLint height = static_cast<GLint>(resolve.height);
        for (RHI::u32 index = 0; index < resolve.colorAttachmentCount; ++index) {
            glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + index);
            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        currentMsaaResolve_ = nullptr;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, api_.DefaultFramebuffer);
    glDrawBuffer(GL_BACK);
}

void GLContext::BindPipeline(const RHI::IPipeline& pipeline) {
    if (!api_.GladLoaded) {
        return;
    }
    if (const auto* glPipeline = dynamic_cast<const GLPipeline*>(&pipeline); glPipeline != nullptr) {
        currentVertexLayout_ = glPipeline->GetDesc().vertexLayout;
        if (glPipeline->GetDesc().sampleCount > 1U) {
            glEnable(GL_MULTISAMPLE);
        } else {
            glDisable(GL_MULTISAMPLE);
        }
        Utils::ApplyCullMode(glPipeline->GetDesc().cullMode);
#ifdef GL_DEPTH_CLAMP
        if (glPipeline->GetDesc().pipelineId == "shadow") {
            glEnable(GL_DEPTH_CLAMP);
        } else {
            glDisable(GL_DEPTH_CLAMP);
        }
#endif
        if (glPipeline->GetDesc().blending) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        } else {
            glDisable(GL_BLEND);
        }
        if (glPipeline->GetDesc().depthTest) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(Utils::ResolveDepthCompare(glPipeline->GetDesc().depthCompare));
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(glPipeline->GetDesc().depthWrite ? GL_TRUE : GL_FALSE);
    } else {
        currentVertexLayout_ = RHI::VertexLayout::None;
        Utils::ApplyCullMode(RHI::CullMode::Back);
        glDisable(GL_MULTISAMPLE);
#ifdef GL_DEPTH_CLAMP
        glDisable(GL_DEPTH_CLAMP);
#endif
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_GEQUAL);
        glDepthMask(GL_TRUE);
    }
    const auto program = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(pipeline.GetNativeHandle()));
    currentProgram_ = program;
    glUseProgram(program);
}

void GLContext::BindVertexBuffer(const RHI::u32 slot, const RHI::IBuffer& buffer, const std::size_t offset) {
    static_cast<void>(slot);
    static_cast<void>(offset);
    if (!api_.GladLoaded) {
        return;
    }
    const auto handle = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(buffer.GetNativeHandle()));
    glBindBuffer(GL_ARRAY_BUFFER, handle);
    if (currentVertexLayout_ == RHI::VertexLayout::P3 || currentVertexLayout_ == RHI::VertexLayout::P3N3) {
        const GLsizei stride = static_cast<GLsizei>(sizeof(float) * 6);
        const auto* base = reinterpret_cast<const std::byte*>(offset);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, base);
        if (currentVertexLayout_ == RHI::VertexLayout::P3N3) {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, base + (sizeof(float) * 3));
        } else {
            glDisableVertexAttribArray(1);
        }
    }
}

void GLContext::BindIndexBuffer(const RHI::IBuffer& buffer, const RHI::IndexType indexType, const std::size_t offset) {
    static_cast<void>(offset);
    if (!api_.GladLoaded) {
        return;
    }
    currentIndexType_ = indexType;
    const auto handle = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(buffer.GetNativeHandle()));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle);
}

void GLContext::BindResourceSet(const RHI::u32 slot, const RHI::IResourceSet& set) {
    static_cast<void>(slot);
    const auto id = static_cast<RHI::u32>(reinterpret_cast<std::uintptr_t>(set.GetNativeHandle()));
    const auto it = resourceSetTextures_.find(id);
    if (it == resourceSetTextures_.end()) {
        return;
    }
    for (const auto& binding : it->second) {
        if (binding.kind == RHI::ResourceBindingKind::UniformBuffer) {
            if (binding.buffer != nullptr) {
                const auto handle = static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(binding.buffer->GetNativeHandle())
                );
                glBindBufferBase(GL_UNIFORM_BUFFER, binding.slot, handle);
            }
        } else if (binding.kind == RHI::ResourceBindingKind::StorageBuffer) {
            if (binding.buffer != nullptr) {
                const auto handle = static_cast<unsigned int>(
                    reinterpret_cast<std::uintptr_t>(binding.buffer->GetNativeHandle())
                );
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding.slot, handle);
            }
        } else {
            // For sampler arrays we treat each array element as binding.slot + arrayElement.
            BindTexture(binding.slot + binding.arrayElement, binding.texture);
        }
    }
}

void GLContext::PushConstants(const RHI::ICommandBuffer::PushConstantsInfo& info) {
    if (!api_.GladLoaded || currentProgram_ == 0U || info.fields == nullptr || info.fieldCount == 0U) {
        return;
    }

    for (std::size_t i = 0; i < info.fieldCount; ++i) {
        const auto& field = info.fields[i];
        if (field.name == nullptr || field.data == nullptr) {
            continue;
        }
        const GLint location = glGetUniformLocation(currentProgram_, field.name);
        if (location < 0) {
            continue;
        }

        switch (field.type) {
            case RHI::ICommandBuffer::PushConstantFieldType::Float4:
                glUniform4fv(location, 1, static_cast<const float*>(field.data));
                break;
            case RHI::ICommandBuffer::PushConstantFieldType::UInt4:
                glUniform4uiv(location, 1, static_cast<const GLuint*>(field.data));
                break;
            case RHI::ICommandBuffer::PushConstantFieldType::Matrix4x4:
                glUniformMatrix4fv(location, 1, GL_FALSE, static_cast<const float*>(field.data));
                break;
            default:
                break;
        }
    }
}

void GLContext::Draw(const RHI::ICommandBuffer::DrawInfo& info) {
    if (!api_.GladLoaded) {
        return;
    }
    const auto instanceCount = static_cast<GLsizei>(std::max<RHI::u32>(1U, info.instanceCount));
    if (info.indexCount > 0U) {
        const GLenum type = currentIndexType_ == RHI::IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        glDrawElementsInstanced(GL_TRIANGLES, static_cast<int>(info.indexCount), type, nullptr, instanceCount);
    } else if (info.vertexCount > 0U) {
        glDrawArraysInstanced(GL_TRIANGLES, 0, static_cast<GLsizei>(info.vertexCount), instanceCount);
    }
}

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
