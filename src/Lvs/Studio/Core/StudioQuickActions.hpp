#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QApplication;
class QEvent;
class QKeyEvent;
class QMenu;
class QPoint;
class QWidget;

namespace Lvs::Engine {
struct EngineContext;
using EngineContextPtr = std::shared_ptr<EngineContext>;
}

namespace Lvs::Engine::Core {
class Instance;
class Viewport;
}

namespace Lvs::Studio::Controllers {
class ToolbarController;
}

namespace Lvs::Engine::DataModel {
class ChangeHistoryService;
class Place;
class Selection;
}

namespace Lvs::Engine::Objects {
class BasePart;
}

namespace Lvs::Studio::Widgets::Explorer {
class ExplorerWidget;
}

namespace Lvs::Studio::Core {

class StudioQuickActions final : public QObject {
public:
    StudioQuickActions(
        QApplication& app,
        QWidget& window,
        const Engine::EngineContextPtr& context,
        Engine::Core::Viewport* viewport,
        Controllers::ToolbarController* toolbarController
    );
    ~StudioQuickActions() override;

    void SetContext(const Engine::EngineContextPtr& context);
    bool TryShowViewportContextMenu(Engine::Core::Viewport& viewport, const QPoint& globalPos) const;
    bool TryShowExplorerContextMenu(Widgets::Explorer::ExplorerWidget& explorer, const QPoint& globalPos) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] std::shared_ptr<Engine::DataModel::Place> GetCurrentPlace() const;
    [[nodiscard]] std::shared_ptr<Engine::DataModel::Selection> GetSelectionService(
        const std::shared_ptr<Engine::DataModel::Place>& place
    ) const;
    [[nodiscard]] std::shared_ptr<Engine::DataModel::ChangeHistoryService> GetHistoryService(
        const std::shared_ptr<Engine::DataModel::Place>& place
    ) const;
    [[nodiscard]] std::shared_ptr<Engine::Objects::BasePart> GetSelectedBasePart() const;

    [[nodiscard]] bool IsTextInputFocused() const;
    [[nodiscard]] bool IsViewportShortcutContext() const;
    [[nodiscard]] bool IsExplorerShortcutContext() const;
    [[nodiscard]] bool IsQuickActionContext() const;
    [[nodiscard]] bool IsToolShortcut(const QKeyEvent& event, int key) const;
    [[nodiscard]] bool IsDuplicateShortcut(const QKeyEvent& event) const;
    [[nodiscard]] bool IsDeleteShortcut(const QKeyEvent& event) const;

    void ActivateToolShortcut(int key) const;
    void DeleteSelection() const;
    void DuplicateSelection() const;
    void PopulateInsertMenu(
        QMenu& menu,
        const std::shared_ptr<Engine::Core::Instance>& parent
    ) const;
    void InsertObject(
        const std::shared_ptr<Engine::Core::Instance>& parent,
        const QString& className
    ) const;
    bool ShowSelectedBasePartContextMenu(QWidget& owner, const QPoint& globalPos) const;
    Widgets::Explorer::ExplorerWidget* ResolveExplorerWidgetFromObject(QObject* object) const;
    std::shared_ptr<Engine::Core::Instance> CloneRecursive(
        const std::shared_ptr<Engine::Core::Instance>& source
    ) const;

    Engine::EngineContextPtr context_;
    QApplication* app_{nullptr};
    Engine::Core::Viewport* viewport_{nullptr};
    Controllers::ToolbarController* toolbarController_{nullptr};
    QWidget* window_{nullptr};
};

} // namespace Lvs::Studio::Core
