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

namespace Lvs::Studio::Widgets::Explorer {
class ExplorerWidget;
}

namespace Lvs::Studio::Controllers {

class ExplorerSelectionController final {
public:
    ExplorerSelectionController(
        const std::shared_ptr<Engine::DataModel::Place>& place,
        Widgets::Explorer::ExplorerWidget& explorerWidget
    );
    ~ExplorerSelectionController();

    void Destroy();

private:
    void OnExplorerActivated(const std::shared_ptr<Engine::Core::Instance>& instance) const;
    void OnSelectionChanged(const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances) const;

    std::shared_ptr<Engine::DataModel::Selection> selection_;
    QPointer<Widgets::Explorer::ExplorerWidget> explorer_;
    std::shared_ptr<bool> alive_;
    Engine::Utils::Signal<const std::shared_ptr<Engine::Core::Instance>&>::Connection explorerConnection_;
    Engine::Utils::Signal<const std::vector<std::shared_ptr<Engine::Core::Instance>>&>::Connection selectionConnection_;
};

} // namespace Lvs::Studio::Controllers
