#pragma once

#include "Lvs/Engine/Utils/Signal.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <vulkan/vulkan.h>

#include <QHash>

#include <memory>
#include <optional>
#include <vector>

namespace Lvs::Engine::DataModel {
class Place;
}

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::Rendering::Vulkan {

class RenderPartProxy;
class Renderer;

class RenderScene final {
public:
    RenderScene() = default;
    ~RenderScene() = default;

    void Build(const std::shared_ptr<DataModel::Place>& place);
    void BuildDrawLists(Renderer& renderer, const Math::Vector3& cameraPosition);
    void DrawOpaque(VkCommandBuffer commandBuffer, Renderer& renderer);
    void DrawTransparent(VkCommandBuffer commandBuffer, Renderer& renderer);
    [[nodiscard]] const std::vector<std::shared_ptr<RenderPartProxy>>& GetOpaqueProxies() const;

private:
    void Clear();
    void AddRecursive(const std::shared_ptr<::Lvs::Engine::Core::Instance>& instance);
    void RemoveRecursive(const std::shared_ptr<::Lvs::Engine::Core::Instance>& instance);

    struct InstanceConnections {
        Utils::Signal<const std::shared_ptr<::Lvs::Engine::Core::Instance>&>::Connection ChildAdded;
        Utils::Signal<const std::shared_ptr<::Lvs::Engine::Core::Instance>&>::Connection ChildRemoved;
    };

    std::vector<std::shared_ptr<RenderPartProxy>> partProxies_;
    std::vector<std::shared_ptr<RenderPartProxy>> opaqueProxies_;
    std::vector<std::shared_ptr<RenderPartProxy>> transparentProxies_;
    QHash<QString, std::shared_ptr<RenderPartProxy>> proxyById_;
    QHash<QString, InstanceConnections> instanceConnections_;
};

} // namespace Lvs::Engine::Rendering::Vulkan
