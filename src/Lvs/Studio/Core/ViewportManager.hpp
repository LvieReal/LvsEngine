#pragma once

#include "Lvs/Studio/Core/Settings.hpp"

#include <memory>
#include <vector>

namespace Lvs::Engine {
struct EngineContext;
using EngineContextPtr = std::shared_ptr<EngineContext>;
}

namespace Lvs::Engine::Core {
class Viewport;
class Window;
}

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Studio::Core {

class StudioViewportToolLayer;

class ViewportManager final {
public:
    ViewportManager(Engine::Core::Window& window, const Engine::EngineContextPtr& context);
    ~ViewportManager();

    Engine::Core::Viewport* GetViewport() const;

    void BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place);
    void Unbind();
    void Show() const;
    void Hide() const;

private:
    void BindSettings();
    void ApplyMsaaSetting(const QVariant& value) const;
    void ApplySurfaceMipmappingSetting(const QVariant& value) const;

    Engine::Core::Viewport* viewport_{nullptr};
    StudioViewportToolLayer* toolLayer_{nullptr};
    std::weak_ptr<Engine::DataModel::Place> place_{};
    std::vector<Settings::Connection> settingsConnections_;
};

} // namespace Lvs::Studio::Core
