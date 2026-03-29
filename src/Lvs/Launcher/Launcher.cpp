#include "Lvs/Launcher/Launcher.hpp"

#include "Lvs/Engine/Bootstrap.hpp"
#include "Lvs/Studio/Core/CrashHandler.hpp"
#include "Lvs/Studio/Core/CriticalError.hpp"
#include "Lvs/Studio/Core/SessionLog.hpp"
#include "Lvs/Studio/Core/Window.hpp"
#include "Lvs/Qt/QtBridge.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"
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

    const QStringList args = app.arguments();
    Engine::Utils::Benchmark::SetEnabled(args.contains("--bench"));

    Engine::Core::CrashHandler::Install();
    Engine::Core::SessionLog::Start(QString::fromUtf8(appName));

    if (appIconPath != nullptr) {
        const QString iconInput = QString::fromUtf8(appIconPath);
        const QString iconPath = QFileInfo::exists(iconInput)
            ? iconInput
            : Engine::Core::QtBridge::ToQString(
                Engine::Utils::SourcePath::GetResourcePath(Engine::Core::QtBridge::ToStdString(iconInput))
            );
        if (QFileInfo::exists(iconPath)) {
            app.setWindowIcon(QIcon(iconPath));
        }
    }

    try {
        Engine::Core::Window window(QString::fromUtf8(appName));
        const auto context = Engine::Bootstrap::Run();

        if (buildType == BuildType::Studio) {
            Studio::Bootstrap::Run(app, window, context);
        } else {
            window.showMaximized();
        }

        const int code = app.exec();
        Engine::Utils::Benchmark::DumpToStdout();
        return code;
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

    Engine::Utils::Benchmark::DumpToStdout();
    return 1;
}

} // namespace Lvs::Launcher
