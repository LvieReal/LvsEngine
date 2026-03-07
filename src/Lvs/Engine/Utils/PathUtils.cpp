#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Lvs::Engine::Utils::PathUtils {

namespace {

std::filesystem::path Normalize(const std::filesystem::path& path) {
    std::error_code error;
    const auto absolute = std::filesystem::absolute(path, error);
    if (error) {
        return path.lexically_normal();
    }
    return absolute.lexically_normal();
}

std::filesystem::path FindAnchorDir() {
    std::vector<std::filesystem::path> roots{std::filesystem::current_path()};
    const auto executablePath = GetExecutablePath();
    if (!executablePath.empty()) {
        roots.push_back(executablePath);
    }

    for (auto root : roots) {
        for (int i = 0; i < 6; ++i) {
            if (std::filesystem::exists(root / "src/Lvs/Engine/Content") || std::filesystem::exists(root / "Lvs/Engine/Content")) {
                return Normalize(root);
            }
            if (!root.has_parent_path()) {
                break;
            }
            const auto parent = root.parent_path();
            if (parent == root) {
                break;
            }
            root = parent;
        }
    }

    return Normalize(std::filesystem::current_path());
}

std::filesystem::path ResourceRoot() {
    static const std::filesystem::path root = []() {
        const auto anchor = FindAnchorDir();
        const auto devPath = anchor / "src/Lvs/Engine/Content";
        if (std::filesystem::exists(devPath)) {
            return Normalize(devPath);
        }
        return Normalize(anchor / "Lvs/Engine/Content");
    }();
    return root;
}

std::filesystem::path SourceRoot() {
    static const std::filesystem::path root = Normalize(FindAnchorDir());
    return root;
}

std::string StripPrefix(std::string_view path, const std::string_view prefix) {
    return std::string(path.substr(prefix.size()));
}

} // namespace

std::filesystem::path GetExecutablePath() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return {};
    }
    return Normalize(std::filesystem::path(buffer).parent_path());
#else
    return Normalize(std::filesystem::current_path());
#endif
}

std::filesystem::path GetSourcePath(const std::string_view path) {
    return Normalize(SourceRoot() / std::filesystem::path(path));
}

std::filesystem::path GetResourcePath(const std::string_view path) {
    return Normalize(ResourceRoot() / std::filesystem::path(path));
}

std::filesystem::path CorePathToOs(const std::string_view path) {
    if (!path.starts_with(CORE_PATH_FORMAT)) {
        throw std::runtime_error("Invalid core path");
    }
    return GetResourcePath(StripPrefix(path, CORE_PATH_FORMAT));
}

std::filesystem::path LocalPathToOs(const std::string_view path) {
    if (!path.starts_with(LOCAL_PATH_FORMAT)) {
        throw std::runtime_error("Invalid local path");
    }
    return GetSourcePath(StripPrefix(path, LOCAL_PATH_FORMAT));
}

std::filesystem::path ToOsPath(const std::string_view path) {
    if (path.empty()) {
        return {};
    }
    if (path.starts_with(CORE_PATH_FORMAT)) {
        return CorePathToOs(path);
    }
    if (path.starts_with(LOCAL_PATH_FORMAT)) {
        return LocalPathToOs(path);
    }
    return Normalize(std::filesystem::path(path));
}

} // namespace Lvs::Engine::Utils::PathUtils
