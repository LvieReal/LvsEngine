#pragma once

#ifdef _WIN32
#include <windows.h>

namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils {

void* GetGLProcAddress(const char* name);
void DisableVSyncIfAvailable();
HGLRC CreateBestOpenGLContextForDevice(HDC hdc, unsigned int& majorOut, unsigned int& minorOut);

} // namespace Lvs::Engine::Rendering::Backends::OpenGL::Utils

#endif

