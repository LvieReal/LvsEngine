#pragma once

#include "Lvs/Engine/Core/Types.hpp"

namespace Lvs::Engine::Core::PlaceFileUtils {

[[nodiscard]] Core::String BinaryExtension();
[[nodiscard]] Core::String LegacyTextExtension();

[[nodiscard]] Core::String DefaultUntitledFileName();
[[nodiscard]] Core::String DefaultUntitledTomlFileName();

[[nodiscard]] Core::String FileDialogOpenFilter();
[[nodiscard]] Core::String FileDialogSaveBinaryFilter();
[[nodiscard]] Core::String FileDialogSaveTomlFilter();

[[nodiscard]] Core::String EnsureExtension(Core::String path, const Core::String& extensionWithoutDot);

} // namespace Lvs::Engine::Core::PlaceFileUtils

