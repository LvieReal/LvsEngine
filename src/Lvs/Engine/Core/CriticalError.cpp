#include "Lvs/Engine/Core/CriticalError.hpp"
#include "Lvs/Engine/Core/CrashHandler.hpp"

#include <QCoreApplication>
#include <QMessageBox>
#include <QPushButton>
#include <cstdlib>

namespace Lvs::Engine::Core::CriticalError {

void ShowCriticalError(const QString& text) {
    QMessageBox::critical(
        nullptr,
        "Critical Error",
        text + "\n\nIf this is not your fault, please let us know: dombrowor69@gmail.com"
    );
}

void ShowGraphicsUnsupportedError(const QString& text) {
    QMessageBox box;
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle("Graphics Unsupported");
    box.setText("This system cannot run Lvs Engine because Graphics are unavailable or no compatible GPU was found.");
    box.setInformativeText(
        text + "\n\nUpdate your GPU drivers or run the app on hardware with Graphics support."
    );
    QPushButton* exitButton = box.addButton("Exit", QMessageBox::DestructiveRole);
    box.setDefaultButton(exitButton);
    box.setEscapeButton(exitButton);
    box.exec();

    CrashHandler::WriteCrashLog("Graphics Unsupported", text);
    QCoreApplication::exit(1);
}

[[noreturn]] void ShowUnexpectedNoReturnError(const QString& text) {
    const QString appName = QCoreApplication::applicationName();
    const QString appLabel = appName.contains("studio", Qt::CaseInsensitive) ? "Studio" : "App";

    QMessageBox box;
    box.setIcon(QMessageBox::Critical);
    box.setWindowTitle("Unexpected Error");
    box.setText(
        QString("An unexpected error occured and %1 needs to quit, sorry!\n\n%2").arg(appLabel, text)
    );
    QPushButton* exitButton = box.addButton("Exit", QMessageBox::DestructiveRole);
    box.setDefaultButton(exitButton);
    box.setEscapeButton(exitButton);
    box.exec();

    CrashHandler::WriteCrashLog("Unexpected Error (Exit)", text);
    QCoreApplication::quit();
    std::exit(1);
}

void ShowCriticalErrorFromException(const std::exception& ex) {
    ShowCriticalError(QString::fromUtf8(ex.what()));
}

void ShowUnknownCriticalError() {
    ShowCriticalError("Unknown startup failure.");
}

} // namespace Lvs::Engine::Core::CriticalError
