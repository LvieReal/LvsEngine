# LvsEngine

Early Lvs Engine version in modern C++ 23 with Qt6 UI and default Vulkan (1.0+) or fallback OpenGL (4.5+) rendering.

Studio is constantly being maintained, slowly expanding into building toy!

Official Discord (report issues, support, ...): https://discord.gg/45eqsxKT

## Project requirements

Windows 10+ 64 Bit, and:

- CMake 3.27+
- Ninja
- MSYS2
- MinGW-w64 toolchain (`mingw64`)
- Qt6 (MSYS2 `mingw-w64-x86_64-qt6-base`, `mingw-w64-x86_64-qt6-tools`)
- Assimp (MSYS2 `mingw-w64-x86_64-assimp`)
- Optional: Clang toolchain for ASAN (`clang64`)
- Vulkan SDK
  - `VULKAN_SDK` env var should point to your SDK install
  - `glslangValidator` or `glslc` must be available (used for shader compilation)

## Suggested MSYS2 packages

Run from MSYS2 shell:

```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
pacman -S --needed mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-tools
pacman -S --needed mingw-w64-x86_64-assimp
```

Clang version (purely for ASAN support):

```bash
pacman -S --needed mingw-w64-clang-x86_64-toolchain
pacman -S --needed mingw-w64-clang-x86_64-qt6-base mingw-w64-clang-x86_64-qt6-tools
pacman -S --needed mingw-w64-clang-x86_64-assimp
```

## Build (CMake Presets)

Debug (MinGW default):

```powershell
cmake --preset studio-mingw-debug
cmake --build --preset studio-mingw-debug --parallel $env:NUMBER_OF_PROCESSORS
```

Release:

```powershell
cmake --preset studio-mingw-release
cmake --build --preset studio-mingw-release --parallel $env:NUMBER_OF_PROCESSORS
```

Or build using tasks.json in VS Code

## VS Code

- Default configure/build preset is MinGW debug (`.vscode/settings.json`)
- Default build task is `studio-debug-mingw`
- MinGW debug/release launch configs are primary
- Clang ASAN tasks/config are kept for internal diagnostics
