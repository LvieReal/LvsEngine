#pragma once

#include "Lvs/Engine/Utils/Signal.hpp"

#include <QPointer>

#include <memory>
#include <vector>

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::DataModel {
class Place;
class Selection;
}

namespace Lvs::Studio::Widgets::Properties {
class PropertiesWidget;
}

namespace Lvs::Studio::Controllers {

class PropertiesController final {
public:
    PropertiesController(
        const std::shared_ptr<Engine::DataModel::Place>& place,
        Widgets::Properties::PropertiesWidget& propertiesWidget
    );
    ~PropertiesController();

    void Destroy();

private:
    void OnSelectionChanged(const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances) const;

    std::shared_ptr<Engine::DataModel::Selection> selection_;
    QPointer<Widgets::Properties::PropertiesWidget> widget_;
    std::shared_ptr<bool> alive_;
    Engine::Utils::Signal<const std::vector<std::shared_ptr<Engine::Core::Instance>>&>::Connection selectionConnection_;
};

} // namespace Lvs::Studio::Controllers
