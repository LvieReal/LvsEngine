#include "Lvs/Engine/Rendering/Backends/OpenGL/GLContext.hpp"

#include "Lvs/Engine/Rendering/Backends/OpenGL/GLCommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Backends/OpenGL/GLPipeline.hpp"
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

namespace {

#ifdef _WIN32
void* GetGLProcAddress(const char* name) {
    void* proc = reinterpret_cast<void*>(wglGetProcAddress(name));
    if (proc == nullptr || proc == reinterpret_cast<void*>(0x1) || proc == reinterpret_cast<void*>(0x2) ||
        proc == reinterpret_cast<void*>(0x3) || proc == reinterpret_cast<void*>(-1)) {
        HMODULE module = GetModuleHandleA("opengl32.dll");
        proc = module != nullptr ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    }
    return proc;
}

using WglSwapIntervalExtProc = BOOL(WINAPI*)(int);
using WglCreateContextAttribsArbProc = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

constexpr int kWglContextMajorVersionArb = 0x2091;
constexpr int kWglContextMinorVersionArb = 0x2092;
constexpr int kWglContextFlagsArb = 0x2094;
constexpr int kWglContextProfileMaskArb = 0x9126;
constexpr int kWglContextCoreProfileBitArb = 0x00000001;
#if !defined(NDEBUG)
constexpr int kWglContextDebugBitArb = 0x0001;
#endif

void DisableVSyncIfAvailable() {
    const auto proc = reinterpret_cast<WglSwapIntervalExtProc>(GetGLProcAddress("wglSwapIntervalEXT"));
    if (proc != nullptr) {
        proc(0);
    }
}

HGLRC CreateBestOpenGLContextForDevice(HDC hdc, unsigned int& majorOut, unsigned int& minorOut) {
    majorOut = 0U;
    minorOut = 0U;
    if (hdc == nullptr) {
        return nullptr;
    }

    HGLRC legacyContext = wglCreateContext(hdc);
    if (legacyContext == nullptr || wglMakeCurrent(hdc, legacyContext) == FALSE) {
        if (legacyContext != nullptr) {
            wglDeleteContext(legacyContext);
        }
        return nullptr;
    }

    const auto createAttribs = reinterpret_cast<WglCreateContextAttribsArbProc>(GetGLProcAddress("wglCreateContextAttribsARB"));
    if (createAttribs == nullptr) {
        majorOut = 1U;
        minorOut = 0U;
        return legacyContext;
    }

    const std::array<std::array<int, 2>, 2> candidates{{
        {4, 6},
        {4, 5}
    }};
    for (const auto& candidate : candidates) {
        const int major = candidate[0];
        const int minor = candidate[1];
        const int flags =
#if !defined(NDEBUG)
            kWglContextDebugBitArb;
#else
            0;
#endif
        const std::array<int, 9> attribs{
            kWglContextMajorVersionArb,
            major,
            kWglContextMinorVersionArb,
            minor,
            kWglContextProfileMaskArb,
            kWglContextCoreProfileBitArb,
            kWglContextFlagsArb,
            flags,
            0
        };
        HGLRC created = createAttribs(hdc, nullptr, attribs.data());
        if (created == nullptr) {
            continue;
        }
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(legacyContext);
        if (wglMakeCurrent(hdc, created) == FALSE) {
            wglDeleteContext(created);
            return nullptr;
        }
        majorOut = static_cast<unsigned int>(major);
        minorOut = static_cast<unsigned int>(minor);
        return created;
    }

    majorOut = 1U;
    minorOut = 0U;
    return legacyContext;
}
#endif

class GLResourceSet final : public RHI::IResourceSet {
public:
    explicit GLResourceSet(const RHI::u32 id)
        : id_(id) {}

    [[nodiscard]] void* GetNativeHandle() const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id_));
    }

private:
    RHI::u32 id_{0U};
};

class GLRenderTarget final : public RHI::IRenderTarget {
public:
    GLRenderTarget(
        const RHI::u32 width,
        const RHI::u32 height,
        const unsigned int framebuffer,
        const std::vector<unsigned int>& colorTextures,
        const unsigned int depthRenderbuffer,
        const unsigned int depthTexture,
        const RHI::Format colorFormat,
        const RHI::Format depthFormat
    )
        : width_(width),
          height_(height),
          framebuffer_(framebuffer),
          colorTextures_(colorTextures),
          depthRenderbuffer_(depthRenderbuffer),
          depthTexture_(depthTexture),
          colorFormat_(colorFormat),
          depthFormat_(depthFormat) {
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

    ~GLRenderTarget() override {
        if (depthTexture_ != 0U) {
            glDeleteTextures(1, &depthTexture_);
        } else if (depthRenderbuffer_ != 0U) {
            glDeleteRenderbuffers(1, &depthRenderbuffer_);
        }
        if (!colorTextures_.empty()) {
            glDeleteTextures(static_cast<GLsizei>(colorTextures_.size()), colorTextures_.data());
        }
        if (framebuffer_ != 0U) {
            glDeleteFramebuffers(1, &framebuffer_);
        }
    }

    [[nodiscard]] void* GetRenderPassHandle() const override {
        return nullptr;
    }

    [[nodiscard]] void* GetFramebufferHandle() const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(framebuffer_));
    }

    [[nodiscard]] RHI::u32 GetWidth() const override {
        return width_;
    }

    [[nodiscard]] RHI::u32 GetHeight() const override {
        return height_;
    }

    [[nodiscard]] RHI::u32 GetColorAttachmentCount() const override {
        return static_cast<RHI::u32>(colorTextureViews_.size());
    }

    [[nodiscard]] RHI::Texture GetColorTexture(const RHI::u32 index) const override {
        if (index >= colorTextureViews_.size()) {
            return {};
        }
        return colorTextureViews_[index];
    }

    [[nodiscard]] bool HasDepth() const override {
        return depthRenderbuffer_ != 0U || depthTexture_ != 0U;
    }

    [[nodiscard]] RHI::Texture GetDepthTexture() const override {
        return hasDepthTexture_ ? depthTextureView_ : RHI::Texture{};
    }

private:
    RHI::u32 width_{0U};
    RHI::u32 height_{0U};
    unsigned int framebuffer_{0U};
    std::vector<unsigned int> colorTextures_{};
    unsigned int depthRenderbuffer_{0U};
    unsigned int depthTexture_{0U};
    RHI::Format colorFormat_{RHI::Format::R8G8B8A8_UNorm};
    RHI::Format depthFormat_{RHI::Format::D32_Float};
    std::vector<RHI::Texture> colorTextureViews_{};
    bool hasDepthTexture_{false};
    RHI::Texture depthTextureView_{};
};

class GLBuffer final : public RHI::IBuffer {
public:
    GLBuffer(const unsigned int handle, const std::size_t size)
        : handle_(handle),
          size_(size) {}

    ~GLBuffer() override {
        if (handle_ != 0U) {
            glDeleteBuffers(1, &handle_);
        }
    }

    [[nodiscard]] void* GetNativeHandle() const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(handle_));
    }

    [[nodiscard]] std::size_t GetSize() const override {
        return size_;
    }

private:
    unsigned int handle_{0U};
    std::size_t size_{0};
};

unsigned int CompileShader(
    const unsigned int shaderType,
    const std::string& source,
    const char* stageName
) {
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

} // namespace

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
    const unsigned int vertShader = CompileShader(GL_VERTEX_SHADER, vertSrc, "vertex");
    const unsigned int fragShader = CompileShader(GL_FRAGMENT_SHADER, fragSrc, "fragment");

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

    unsigned int framebuffer = 0U;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    std::vector<unsigned int> colorTextures;
    if (desc.colorAttachmentCount > 0U) {
        colorTextures.resize(desc.colorAttachmentCount, 0U);
        glGenTextures(static_cast<GLsizei>(colorTextures.size()), colorTextures.data());
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
    } else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    unsigned int depthRenderbuffer = 0U;
    unsigned int depthTexture = 0U;
    if (desc.hasDepth) {
        if (desc.depthTexture) {
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
            glGenRenderbuffers(1, &depthRenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
            glRenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH_COMPONENT24,
                static_cast<GLsizei>(desc.width),
                static_cast<GLsizei>(desc.height)
            );
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
        }
    }

    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, api_.DefaultFramebuffer);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        if (depthRenderbuffer != 0U) {
            glDeleteRenderbuffers(1, &depthRenderbuffer);
        }
        if (depthTexture != 0U) {
            glDeleteTextures(1, &depthTexture);
        }
        if (!colorTextures.empty()) {
            glDeleteTextures(static_cast<GLsizei>(colorTextures.size()), colorTextures.data());
        }
        if (framebuffer != 0U) {
            glDeleteFramebuffers(1, &framebuffer);
        }
        return nullptr;
    }

    return std::make_unique<GLRenderTarget>(
        desc.width,
        desc.height,
        framebuffer,
        colorTextures,
        depthRenderbuffer,
        depthTexture,
        desc.colorAttachmentCount > 0U ? RHI::Format::R16G16B16A16_Float : RHI::Format::Unknown,
        desc.depthFormat
    );
}

std::unique_ptr<RHI::IBuffer> GLContext::CreateBuffer(const RHI::BufferDesc& desc) {
    if (!api_.GladLoaded || desc.size == 0) {
        return std::make_unique<GLBuffer>(0U, desc.size);
    }

    unsigned int handle = 0U;
    glGenBuffers(1, &handle);

    const GLenum target = desc.type == RHI::BufferType::Index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    const GLenum usage = desc.usage == RHI::BufferUsage::Dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glBindBuffer(target, handle);
    glBufferData(target, static_cast<GLsizeiptr>(desc.size), desc.initialData, usage);

    return std::make_unique<GLBuffer>(handle, desc.size);
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
    return std::make_unique<GLResourceSet>(id);
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
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        desc.linearFiltering ? static_cast<GLint>(GL_LINEAR) : static_cast<GLint>(GL_NEAREST)
    );
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
    hglrc = CreateBestOpenGLContextForDevice(hdc, majorVersion, minorVersion);
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
    DisableVSyncIfAvailable();
#endif
    if (!api_.GladLoaded) {
#ifdef _WIN32
        api_.GladLoaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(GetGLProcAddress)) != 0;
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
            glDebugMessageCallback(GLDebugMessageCallback, nullptr);
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

void GLContext::BeginRenderPass(const RHI::RenderPassInfo& info) {
    if (!api_.GladLoaded) {
        return;
    }
    const unsigned int framebuffer = static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(info.framebufferHandle));
    const bool useDefaultFramebuffer = framebuffer == 0U || framebuffer == api_.DefaultFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, useDefaultFramebuffer ? api_.DefaultFramebuffer : framebuffer);
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
    glBindFramebuffer(GL_FRAMEBUFFER, api_.DefaultFramebuffer);
    glDrawBuffer(GL_BACK);
}

void GLContext::BindPipeline(const RHI::IPipeline& pipeline) {
    if (!api_.GladLoaded) {
        return;
    }
    if (const auto* glPipeline = dynamic_cast<const GLPipeline*>(&pipeline); glPipeline != nullptr) {
        currentVertexLayout_ = glPipeline->GetDesc().vertexLayout;
        ApplyCullMode(glPipeline->GetDesc().cullMode);
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
            glDepthFunc(ResolveDepthCompare(glPipeline->GetDesc().depthCompare));
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(glPipeline->GetDesc().depthWrite ? GL_TRUE : GL_FALSE);
    } else {
        currentVertexLayout_ = RHI::VertexLayout::None;
        ApplyCullMode(RHI::CullMode::Back);
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
        } else {
            BindTexture(binding.slot, binding.texture);
        }
    }
}

void GLContext::PushConstants(const void* data, const std::size_t size) {
    if (!api_.GladLoaded || currentProgram_ == 0U || pushConstantBuffer_ == 0U || data == nullptr || size == 0) {
        return;
    }

    const std::array<const char*, 5> pushBlockNames{
        "PushConstants",
        "ShadowPush",
        "SkyPush",
        "PostSettings",
        "BlurSettings"
    };
    bool uploadedToBlock = false;
    std::uint32_t pushBlockBinding = 15U;
    for (const char* blockName : pushBlockNames) {
        const GLuint blockIndex = glGetUniformBlockIndex(currentProgram_, blockName);
        if (blockIndex == GL_INVALID_INDEX) {
            continue;
        }
        GLint blockSize = 0;
        glGetActiveUniformBlockiv(currentProgram_, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
        const std::size_t uploadSize = static_cast<std::size_t>(std::max(0, blockSize));
        if (uploadSize == 0U) {
            continue;
        }
        std::vector<std::byte> blockData(uploadSize, std::byte{0});
        std::memcpy(blockData.data(), data, std::min(uploadSize, size));
        glUniformBlockBinding(currentProgram_, blockIndex, pushBlockBinding);
        glBindBuffer(GL_UNIFORM_BUFFER, pushConstantBuffer_);
        glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(uploadSize), blockData.data(), GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, pushBlockBinding, pushConstantBuffer_);
        uploadedToBlock = true;
        ++pushBlockBinding;
    }
    if (uploadedToBlock) {
        return;
    }

    struct UniformField {
        const char* name;
        bool matrix4;
        const float* value;
    };

    if (size == sizeof(Common::ShadowPushConstants)) {
        const auto& shadowPush = *static_cast<const Common::ShadowPushConstants*>(data);
        const std::array<UniformField, 2> shadowFields{
            UniformField{"pushData.model", true, shadowPush.Model.data()},
            UniformField{"pushData.cascade", false, shadowPush.Cascade.data()}
        };
        bool updated = false;
        for (const auto& field : shadowFields) {
            const GLint location = glGetUniformLocation(currentProgram_, field.name);
            if (location < 0) {
                continue;
            }
            if (field.matrix4) {
                glUniformMatrix4fv(location, 1, GL_FALSE, field.value);
            } else {
                glUniform4fv(location, 1, field.value);
            }
            updated = true;
        }
        if (updated) {
            return;
        }
    }

    if (size == sizeof(Common::SkyboxPushConstants)) {
        const auto& skyPush = *static_cast<const Common::SkyboxPushConstants*>(data);
        const std::array<UniformField, 2> skyFields{
            UniformField{"skyPush.viewProjection", true, skyPush.ViewProjection.data()},
            UniformField{"skyPush.tint", false, skyPush.Tint.data()}
        };
        bool updated = false;
        for (const auto& field : skyFields) {
            const GLint location = glGetUniformLocation(currentProgram_, field.name);
            if (location < 0) {
                continue;
            }
            if (field.matrix4) {
                glUniformMatrix4fv(location, 1, GL_FALSE, field.value);
            } else {
                glUniform4fv(location, 1, field.value);
            }
            updated = true;
        }
        if (updated) {
            return;
        }
    }

    if (size == sizeof(Common::PostProcessPushConstants)) {
        const auto& push = *static_cast<const Common::PostProcessPushConstants*>(data);
        const std::array<const char*, 2> names{"pushData.settings", "settings"};
        for (const char* name : names) {
            const GLint location = glGetUniformLocation(currentProgram_, name);
            if (location >= 0) {
                glUniform4fv(location, 1, push.Settings.data());
                return;
            }
        }
    }

    if (size < sizeof(Common::DrawPushConstants)) {
        return;
    }
    const auto& push = *static_cast<const Common::DrawPushConstants*>(data);
    const std::array<UniformField, 5> fields{
        UniformField{"pushData.model", true, push.Model.data()},
        UniformField{"pushData.baseColor", false, push.BaseColor.data()},
        UniformField{"pushData.material", false, push.Material.data()},
        UniformField{"pushData.surfaceData0", false, push.SurfaceData0.data()},
        UniformField{"pushData.surfaceData1", false, push.SurfaceData1.data()}
    };

    for (const auto& field : fields) {
        const GLint location = glGetUniformLocation(currentProgram_, field.name);
        if (location < 0) {
            continue;
        }
        if (field.matrix4) {
            glUniformMatrix4fv(location, 1, GL_FALSE, field.value);
        } else {
            glUniform4fv(location, 1, field.value);
        }
    }
}

void GLContext::DrawIndexed(const RHI::u32 indexCount) {
    if (!api_.GladLoaded || indexCount == 0) {
        return;
    }
    const GLenum type = currentIndexType_ == RHI::IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElements(GL_TRIANGLES, static_cast<int>(indexCount), type, nullptr);
}

void GLContext::Draw(const RHI::u32 vertexCount) {
    if (!api_.GladLoaded || vertexCount == 0U) {
        return;
    }
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertexCount));
}

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
