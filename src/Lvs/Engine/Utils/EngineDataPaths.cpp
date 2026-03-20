#include "Lvs/Engine/Utils/EngineDataPaths.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace Lvs::Engine::Utils::EngineDataPaths {

namespace {

Core::String ToUtf8String(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return Core::String(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::filesystem::path EnsureDir(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return std::filesystem::absolute(path, ec);
}

std::filesystem::path HomeDir() {
#if defined(_WIN32)
    if (const char* userProfile = std::getenv("USERPROFILE"); userProfile != nullptr && *userProfile != '\0') {
        return std::filesystem::path(userProfile);
    }
    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (homeDrive != nullptr && homePath != nullptr) {
        return std::filesystem::path(Core::String(homeDrive) + Core::String(homePath));
    }
    return std::filesystem::current_path();
#else
    if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
        return std::filesystem::path(home);
    }
    return std::filesystem::current_path();
#endif
}

} // namespace

Core::String RootDir() {
    std::filesystem::path base;
#if defined(_WIN32)
    if (const char* localAppData = std::getenv("LOCALAPPDATA"); localAppData != nullptr && *localAppData != '\0') {
        base = std::filesystem::path(localAppData);
    } else {
        base = HomeDir();
    }
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && *xdg != '\0') {
        base = std::filesystem::path(xdg);
    } else {
        base = HomeDir() / ".local" / "share";
    }
#endif

    return ToUtf8String(EnsureDir(base / "LvsEngine"));
}

Core::String StudioConfigFile() {
    return ToUtf8String(std::filesystem::path(RootDir()) / "studioConfig.json");
}

Core::String LogsDir() {
    return ToUtf8String(EnsureDir(std::filesystem::path(RootDir()) / "logs"));
}

Core::String CrashLogsDir() {
    return ToUtf8String(EnsureDir(std::filesystem::path(RootDir()) / "crash_logs"));
}

Core::String DefaultLocalAssetsDir() {
    return ToUtf8String(EnsureDir(std::filesystem::path(RootDir()) / "assets"));
}

} // namespace Lvs::Engine::Utils::EngineDataPaths
