#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Lvs::Engine::Utils::PathUtils {

inline constexpr std::string_view CORE_PATH_FORMAT = "core://";
inline constexpr std::string_view LOCAL_PATH_FORMAT = "local://";

[[nodiscard]] std::filesystem::path GetExecutablePath();
[[nodiscard]] std::filesystem::path GetSourcePath(std::string_view path = {});
[[nodiscard]] std::filesystem::path GetResourcePath(std::string_view path = {});
[[nodiscard]] std::filesystem::path CorePathToOs(std::string_view path);
[[nodiscard]] std::filesystem::path LocalPathToOs(std::string_view path);
[[nodiscard]] std::filesystem::path ToOsPath(std::string_view path);

} // namespace Lvs::Engine::Utils::PathUtils
