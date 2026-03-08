#pragma once

#include "Lvs/Engine/Rendering/Common/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::Rendering::Common {

class RenderScene final {
public:
    RenderScene() = default;
    ~RenderScene() = default;

    void Build(const std::shared_ptr<DataModel::Place>& place);
    void SyncProxies(SceneRenderer& renderer);
    [[nodiscard]] std::vector<std::shared_ptr<RenderProxy>> GetRenderProxies() const;

private:
    void Clear();
    void AddRecursive(const std::shared_ptr<::Lvs::Engine::Core::Instance>& instance);
    void RemoveRecursive(const std::shared_ptr<::Lvs::Engine::Core::Instance>& instance);

    struct InstanceConnections {
        Utils::Signal<const std::shared_ptr<::Lvs::Engine::Core::Instance>&>::Connection ChildAdded;
        Utils::Signal<const std::shared_ptr<::Lvs::Engine::Core::Instance>&>::Connection ChildRemoved;
    };

    std::vector<std::shared_ptr<RenderPartProxy>> partProxies_;
    std::unordered_map<std::string, std::shared_ptr<RenderPartProxy>> proxyById_;
    std::unordered_map<std::string, InstanceConnections> instanceConnections_;
};

} // namespace Lvs::Engine::Rendering::Common
