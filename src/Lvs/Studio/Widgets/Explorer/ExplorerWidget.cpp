#include "Lvs/Studio/Widgets/Explorer/ExplorerWidget.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/RegularError.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QCursor>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QSize>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QWidget>

#include <exception>
#include <utility>

namespace Lvs::Studio::Widgets::Explorer {

ExplorerWidget::ExplorerWidget(const std::shared_ptr<Engine::DataModel::Place>& place, QWidget* parent)
    : QTreeWidget(parent),
      place_(place) {
    if (place_ != nullptr) {
        historyService_ = std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(
            place_->FindService("ChangeHistoryService")
        );
    }

    setColumnCount(1);
    setHeaderHidden(true);
    setMouseTracking(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDefaultDropAction(Qt::MoveAction);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setContentsMargins(0, 0, 0, 0);

	QObject::connect(this, &QTreeWidget::itemSelectionChanged, this, [this]() { OnQtSelectionChanged(); });

	iconPackConnection_.Disconnect();
	iconPackConnection_ = Core::Settings::Changed("ExplorerIconPack", [this](const QVariant&) { RefreshIcons(); });

	showHiddenServicesConnection_.Disconnect();
	showHiddenServicesConnection_ = Core::Settings::Changed(
	    "ExplorerShowHiddenServices",
	    [this](const QVariant& value) {
	        showHiddenServices_ = value.toBool();
	        const auto root = rootInstance_;
	        if (root != nullptr) {
	            BindToRoot(root);
	        }
	    },
	    true
	);
}

ExplorerWidget::~ExplorerWidget() {
    Unbind();
}

void ExplorerWidget::BindToRoot(const std::shared_ptr<Engine::Core::Instance>& rootInstance) {
    Unbind();
    rootInstance_ = rootInstance;

    if (rootInstance_ == nullptr) {
        return;
    }

    for (const auto& child : rootInstance_->GetChildren()) {
        AddInstanceRecursive(nullptr, child);
    }
}

void ExplorerWidget::Unbind() {
	isUnbinding_ = true;
	HideLastHoveredPlusButton();
	const auto connectionKeys = instanceConnections_.keys();
	for (const auto& id : connectionKeys) {
	    DisconnectInstanceConnections(id);
	}

    clear();
    instanceToItem_.clear();
    instanceToNameLabel_.clear();
    instanceToIconLabel_.clear();
    instanceToPlusButton_.clear();
    instanceConnections_.clear();
    rootInstance_.reset();
    columnWidthUpdateQueued_ = false;
    isUnbinding_ = false;
}

void ExplorerWidget::SetSelection(const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances) {
    suppressSelectionSignal_ = true;
    clearSelection();

    if (!instances.empty()) {
        const auto& instance = instances.front();
        if (instance != nullptr) {
            if (QTreeWidgetItem* item = instanceToItem_.value(instance->GetId(), nullptr); item != nullptr) {
                item->setSelected(true);
                scrollToItem(item);
            }
        }
    }

    suppressSelectionSignal_ = false;
}

void ExplorerWidget::mouseMoveEvent(QMouseEvent* event) {
    QTreeWidget::mouseMoveEvent(event);

    QTreeWidgetItem* item = itemAt(event->pos());
    if (item != nullptr) {
        ShowItemPlusButton(item);
    }
    if (lastHoveredItem_ != nullptr && lastHoveredItem_ != item) {
        HideItemPlusButton(lastHoveredItem_);
    }
    lastHoveredItem_ = item;
}

void ExplorerWidget::leaveEvent(QEvent* event) {
    QTreeWidget::leaveEvent(event);
    HideLastHoveredPlusButton();
}

void ExplorerWidget::startDrag(Qt::DropActions supportedActions) {
    QTreeWidgetItem* item = currentItem();
    if (item == nullptr) {
        return;
    }

    const auto instance = ResolveItemInstance(item);
    if (instance == nullptr) {
        return;
    }

    const QString instanceId = instance->GetId();
    if (instanceId.isEmpty()) {
        return;
    }

    auto* mimeData = new QMimeData();
    mimeData->setText(instanceId);

    QDrag drag(this);
    drag.setMimeData(mimeData);
    drag.exec(Qt::MoveAction, Qt::MoveAction);

    static_cast<void>(supportedActions);
}

void ExplorerWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ExplorerWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (!event->mimeData()->hasText()) {
        event->ignore();
        return;
    }

    const auto instance = ResolveMimeInstance(event->mimeData());
    if (instance == nullptr) {
        event->ignore();
        return;
    }

    const auto target = DropTargetFromEvent(event);
    if (CanReparent(instance, target)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void ExplorerWidget::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasText()) {
        event->ignore();
        return;
    }

    const auto instance = ResolveMimeInstance(event->mimeData());
    const auto target = DropTargetFromEvent(event);

    if (!CanReparent(instance, target)) {
        event->ignore();
        return;
    }

    RecordReparentCommand(instance, target);
    event->acceptProposedAction();
}

void ExplorerWidget::OnQtSelectionChanged() {
    if (suppressSelectionSignal_) {
        return;
    }

    const QList<QTreeWidgetItem*> selected = selectedItems();
    if (selected.isEmpty()) {
        InstanceActivated.Fire(nullptr);
        return;
    }

    InstanceActivated.Fire(ResolveItemInstance(selected.front()));
}

void ExplorerWidget::AddInstanceRecursive(
    QTreeWidgetItem* parentItem,
    const std::shared_ptr<Engine::Core::Instance>& instance
) {
	if (isUnbinding_ || instance == nullptr) {
	    return;
	}
	if (!showHiddenServices_ && instance->IsHiddenService()) {
	    return;
	}
	const QString instanceId = instance->GetId();
	if (instanceId.isEmpty()) {
	    return;
	}
	if (QTreeWidgetItem* existing = instanceToItem_.value(instanceId, nullptr); existing != nullptr) {
	    const bool isHiddenService = instance->IsHiddenService();
	    existing->setHidden(isHiddenService && !showHiddenServices_);
	    if (QLabel* nameLabel = instanceToNameLabel_.value(instanceId, nullptr); nameLabel != nullptr) {
	        nameLabel->setDisabled(isHiddenService);
	    }
	    if (parentItem != nullptr && existing->parent() != parentItem) {
	        if (QTreeWidgetItem* oldParent = existing->parent(); oldParent != nullptr) {
	            oldParent->removeChild(existing);
            } else {
                const int topLevel = indexOfTopLevelItem(existing);
                if (topLevel >= 0) {
                    takeTopLevelItem(topLevel);
                }
            }
            parentItem->addChild(existing);
        } else if (parentItem == nullptr && existing->parent() != nullptr) {
            existing->parent()->removeChild(existing);
            addTopLevelItem(existing);
        }
        UpdateColumnWidthForItem(existing);
        return;
    }

	auto* item = new QTreeWidgetItem();
	item->setData(0, Qt::UserRole, instanceId);
	const bool isHiddenService = instance->IsHiddenService();
	item->setHidden(isHiddenService && !showHiddenServices_);
	item->setSizeHint(0, QSize(0, 20));

    instanceToItem_.insert(instanceId, item);

    if (parentItem != nullptr) {
        parentItem->addChild(item);
    } else {
        addTopLevelItem(item);
    }

    auto* rowWidget = new QWidget(this);
    rowWidget->setMouseTracking(true);

    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(2, 0, 2, 0);
    rowLayout->setSpacing(4);

    auto* iconLabel = new QLabel(rowWidget);
    iconLabel->setFixedSize(16, 16);
    iconLabel->setScaledContents(false);
    {
        const QPixmap icon = Core::GetIconPackManager().GetPixmapForInstance(instance);
        if (!icon.isNull()) {
            iconLabel->setPixmap(icon);
        }
    }
    rowLayout->addWidget(iconLabel);

	auto* nameLabel = new QLabel(instance->GetProperty("Name").toString(), rowWidget);
	nameLabel->setMouseTracking(true);
	nameLabel->setDisabled(isHiddenService);
	rowLayout->addWidget(nameLabel);

    auto* plusButton = new QPushButton("+", rowWidget);
    plusButton->setFixedSize(15, 15);
    plusButton->setFocusPolicy(Qt::NoFocus);
    plusButton->hide();
    rowLayout->addWidget(plusButton);

    rowLayout->addStretch(1);

    setItemWidget(item, 0, rowWidget);

    UpdateColumnWidthForItem(item);

    instanceToNameLabel_.insert(instanceId, nameLabel);
    instanceToIconLabel_.insert(instanceId, iconLabel);
    instanceToPlusButton_.insert(instanceId, plusButton);

    QObject::connect(plusButton, &QPushButton::clicked, this, [this, instance, item]() {
        ShowInsertPopup(instance, item);
    });

    DisconnectInstanceConnections(instanceId);
    InstanceConnections connections;
    connections.ChildAdded = instance->ChildAdded.Connect([this, instanceId](const auto& child) {
        AddInstanceRecursiveByParentId(instanceId, child);
    });
    connections.ChildRemoved = instance->ChildRemoved.Connect([this](const auto& child) {
        RemoveInstanceRecursive(child);
    });
    connections.PropertyChanged = instance->PropertyChanged.Connect(
        [this, id = instanceId](const QString& propertyName, const QVariant& value) {
            if (propertyName != "Name") {
                return;
            }
            if (QLabel* label = instanceToNameLabel_.value(id, nullptr); label != nullptr) {
                label->setText(value.toString());
                if (QTreeWidgetItem* item = instanceToItem_.value(id, nullptr); item != nullptr) {
                    UpdateColumnWidthForItem(item);
                }
            }
        }
    );
    instanceConnections_.insert(instanceId, std::move(connections));

    for (const auto& child : instance->GetChildren()) {
        AddInstanceRecursive(item, child);
    }

    if (instance->IsService() && instance->GetProperty("Name").toString() == "Workspace") {
        item->setExpanded(true);
    }
}

void ExplorerWidget::AddInstanceRecursiveByParentId(
    const QString& parentId,
    const std::shared_ptr<Engine::Core::Instance>& instance
) {
    if (isUnbinding_ || instance == nullptr) {
        return;
    }
    QTreeWidgetItem* parentItem = instanceToItem_.value(parentId, nullptr);
    if (parentItem == nullptr) {
        return;
    }
    AddInstanceRecursive(parentItem, instance);
}

void ExplorerWidget::RemoveInstanceRecursive(const std::shared_ptr<Engine::Core::Instance>& instance) {
    if (instance == nullptr) {
        return;
    }

    for (const auto& child : instance->GetChildren()) {
        RemoveInstanceRecursive(child);
    }

    const QString id = instance->GetId();
    DisconnectInstanceConnections(id);
    QTreeWidgetItem* item = instanceToItem_.take(id);
    instanceToNameLabel_.remove(id);
    instanceToIconLabel_.remove(id);
    instanceToPlusButton_.remove(id);
    if (item == nullptr) {
        return;
    }

    if (item == lastHoveredItem_) {
        lastHoveredItem_ = nullptr;
    }

    if (QTreeWidgetItem* parent = item->parent(); parent != nullptr) {
        parent->removeChild(item);
    } else {
        const int index = indexOfTopLevelItem(item);
        if (index >= 0) {
            takeTopLevelItem(index);
        }
    }
    delete item;
    QueueRecomputeColumnWidth();
}

void ExplorerWidget::UpdateColumnWidthForItem(QTreeWidgetItem* item) {
    if (item == nullptr) {
        return;
    }

    QWidget* rowWidget = itemWidget(item, 0);
    if (rowWidget == nullptr) {
        return;
    }

    const int indent = ComputeIndentForItem(item);
    const int neededWidth = indent + rowWidget->sizeHint().width() + 8;
    if (neededWidth > columnWidth(0)) {
        setColumnWidth(0, neededWidth);
    }
}

int ExplorerWidget::ComputeIndentForItem(const QTreeWidgetItem* item) const {
    int depth = 0;
    for (auto* parent = item != nullptr ? item->parent() : nullptr; parent != nullptr; parent = parent->parent()) {
        ++depth;
    }
    return depth * indentation();
}

void ExplorerWidget::QueueRecomputeColumnWidth() {
    if (columnWidthUpdateQueued_) {
        return;
    }
    columnWidthUpdateQueued_ = true;

    QTimer::singleShot(0, this, [this]() {
        columnWidthUpdateQueued_ = false;
        RecomputeColumnWidth();
    });
}

void ExplorerWidget::RecomputeColumnWidth() {
    int maxNeededWidth = 0;
    for (QTreeWidgetItemIterator it(this); *it != nullptr; ++it) {
        QTreeWidgetItem* item = *it;
        QWidget* rowWidget = itemWidget(item, 0);
        if (rowWidget == nullptr) {
            continue;
        }
        const int indent = ComputeIndentForItem(item);
        const int neededWidth = indent + rowWidget->sizeHint().width() + 8;
        maxNeededWidth = std::max(maxNeededWidth, neededWidth);
    }

    const int minWidth = viewport() != nullptr ? viewport()->width() : 0;
    setColumnWidth(0, std::max(maxNeededWidth, minWidth));
}

void ExplorerWidget::DisconnectInstanceConnections(const QString& instanceId) {
    if (!instanceConnections_.contains(instanceId)) {
        return;
    }
    auto& connections = instanceConnections_[instanceId];
    connections.ChildAdded.Disconnect();
    connections.ChildRemoved.Disconnect();
    connections.PropertyChanged.Disconnect();
    instanceConnections_.remove(instanceId);
}

std::shared_ptr<Engine::Core::Instance> ExplorerWidget::ResolveItemInstance(const QTreeWidgetItem* item) const {
    if (item == nullptr || rootInstance_ == nullptr) {
        return nullptr;
    }

    const QString instanceId = item->data(0, Qt::UserRole).toString();
    if (instanceId.isEmpty()) {
        return nullptr;
    }

    if (const auto dataModel = std::dynamic_pointer_cast<Engine::DataModel::DataModel>(rootInstance_);
        dataModel != nullptr) {
        return dataModel->FindInstanceById(instanceId);
    }
    return nullptr;
}

std::shared_ptr<Engine::Core::Instance> ExplorerWidget::ResolveMimeInstance(const QMimeData* mimeData) const {
    if (mimeData == nullptr || rootInstance_ == nullptr) {
        return nullptr;
    }
    if (const auto dataModel = std::dynamic_pointer_cast<Engine::DataModel::DataModel>(rootInstance_);
        dataModel != nullptr) {
        return dataModel->FindInstanceById(mimeData->text());
    }
    return nullptr;
}

std::shared_ptr<Engine::Core::Instance> ExplorerWidget::DropTargetFromEvent(const QDropEvent* event) const {
    if (event == nullptr) {
        return nullptr;
    }

    QTreeWidgetItem* item = itemAt(event->position().toPoint());
    if (item == nullptr) {
        return nullptr;
    }

    const auto target = ResolveItemInstance(item);
    return target;
}

bool ExplorerWidget::CanReparent(
    const std::shared_ptr<Engine::Core::Instance>& instance,
    const std::shared_ptr<Engine::Core::Instance>& target
) const {
    if (instance == nullptr || target == nullptr) {
        return false;
    }
    if (instance == target || instance->IsService()) {
        return false;
    }

    auto parent = target;
    while (parent != nullptr) {
        if (parent == instance) {
            return false;
        }
        parent = parent->GetParent();
    }

    if (!instance->CanParentTo(target)) {
        return false;
    }
    if (!target->CanAcceptChild(instance)) {
        return false;
    }
    return true;
}

void ExplorerWidget::RecordReparentCommand(
    const std::shared_ptr<Engine::Core::Instance>& instance,
    const std::shared_ptr<Engine::Core::Instance>& target
) const {
    try {
        auto command = std::make_shared<Engine::Utils::ReparentCommand>(instance, target);
        if (historyService_ == nullptr) {
            command->Do();
            return;
        }

        if (historyService_->IsRecording()) {
            historyService_->Record(command);
            return;
        }

        historyService_->BeginRecording("Reparent");
        try {
            historyService_->Record(command);
            historyService_->FinishRecording();
        } catch (...) {
            historyService_->FinishRecording();
            throw;
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
}

void ExplorerWidget::ShowInsertPopup(const std::shared_ptr<Engine::Core::Instance>& parent, QTreeWidgetItem* item) {
    if (parent == nullptr) {
        return;
    }

    QMenu menu(this);
    bool hasAction = false;

    const auto classGroups = Engine::DataModel::ClassRegistry::GetClassesByCategory();
    for (auto categoryIt = classGroups.cbegin(); categoryIt != classGroups.cend(); ++categoryIt) {
        auto* categoryMenu = menu.addMenu(categoryIt.key());
        bool hasCategoryAction = false;

        for (const auto& classInfo : categoryIt.value()) {
            const auto probe = classInfo.Factory();
            if (probe == nullptr || !probe->IsInsertable()) {
                continue;
            }
            if (!probe->CanParentTo(parent) || !parent->CanAcceptChild(probe)) {
                continue;
            }

            QAction* action = categoryMenu->addAction(classInfo.Name);
            const QPixmap icon = Core::GetIconPackManager().GetPixmapForInstance(probe);
            if (!icon.isNull()) {
                action->setIcon(QIcon(icon));
            }

            QObject::connect(action, &QAction::triggered, this, [parent, item, classInfo]() {
                try {
                    const auto created = classInfo.Factory();
                    if (created == nullptr) {
                        return;
                    }
                    if (!created->CanParentTo(parent) || !parent->CanAcceptChild(created)) {
                        return;
                    }
                    created->SetParent(parent);
                    if (item != nullptr) {
                        item->setExpanded(true);
                    }
                } catch (const std::exception& ex) {
                    Engine::Core::RegularError::ShowErrorFromException(ex);
                }
            });

            hasAction = true;
            hasCategoryAction = true;
        }

        if (!hasCategoryAction) {
            delete categoryMenu;
        }
    }

    if (!hasAction) {
        return;
    }

    const QPushButton* plus = instanceToPlusButton_.value(parent->GetId(), nullptr);
    const QPoint popupPos = plus != nullptr ? plus->mapToGlobal(QPoint(0, plus->height())) : QCursor::pos();
    menu.exec(popupPos);
}

void ExplorerWidget::ShowItemPlusButton(QTreeWidgetItem* item) {
    const auto instance = ResolveItemInstance(item);
    if (instance == nullptr || instance->IsHiddenService()) {
        return;
    }

    if (QPushButton* plusButton = instanceToPlusButton_.value(instance->GetId(), nullptr); plusButton != nullptr) {
        plusButton->show();
    }
}

void ExplorerWidget::HideItemPlusButton(QTreeWidgetItem* item) {
    const auto instance = ResolveItemInstance(item);
    if (instance == nullptr) {
        return;
    }

    if (QPushButton* plusButton = instanceToPlusButton_.value(instance->GetId(), nullptr); plusButton != nullptr) {
        plusButton->hide();
    }
}

void ExplorerWidget::HideLastHoveredPlusButton() {
    if (lastHoveredItem_ == nullptr) {
        return;
    }
    HideItemPlusButton(lastHoveredItem_);
    lastHoveredItem_ = nullptr;
}

void ExplorerWidget::RefreshIcons() {
    if (rootInstance_ == nullptr) {
        return;
    }

    if (const auto dataModel = std::dynamic_pointer_cast<Engine::DataModel::DataModel>(rootInstance_);
        dataModel != nullptr) {
        for (auto it = instanceToIconLabel_.begin(); it != instanceToIconLabel_.end(); ++it) {
            const auto instance = dataModel->FindInstanceById(it.key());
            if (instance == nullptr) {
                continue;
            }
            it.value()->setPixmap(Core::GetIconPackManager().GetPixmapForInstance(instance));
        }
    }
}

} // namespace Lvs::Studio::Widgets::Explorer
