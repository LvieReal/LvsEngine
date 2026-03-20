#include "Lvs/Studio/Core/RegularError.hpp"

#include <QMessageBox>

namespace Lvs::Engine::Core::RegularError {

void ShowError(const QString& text) {
    QMessageBox::warning(nullptr, "Error", text);
}

void ShowErrorFromException(const std::exception& ex) {
    ShowError(QString::fromUtf8(ex.what()));
}

void ShowUnknownError() {
    ShowError("Unknown error.");
}

} // namespace Lvs::Engine::Core::RegularError
