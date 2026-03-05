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

#include <memory>

namespace Lvs::Studio::Bootstrap {

void Run(QApplication& app, Engine::Core::Window& window, const Engine::EngineContextPtr& context) {
    if (const auto* screen = window.screen(); screen != nullptr) {
        const QSize available = screen->availableSize();
        window.resize(available);
    }

    Core::Settings::Load();
    Theme::ApplyTheme(app);

    context->PlaceManager = std::make_unique<Engine::DataModel::PlaceManager>();
    context->EditorToolState = std::make_unique<Engine::Core::EditorToolState>();

    context->ViewportManager = std::make_unique<Core::ViewportManager>(window, context);

    context->DockManager = std::make_unique<Core::DockManager>(window);
    context->DockManager->Build();

    context->TopBarController = std::make_unique<Controllers::TopBarController>(
        window,
        *context->DockManager,
        *context->PlaceManager
    );
    context->TopBarController->Build();
    window.addToolBarBreak();
    context->ToolbarController = std::make_unique<Controllers::ToolbarController>(
        window,
        *context->EditorToolState
    );
    context->ToolbarController->Build();

    context->HistoryShortcuts = std::make_unique<Core::HistoryShortcuts>(app, window, context);
    context->StudioQuickActions = std::make_unique<Core::StudioQuickActions>(app, window, context);

    context->DockManager->HidePlaceRequiredDocks();
    context->ViewportManager->Hide();
    if (context->ToolbarController != nullptr) {
        context->ToolbarController->SetVisible(false);
    }

    context->PlaceManager->PlaceOpened.Connect([context](const std::shared_ptr<Engine::DataModel::Place>& place) {
        if (context->ViewportManager != nullptr) {
            context->ViewportManager->BindToPlace(place);
            context->ViewportManager->Show();
        }
        if (context->DockManager != nullptr) {
            context->DockManager->BindToPlace(place);
            context->DockManager->ApplyPlaceRequiredDockVisibility();
        }
        if (context->ToolbarController != nullptr) {
            context->ToolbarController->SetVisible(true);
        }
    });
    context->PlaceManager->PlaceClosed.Connect([context](const std::shared_ptr<Engine::DataModel::Place>&) {
        if (context->ViewportManager != nullptr) {
            context->ViewportManager->Unbind();
            context->ViewportManager->Hide();
        }
        if (context->DockManager != nullptr) {
            context->DockManager->CachePlaceRequiredDockVisibility();
            context->DockManager->Unbind();
            context->DockManager->HidePlaceRequiredDocks();
        }
        if (context->ToolbarController != nullptr) {
            context->ToolbarController->SetVisible(false);
        }
    });

    window.showMaximized();
}

} // namespace Lvs::Studio::Bootstrap
