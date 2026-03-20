#pragma once

#include <QString>

namespace Lvs::Engine::Core::SessionLog {

void Start(const QString& sessionName = {});
void Stop();

void Write(const QString& message);
void Info(const QString& message);
void Warn(const QString& message);
void Error(const QString& message);

[[nodiscard]] QString CurrentFilePath();

} // namespace Lvs::Engine::Core::SessionLog
