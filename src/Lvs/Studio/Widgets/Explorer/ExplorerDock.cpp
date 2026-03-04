#include "Lvs/Studio/Widgets/Explorer/ExplorerDock.hpp"

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Studio/Controllers/ExplorerSelectionController.hpp"
#include "Lvs/Studio/Widgets/Explorer/ExplorerWidget.hpp"

#include <QWidget>

namespace Lvs::Studio::Widgets::Explorer {

ExplorerDock::ExplorerDock(QWidget* parent)
    : QDockWidget("Explorer", parent) {
}

void ExplorerDock::BindToPlace(const std::shared_ptr<Lvs::Engine::DataModel::Place>& place) {
    Unbind();

    widget_ = new ExplorerWidget(place, this);
    setWidget(widget_);

    widget_->BindToRoot(place->GetDataModel());
    controller_ = std::make_unique<Controllers::ExplorerSelectionController>(place, *widget_);
}

void ExplorerDock::Unbind() {
    controller_.reset();
    if (widget() != nullptr) {
        setWidget(nullptr);
    }
    widget_ = nullptr;
}

} // namespace Lvs::Studio::Widgets::Explorer
