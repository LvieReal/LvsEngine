#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Lvs::Engine::Utils::SourcePath {

namespace {

Core::String ToUtf8String(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return Core::String(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::filesystem::path NormalizePath(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::absolute(path, ec);
    abs = abs.lexically_normal();
    return abs;
}

bool StartsWithInsensitive(const Core::String& text, const Core::String& prefix) {
    if (prefix.size() > text.size()) {
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        const auto a = static_cast<unsigned char>(text[i]);
        const auto b = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

std::filesystem::path FindAnchorDir() {
    std::vector<std::filesystem::path> roots;
    roots.push_back(std::filesystem::current_path());

    const Core::String exeDir = GetExecutablePath();
    if (!exeDir.empty()) {
        roots.push_back(std::filesystem::path(exeDir));
    }

    for (auto root : roots) {
        root = NormalizePath(root);
        for (int i = 0; i < 6; ++i) {
            if (std::filesystem::exists(root / "src" / "Lvs" / "Engine" / "Content")) {
                return root;
            }
            if (std::filesystem::exists(root / "Lvs" / "Engine" / "Content")) {
                return root;
            }
            if (!root.has_parent_path()) {
                break;
            }
            root = root.parent_path();
        }
    }

    return NormalizePath(std::filesystem::current_path());
}

std::filesystem::path ResourceRootPath() {
    static const std::filesystem::path root = []() {
        const std::filesystem::path anchor = FindAnchorDir();
        const std::filesystem::path devPath = anchor / "src" / "Lvs" / "Engine" / "Content";
        if (std::filesystem::exists(devPath)) {
            return NormalizePath(devPath);
        }
        return NormalizePath(anchor / "Lvs" / "Engine" / "Content");
    }();
    return root;
}

std::filesystem::path SourceRootPath() {
    static const std::filesystem::path root = NormalizePath(FindAnchorDir());
    return root;
}

Core::String RelativeNormalized(const std::filesystem::path& path) {
    return path.lexically_normal().generic_string();
}

bool IsSubpath(const std::filesystem::path& base, const std::filesystem::path& target) {
    const auto baseStr = ToUtf8String(NormalizePath(base));
    const auto targetStr = ToUtf8String(NormalizePath(target));
#if defined(_WIN32)
    return StartsWithInsensitive(targetStr, baseStr);
#else
    return targetStr.rfind(baseStr, 0) == 0;
#endif
}

} // namespace

Core::String GetExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer;
    buffer.resize(32768);
    const DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0 || len >= buffer.size()) {
        return {};
    }
    buffer.resize(len);
    const std::filesystem::path exePath(buffer);
    return ToUtf8String(exePath.parent_path());
#else
    // Portable fallback.
    return ToUtf8String(std::filesystem::current_path());
#endif
}

Core::String GetSourcePath(const Core::String& path) {
    return ToUtf8String(NormalizePath(SourceRootPath() / std::filesystem::path(path)));
}

Core::String GetResourcePath(const Core::String& path) {
    return ToUtf8String(NormalizePath(ResourceRootPath() / std::filesystem::path(path)));
}

Core::String OsToCorePath(const Core::String& path) {
    const std::filesystem::path base = NormalizePath(std::filesystem::path(GetResourcePath({})));
    const std::filesystem::path target = NormalizePath(std::filesystem::path(path));

    if (!IsSubpath(base, target)) {
        throw std::runtime_error("Path is not inside core resources");
    }

    const std::filesystem::path rel = target.lexically_relative(base);
    return Core::String(CORE_PATH_FORMAT) + RelativeNormalized(rel);
}

Core::String OsToLocalPath(const Core::String& path) {
    const std::filesystem::path base = NormalizePath(std::filesystem::path(GetSourcePath({})));
    const std::filesystem::path target = NormalizePath(std::filesystem::path(path));

    if (!IsSubpath(base, target)) {
        throw std::runtime_error("Path is not inside local source");
    }

    const std::filesystem::path rel = target.lexically_relative(base);
    return Core::String(LOCAL_PATH_FORMAT) + RelativeNormalized(rel);
}

Core::String CorePathToOs(const Core::String& path) {
    if (path.rfind(CORE_PATH_FORMAT, 0) != 0) {
        throw std::runtime_error("Invalid core path");
    }
    const Core::String rel = path.substr(std::char_traits<char>::length(CORE_PATH_FORMAT));
    return GetResourcePath(rel);
}

Core::String LocalPathToOs(const Core::String& path) {
    if (path.rfind(LOCAL_PATH_FORMAT, 0) != 0) {
        throw std::runtime_error("Invalid local path");
    }
    const Core::String rel = path.substr(std::char_traits<char>::length(LOCAL_PATH_FORMAT));
    return GetSourcePath(rel);
}

Core::String ToOsPath(const Core::String& path) {
    if (path.empty()) {
        return path;
    }
    if (path.rfind(CORE_PATH_FORMAT, 0) == 0) {
        return CorePathToOs(path);
    }
    if (path.rfind(LOCAL_PATH_FORMAT, 0) == 0) {
        return LocalPathToOs(path);
    }
    return ToUtf8String(NormalizePath(std::filesystem::path(path)));
}

} // namespace Lvs::Engine::Utils::SourcePath

