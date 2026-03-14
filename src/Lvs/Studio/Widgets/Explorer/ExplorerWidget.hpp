#pragma once

#include "Lvs/Engine/Utils/Signal.hpp"

#include <QHash>
#include <QVariant>
#include <QString>
#include <QTreeWidget>

#include <functional>
#include <memory>
#include <vector>

class QDropEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QEvent;
class QLabel;
class QMimeData;
class QMouseEvent;
class QPushButton;
class QTreeWidgetItem;

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::DataModel {
class DataModel;
class Place;
class ChangeHistoryService;
}

namespace Lvs::Studio::Widgets::Explorer {

class ExplorerWidget final : public QTreeWidget {
public:
    explicit ExplorerWidget(const std::shared_ptr<Engine::DataModel::Place>& place = nullptr, QWidget* parent = nullptr);
    ~ExplorerWidget() override;

    void BindToRoot(const std::shared_ptr<Engine::Core::Instance>& rootInstance);
    void Unbind();
    void SetSelection(const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances);

    Engine::Utils::Signal<const std::shared_ptr<Engine::Core::Instance>&> InstanceActivated;

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void OnQtSelectionChanged();
    void AddInstanceRecursive(QTreeWidgetItem* parentItem, const std::shared_ptr<Engine::Core::Instance>& instance);
    void AddInstanceRecursiveByParentId(const QString& parentId, const std::shared_ptr<Engine::Core::Instance>& instance);
    void RemoveInstanceRecursive(const std::shared_ptr<Engine::Core::Instance>& instance);
    void UpdateColumnWidthForItem(QTreeWidgetItem* item);
    int ComputeIndentForItem(const QTreeWidgetItem* item) const;
    void QueueRecomputeColumnWidth();
    void RecomputeColumnWidth();
    std::shared_ptr<Engine::Core::Instance> ResolveItemInstance(const QTreeWidgetItem* item) const;
    std::shared_ptr<Engine::Core::Instance> ResolveMimeInstance(const QMimeData* mimeData) const;
    std::shared_ptr<Engine::Core::Instance> DropTargetFromEvent(const QDropEvent* event) const;
    bool CanReparent(
        const std::shared_ptr<Engine::Core::Instance>& instance,
        const std::shared_ptr<Engine::Core::Instance>& target
    ) const;
    void RecordReparentCommand(
        const std::shared_ptr<Engine::Core::Instance>& instance,
        const std::shared_ptr<Engine::Core::Instance>& target
    ) const;
    void ShowInsertPopup(const std::shared_ptr<Engine::Core::Instance>& parent, QTreeWidgetItem* item);
    void ShowItemPlusButton(QTreeWidgetItem* item);
    void HideItemPlusButton(QTreeWidgetItem* item);
    void HideLastHoveredPlusButton();
    void RefreshIcons();
    void DisconnectInstanceConnections(const QString& instanceId);

    struct InstanceConnections {
        Engine::Utils::Signal<const std::shared_ptr<Engine::Core::Instance>&>::Connection ChildAdded;
        Engine::Utils::Signal<const std::shared_ptr<Engine::Core::Instance>&>::Connection ChildRemoved;
        Engine::Utils::Signal<const QString&, const QVariant&>::Connection PropertyChanged;
    };

    std::shared_ptr<Engine::DataModel::Place> place_;
    std::shared_ptr<Engine::DataModel::ChangeHistoryService> historyService_;
    std::shared_ptr<Engine::Core::Instance> rootInstance_;
    QHash<QString, QTreeWidgetItem*> instanceToItem_;
    QHash<QString, QLabel*> instanceToNameLabel_;
    QHash<QString, QLabel*> instanceToIconLabel_;
    QHash<QString, QPushButton*> instanceToPlusButton_;
    QTreeWidgetItem* lastHoveredItem_{nullptr};
    bool suppressSelectionSignal_{false};
    bool isUnbinding_{false};
    bool columnWidthUpdateQueued_{false};
    QHash<QString, InstanceConnections> instanceConnections_;
    std::vector<std::function<void()>> disconnectors_;
};

} // namespace Lvs::Studio::Widgets::Explorer
