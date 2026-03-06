#include "Lvs/Engine/Core/CriticalError.hpp"

#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>

namespace Lvs::Engine::Core::CriticalError {

void ShowCriticalError(const QString& text) {
    QMessageBox::critical(
        nullptr,
        "Critical Error",
        text + "\n\nIf this is not your fault, please let us know: dombrowor69@gmail.com"
    );
}

void ShowVulkanUnsupportedError(const QString& text) {
    QMessageBox box;
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle("Vulkan Unsupported");
    box.setText("This system cannot run Lvs Engine because Vulkan is unavailable or no compatible GPU was found.");
    box.setInformativeText(
        text + "\n\nUpdate your GPU drivers or run the app on hardware with Vulkan support."
    );
    QPushButton* exitButton = box.addButton("Exit", QMessageBox::DestructiveRole);
    box.setDefaultButton(exitButton);
    box.setEscapeButton(exitButton);
    box.exec();

    QCoreApplication::exit(1);
}

void ShowCriticalErrorFromException(const std::exception& ex) {
    ShowCriticalError(QString::fromUtf8(ex.what()));
}

void ShowUnknownCriticalError() {
    ShowCriticalError("Unknown startup failure.");
}

} // namespace Lvs::Engine::Core::CriticalError
