#include "Lvs/Launcher/Launcher.hpp"

#include "Lvs/Engine/Bootstrap.hpp"
#include "Lvs/Engine/Core/CriticalError.hpp"
#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
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
    } catch (const Engine::Rendering::Vulkan::VulkanInitializationError& ex) {
        Engine::Core::CriticalError::ShowVulkanUnsupportedError(QString::fromUtf8(ex.what()));
    } catch (const std::exception& ex) {
        Engine::Core::CriticalError::ShowCriticalErrorFromException(ex);
    } catch (...) {
        Engine::Core::CriticalError::ShowUnknownCriticalError();
    }

    return 1;
}

} // namespace Lvs::Launcher
