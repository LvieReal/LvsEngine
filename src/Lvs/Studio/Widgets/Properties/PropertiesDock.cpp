#include "Lvs/Studio/Widgets/Properties/PropertiesDock.hpp"

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Studio/Controllers/PropertiesController.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertiesWidget.hpp"

#include <QWidget>

namespace Lvs::Studio::Widgets::Properties {

PropertiesDock::PropertiesDock(QWidget* parent)
    : QDockWidget("Properties", parent) {
}

void PropertiesDock::BindToPlace(const std::shared_ptr<Lvs::Engine::DataModel::Place>& place) {
    Unbind();

    widget_ = new PropertiesWidget(place, this);
    setWidget(widget_);

    controller_ = std::make_unique<Controllers::PropertiesController>(place, *widget_);
}

void PropertiesDock::Unbind() {
    controller_.reset();
    if (widget() != nullptr) {
        setWidget(nullptr);
    }
    widget_ = nullptr;
}

} // namespace Lvs::Studio::Widgets::Properties
