#include "Lvs/Launcher/Launcher.hpp"

#include "Lvs/Engine/Bootstrap.hpp"
#include "Lvs/Engine/Core/CrashHandler.hpp"
#include "Lvs/Engine/Core/CriticalError.hpp"
#include "Lvs/Engine/Core/SessionLog.hpp"
#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"
#include "Lvs/Studio/Bootstrap.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QIcon>

#include <exception>

namespace Lvs::Launcher {

int Run(int argc, char* argv[], const BuildType buildType, const char* appName, const char* appIconPath) {
    QApplication app(argc, argv);
    app.setApplicationDisplayName(appName);
    app.setApplicationName(appName);

    Engine::Core::CrashHandler::Install();
    Engine::Core::SessionLog::Start(QString::fromUtf8(appName));

    if (appIconPath != nullptr) {
        const QString iconInput = QString::fromUtf8(appIconPath);
        const QString iconPath = QFileInfo::exists(iconInput)
            ? iconInput
            : Engine::Utils::SourcePath::GetResourcePath(iconInput);
        if (QFileInfo::exists(iconPath)) {
            app.setWindowIcon(QIcon(iconPath));
        }
    }

    try {
        Engine::Core::Window window(QString::fromUtf8(appName));
        const auto context = Engine::Bootstrap::Run(window);

        if (buildType == BuildType::Studio) {
            Studio::Bootstrap::Run(app, window, context);
        } else {
            window.showMaximized();
        }

        return app.exec();
    } catch (const Engine::Rendering::RenderingInitializationError& ex) {
        Engine::Core::CrashHandler::WriteCrashLog("Rendering initialization error (Exit)", QString::fromUtf8(ex.what()));
        Engine::Core::CriticalError::ShowGraphicsUnsupportedError(QString::fromUtf8(ex.what()));
    } catch (const std::exception& ex) {
        Engine::Core::CrashHandler::WriteCrashLogFromException(ex, "Unhandled exception (Exit)");
        Engine::Core::CriticalError::ShowCriticalErrorFromException(ex);
    } catch (...) {
        Engine::Core::CrashHandler::WriteCrashLog("Unknown error (Exit)");
        Engine::Core::CriticalError::ShowUnknownCriticalError();
    }

    return 1;
}

} // namespace Lvs::Launcher
