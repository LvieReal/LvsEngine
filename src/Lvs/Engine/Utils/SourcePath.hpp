#pragma once

#include <QString>

namespace Lvs::Engine::Utils::SourcePath {

inline constexpr auto CORE_PATH_FORMAT = "core://";
inline constexpr auto LOCAL_PATH_FORMAT = "local://";

QString GetExecutablePath();
QString GetSourcePath(const QString& path);
QString GetResourcePath(const QString& path);

QString OsToCorePath(const QString& path);
QString OsToLocalPath(const QString& path);
QString CorePathToOs(const QString& path);
QString LocalPathToOs(const QString& path);
QString ToOsPath(const QString& path);

} // namespace Lvs::Engine::Utils::SourcePath
