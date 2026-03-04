#include "Lvs/Studio/Controllers/ExplorerSelectionController.hpp"

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Selection.hpp"
#include "Lvs/Studio/Widgets/Explorer/ExplorerWidget.hpp"

#include <QMetaObject>
#include <Qt>

namespace Lvs::Studio::Controllers {

ExplorerSelectionController::ExplorerSelectionController(
    const std::shared_ptr<Engine::DataModel::Place>& place,
    Widgets::Explorer::ExplorerWidget& explorerWidget
)
    : explorer_(&explorerWidget),
      alive_(std::make_shared<bool>(true)) {
    selection_ = std::dynamic_pointer_cast<Engine::DataModel::Selection>(place->FindService("Selection"));
    if (selection_ == nullptr) {
        return;
    }

    const std::weak_ptr<bool> aliveWeak = alive_;
    std::weak_ptr<Engine::DataModel::Selection> selectionWeak = selection_;
    explorerConnection_ = explorer_->InstanceActivated.Connect([aliveWeak, selectionWeak](const auto& instance) {
        if (aliveWeak.expired()) {
            return;
        }
        if (const auto selection = selectionWeak.lock(); selection != nullptr) {
            selection->Set(instance);
        }
    });
    QPointer<Widgets::Explorer::ExplorerWidget> safeExplorer = explorer_;
    selectionConnection_ = selection_->SelectionChanged.Connect([aliveWeak, safeExplorer](const auto& instances) {
        if (aliveWeak.expired() || safeExplorer.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            safeExplorer,
            [aliveWeak, safeExplorer, instances]() {
                if (aliveWeak.expired() || safeExplorer.isNull()) {
                    return;
                }
                safeExplorer->SetSelection(instances);
            },
            Qt::QueuedConnection
        );
    });
    if (!explorer_.isNull()) {
        explorer_->SetSelection(selection_->Get());
    }
}

ExplorerSelectionController::~ExplorerSelectionController() {
    Destroy();
}

void ExplorerSelectionController::Destroy() {
    alive_.reset();
    explorerConnection_.Disconnect();
    selectionConnection_.Disconnect();
}

void ExplorerSelectionController::OnExplorerActivated(const std::shared_ptr<Engine::Core::Instance>& instance) const {
    if (selection_ == nullptr) {
        return;
    }
    selection_->Set(instance);
}

void ExplorerSelectionController::OnSelectionChanged(
    const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances
) const {
    if (!explorer_.isNull()) {
        explorer_->SetSelection(instances);
    }
}

} // namespace Lvs::Studio::Controllers
