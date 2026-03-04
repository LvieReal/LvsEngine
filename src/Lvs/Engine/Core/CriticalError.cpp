#include "Lvs/Engine/Core/CriticalError.hpp"

#include <QMessageBox>

namespace Lvs::Engine::Core::CriticalError {

void ShowCriticalError(const QString& text) {
    QMessageBox::critical(
        nullptr,
        "Critical Error",
        text + "\n\nIf this is not your fault, please let us know: dombrowor69@gmail.com"
    );
}

void ShowCriticalErrorFromException(const std::exception& ex) {
    ShowCriticalError(QString::fromUtf8(ex.what()));
}

void ShowUnknownCriticalError() {
    ShowCriticalError("Unknown startup failure.");
}

} // namespace Lvs::Engine::Core::CriticalError
