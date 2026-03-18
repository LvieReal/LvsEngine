#include "Lvs/Studio/Bootstrap.hpp"

#include "Lvs/Engine/Core/Window.hpp"
#include "Lvs/Studio/Controllers/ToolbarController.hpp"
#include "Lvs/Studio/Controllers/TopBarController.hpp"
#include "Lvs/Studio/Core/DockManager.hpp"
#include "Lvs/Studio/Core/HistoryShortcuts.hpp"
#include "Lvs/Studio/Core/Settings.hpp"
#include "Lvs/Studio/Core/StudioQuickActions.hpp"
#include "Lvs/Studio/Core/ViewportManager.hpp"
#include "Lvs/Studio/Theme.hpp"

#include <QApplication>
#include <QScreen>
#include <QSize>
#include <QStringList>

#include <memory>

namespace Lvs::Studio::Bootstrap {

namespace {

struct StudioRuntimeState {
    std::unique_ptr<Core::DockManager> DockManager;
    std::unique_ptr<Core::ViewportManager> ViewportManager;
    std::unique_ptr<Controllers::TopBarController> TopBarController;
    std::unique_ptr<Controllers::ToolbarController> ToolbarController;
    std::unique_ptr<Core::HistoryShortcuts> HistoryShortcuts;
    std::unique_ptr<Core::StudioQuickActions> StudioQuickActions;
};

} // namespace

void Run(QApplication& app, Engine::Core::Window& window, const Engine::EngineContextPtr& context) {
    static std::shared_ptr<StudioRuntimeState> studio = std::make_shared<StudioRuntimeState>();
    static bool cleanupHookInstalled = false;
    if (!cleanupHookInstalled) {
        QObject::connect(&app, &QApplication::aboutToQuit, []() {
            studio.reset();
        });
        cleanupHookInstalled = true;
    }

    if (const auto* screen = window.screen(); screen != nullptr) {
        const QSize available = screen->availableSize();
        window.resize(available);
    }

    Core::Settings::Load();
    Theme::ApplyTheme(app);

    context->PlaceManager = std::make_unique<Engine::DataModel::PlaceManager>();
    context->EditorToolState = std::make_unique<Engine::Core::EditorToolState>();

    studio->ViewportManager = std::make_unique<Core::ViewportManager>(window, context);

    studio->DockManager = std::make_unique<Core::DockManager>(window);
    studio->DockManager->Build();

    studio->TopBarController = std::make_unique<Controllers::TopBarController>(
        window,
        *studio->DockManager,
        *context->PlaceManager
    );
    studio->TopBarController->Build();
    window.addToolBarBreak();
    studio->ToolbarController = std::make_unique<Controllers::ToolbarController>(
        window,
        *context->EditorToolState
    );
    studio->ToolbarController->Build();

    studio->HistoryShortcuts = std::make_unique<Core::HistoryShortcuts>(app, window, context);
    studio->StudioQuickActions = std::make_unique<Core::StudioQuickActions>(
        app,
        window,
        context,
        studio->ViewportManager != nullptr ? studio->ViewportManager->GetViewport() : nullptr,
        studio->ToolbarController.get()
    );

    studio->DockManager->HidePlaceRequiredDocks();
    studio->ViewportManager->Hide();
    if (studio->ToolbarController != nullptr) {
        studio->ToolbarController->SetVisible(false);
    }

    context->PlaceManager->PlaceOpened.Connect([context](const std::shared_ptr<Engine::DataModel::Place>& place) {
        if (studio != nullptr && studio->ViewportManager != nullptr) {
            studio->ViewportManager->BindToPlace(place);
            studio->ViewportManager->Show();
        }
        if (studio != nullptr && studio->DockManager != nullptr) {
            studio->DockManager->BindToPlace(place);
            studio->DockManager->ApplyPlaceRequiredDockVisibility();
        }
        if (studio != nullptr && studio->ToolbarController != nullptr) {
            studio->ToolbarController->SetVisible(true);
        }
    });
    context->PlaceManager->PlaceClosed.Connect([](const std::shared_ptr<Engine::DataModel::Place>&) {
        if (studio != nullptr && studio->ViewportManager != nullptr) {
            studio->ViewportManager->Unbind();
            studio->ViewportManager->Hide();
        }
        if (studio != nullptr && studio->DockManager != nullptr) {
            studio->DockManager->CachePlaceRequiredDockVisibility();
            studio->DockManager->Unbind();
            studio->DockManager->HidePlaceRequiredDocks();
        }
        if (studio != nullptr && studio->ToolbarController != nullptr) {
            studio->ToolbarController->SetVisible(false);
        }
    });

    // Optional CLI: `--place <path>` or `--place=<path>` or a bare `*.lvsx` path.
    const QStringList args = app.arguments();
    QString placePath;
    for (int i = 0; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg == "--place" && i + 1 < args.size()) {
            placePath = args[i + 1];
            break;
        }
        if (arg.startsWith("--place=")) {
            placePath = arg.mid(QString("--place=").size());
            break;
        }
    }
    if (placePath.isEmpty()) {
        for (int i = 1; i < args.size(); ++i) {
            const QString& arg = args[i];
            if (arg.startsWith("--")) {
                continue;
            }
            if (arg.endsWith(".lvsx", Qt::CaseInsensitive)) {
                placePath = arg;
                break;
            }
        }
    }
    if (!placePath.isEmpty() && context->PlaceManager != nullptr) {
        context->PlaceManager->OpenPlaceFromFile(placePath);
    }

    window.showMaximized();
}

} // namespace Lvs::Studio::Bootstrap
