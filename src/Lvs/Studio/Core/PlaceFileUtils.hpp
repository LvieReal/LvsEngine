#pragma once

#include <QString>

namespace Lvs::Studio::Core::PlaceFileUtils {

[[nodiscard]] QString Extension();
[[nodiscard]] QString DefaultUntitledFileName();
[[nodiscard]] QString FileDialogFilter();
[[nodiscard]] QString EnsureExtension(QString path);

} // namespace Lvs::Studio::Core::PlaceFileUtils

