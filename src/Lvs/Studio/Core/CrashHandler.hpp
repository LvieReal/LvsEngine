#pragma once

#include <QString>

#include <exception>

namespace Lvs::Engine::Core::CrashHandler {

void Install();

void WriteCrashLog(const QString& title, const QString& details = {});
void WriteCrashLogFromException(const std::exception& ex, const QString& context = {});

} // namespace Lvs::Engine::Core::CrashHandler
