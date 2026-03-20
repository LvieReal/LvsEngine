#pragma once

#include <QString>

#include <exception>

namespace Lvs::Engine::Core::RegularError {

void ShowError(const QString& text);
void ShowErrorFromException(const std::exception& ex);
void ShowUnknownError();

} // namespace Lvs::Engine::Core::RegularError
