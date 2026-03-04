#pragma once

#include <QDockWidget>

#include <memory>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Studio::Widgets::Output {

class OutputDock final : public QDockWidget {
public:
    explicit OutputDock(QWidget* parent = nullptr);
    void BindToPlace(const std::shared_ptr<Lvs::Engine::DataModel::Place>& place);
    void Unbind();
};

} // namespace Lvs::Studio::Widgets::Output
