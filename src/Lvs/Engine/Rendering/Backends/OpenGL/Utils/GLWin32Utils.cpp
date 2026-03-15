#include "Lvs/Engine/Rendering/Backends/OpenGL/Utils/GLWin32Utils.hpp"

#ifdef _WIN32

#include <array>

namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils {

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

    const auto createAttribs =
        reinterpret_cast<WglCreateContextAttribsArbProc>(GetGLProcAddress("wglCreateContextAttribsARB"));
    if (createAttribs == nullptr) {
        majorOut = 1U;
        minorOut = 0U;
        return legacyContext;
    }

    const std::array<std::array<int, 2>, 2> candidates{{{4, 6}, {4, 5}}};
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

} // namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils

#endif

