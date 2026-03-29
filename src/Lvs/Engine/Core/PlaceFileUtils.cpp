#include "Lvs/Engine/Core/PlaceFileUtils.hpp"

#include <filesystem>

namespace Lvs::Engine::Core::PlaceFileUtils {

Core::String BinaryExtension() {
    return "lvsp";
}

Core::String LegacyTextExtension() {
    return "lvsx";
}

Core::String DefaultUntitledFileName() {
    return "untitled." + BinaryExtension();
}

Core::String DefaultUntitledTomlFileName() {
    return "untitled.toml";
}

Core::String FileDialogOpenFilter() {
    return "Lvs Place Files (*.lvsp *.lvsx *.toml *.xml);;Binary Place Files (*.lvsp);;Text Place Files (*.lvsx *.toml *.xml);;All Files (*)";
}

Core::String FileDialogSaveBinaryFilter() {
    return "Lvs Place Files (*.lvsp);;All Files (*)";
}

Core::String FileDialogSaveTomlFilter() {
    return "TOML Place Files (*.toml *.lvsx);;All Files (*)";
}

Core::String EnsureExtension(Core::String path, const Core::String& extensionWithoutDot) {
    const std::filesystem::path p(path);
    if (p.has_extension()) {
        return path;
    }
    if (!extensionWithoutDot.empty()) {
        path += ".";
        path += extensionWithoutDot;
    }
    return path;
}

} // namespace Lvs::Engine::Core::PlaceFileUtils

