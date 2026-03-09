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

void DisableVSyncIfAvailable() {
    const auto proc = reinterpret_cast<WglSwapIntervalExtProc>(GetGLProcAddress("wglSwapIntervalEXT"));
    if (proc != nullptr) {
        proc(0);
    }
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
    const auto target = texture.type == RHI::TextureType::TextureCube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
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

    hglrc = wglCreateContext(hdc);
    if (hglrc == nullptr || wglMakeCurrent(hdc, hglrc) == FALSE) {
        if (hglrc != nullptr) {
            wglDeleteContext(hglrc);
        }
        ReleaseDC(hwnd, hdc);
        return false;
    }

    deviceContext_ = hdc;
    api_.ContextHandle = hglrc;
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
        GLint fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        api_.DefaultFramebuffer = static_cast<unsigned int>(fbo);
        glEnable(GL_SCISSOR_TEST);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glDepthFunc(GL_GEQUAL);
        glClearDepthf(0.0F);
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
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer != 0U ? framebuffer : api_.DefaultFramebuffer);
    if (defaultVao_ != 0U) {
        glBindVertexArray(defaultVao_);
    }
    glViewport(0, 0, static_cast<int>(info.width), static_cast<int>(info.height));
    glScissor(0, 0, static_cast<int>(info.width), static_cast<int>(info.height));
    unsigned int clearMask = 0U;
    if (info.clearColor) {
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
    }
}

void GLContext::EndRenderPass() {
    if (!api_.GladLoaded) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, api_.DefaultFramebuffer);
}

void GLContext::BindPipeline(const RHI::IPipeline& pipeline) {
    if (!api_.GladLoaded) {
        return;
    }
    if (const auto* glPipeline = dynamic_cast<const GLPipeline*>(&pipeline); glPipeline != nullptr) {
        currentVertexLayout_ = glPipeline->GetDesc().vertexLayout;
        ApplyCullMode(glPipeline->GetDesc().cullMode);
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
    const GLuint blockIndex = glGetUniformBlockIndex(currentProgram_, "PushConstants");
    if (blockIndex != GL_INVALID_INDEX) {
        constexpr GLuint pushConstantBinding = 15U;
        glUniformBlockBinding(currentProgram_, blockIndex, pushConstantBinding);
        glBindBuffer(GL_UNIFORM_BUFFER, pushConstantBuffer_);
        glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, pushConstantBinding, pushConstantBuffer_);
        return;
    }

    struct UniformField {
        const char* name;
        bool matrix4;
        const float* value;
    };

    if (size >= sizeof(Common::SkyboxPushConstants)) {
        const auto& skyPush = *static_cast<const Common::SkyboxPushConstants*>(data);
        const std::array<UniformField, 2> skyFields{
            UniformField{"skyPush.viewProjection", true, skyPush.ViewProjection.data()},
            UniformField{"skyPush.tint", false, skyPush.Tint.data()}
        };
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

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
