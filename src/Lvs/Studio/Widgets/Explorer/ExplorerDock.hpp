#pragma once

#include <QDockWidget>

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Studio::Controllers {
class ExplorerSelectionController;
}

namespace Lvs::Studio::Widgets::Explorer {
class ExplorerWidget;
}

namespace Lvs::Studio::Widgets::Explorer {

class ExplorerDock final : public QDockWidget {
public:
    explicit ExplorerDock(QWidget* parent = nullptr);

    void BindToPlace(const std::shared_ptr<Lvs::Engine::DataModel::Place>& place);
    void Unbind();

private:
    ExplorerWidget* widget_{nullptr};
    std::unique_ptr<Controllers::ExplorerSelectionController> controller_;
};

} // namespace Lvs::Studio::Widgets::Explorer
