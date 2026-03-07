#include "Lvs/Engine/Utils/FileIO.hpp"

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace Lvs::Engine::Utils::FileIO {

bool Exists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::vector<char> ReadBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    const std::ifstream::pos_type size = file.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to read file size: " + path.string());
    }

    std::vector<char> data(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!data.empty() && !file.read(data.data(), static_cast<std::streamsize>(data.size()))) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    return data;
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace Lvs::Engine::Utils::FileIO
