#include "Lvs/Studio/Controllers/PropertiesController.hpp"

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Selection.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertiesWidget.hpp"

#include <QMetaObject>
#include <Qt>

namespace Lvs::Studio::Controllers {

PropertiesController::PropertiesController(
    const std::shared_ptr<Engine::DataModel::Place>& place,
    Widgets::Properties::PropertiesWidget& propertiesWidget
)
    : widget_(&propertiesWidget),
      alive_(std::make_shared<bool>(true)) {
    selection_ = std::dynamic_pointer_cast<Engine::DataModel::Selection>(place->FindService("Selection"));
    if (selection_ == nullptr) {
        return;
    }

    const std::weak_ptr<bool> aliveWeak = alive_;
    QPointer<Widgets::Properties::PropertiesWidget> safeWidget = widget_;
    selectionConnection_ = selection_->SelectionChanged.Connect([aliveWeak, safeWidget](const auto& instances) {
        if (aliveWeak.expired() || safeWidget.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            safeWidget,
            [aliveWeak, safeWidget, instances]() {
                if (aliveWeak.expired() || safeWidget.isNull()) {
                    return;
                }
                if (instances.empty()) {
                    safeWidget->Clear();
                    return;
                }
                safeWidget->BindInstance(instances.front());
            },
            Qt::QueuedConnection
        );
    });
    if (!widget_.isNull()) {
        OnSelectionChanged(selection_->Get());
    }
}

PropertiesController::~PropertiesController() {
    Destroy();
}

void PropertiesController::Destroy() {
    alive_.reset();
    selectionConnection_.Disconnect();
}

void PropertiesController::OnSelectionChanged(
    const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances
) const {
    if (widget_.isNull()) {
        return;
    }
    if (instances.empty()) {
        widget_->Clear();
        return;
    }
    widget_->BindInstance(instances.front());
}

} // namespace Lvs::Studio::Controllers
