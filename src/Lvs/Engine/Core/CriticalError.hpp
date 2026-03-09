#pragma once

#include <QString>

#include <exception>

namespace Lvs::Engine::Core::CriticalError {

void ShowCriticalError(const QString& text);
void ShowGraphicsUnsupportedError(const QString& text);
[[noreturn]] void ShowUnexpectedNoReturnError(const QString& text);
void ShowCriticalErrorFromException(const std::exception& ex);
void ShowUnknownCriticalError();

} // namespace Lvs::Engine::Core::CriticalError
