#pragma once

#include <QDockWidget>

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Studio::Controllers {
class PropertiesController;
}

namespace Lvs::Studio::Widgets::Properties {
class PropertiesWidget;
}

namespace Lvs::Studio::Widgets::Properties {

class PropertiesDock final : public QDockWidget {
public:
    explicit PropertiesDock(QWidget* parent = nullptr);

    void BindToPlace(const std::shared_ptr<Lvs::Engine::DataModel::Place>& place);
    void Unbind();

private:
    PropertiesWidget* widget_{nullptr};
    std::unique_ptr<Controllers::PropertiesController> controller_;
};

} // namespace Lvs::Studio::Widgets::Properties
