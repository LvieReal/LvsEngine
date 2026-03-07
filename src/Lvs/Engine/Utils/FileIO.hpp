#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Lvs::Engine::Utils::FileIO {

[[nodiscard]] bool Exists(const std::filesystem::path& path);
[[nodiscard]] std::vector<char> ReadBinaryFile(const std::filesystem::path& path);
[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path);

} // namespace Lvs::Engine::Utils::FileIO
