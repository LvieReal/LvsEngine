#include "Lvs/Engine/Rendering/Vulkan/RenderScene.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Renderer.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering::Vulkan {

void RenderScene::Build(const std::shared_ptr<DataModel::Place>& place) {
    Clear();
    if (place == nullptr) {
        return;
    }

    const auto workspace = std::dynamic_pointer_cast<DataModel::Workspace>(place->FindService("Workspace"));
    if (workspace == nullptr) {
        return;
    }

    AddRecursive(workspace);
}

void RenderScene::BuildDrawLists(Renderer& renderer, const Math::Vector3& cameraPosition) {
    opaqueProxies_.clear();
    transparentProxies_.clear();
    opaqueProxies_.reserve(partProxies_.size());
    transparentProxies_.reserve(partProxies_.size());

    for (auto& proxy : partProxies_) {
        proxy->SyncFromInstance(renderer);
        if (proxy->GetAlpha() < 0.999F) {
            transparentProxies_.push_back(proxy);
        } else {
            opaqueProxies_.push_back(proxy);
        }
    }

    std::sort(transparentProxies_.begin(), transparentProxies_.end(), [&cameraPosition](const auto& a, const auto& b) {
        const double da = (a->GetWorldPosition() - cameraPosition).MagnitudeSquared();
        const double db = (b->GetWorldPosition() - cameraPosition).MagnitudeSquared();
        return da > db;
    });
}

void RenderScene::DrawOpaque(const VkCommandBuffer commandBuffer, Renderer& renderer) {
    for (auto& proxy : opaqueProxies_) {
        proxy->Draw(commandBuffer, renderer);
    }
}

void RenderScene::DrawTransparent(const VkCommandBuffer commandBuffer, Renderer& renderer) {
    for (auto& proxy : transparentProxies_) {
        proxy->Draw(commandBuffer, renderer, true);
    }
}

const std::vector<std::shared_ptr<RenderPartProxy>>& RenderScene::GetOpaqueProxies() const {
    return opaqueProxies_;
}

void RenderScene::Clear() {
    for (auto it = instanceConnections_.begin(); it != instanceConnections_.end(); ++it) {
        it->ChildAdded.Disconnect();
        it->ChildRemoved.Disconnect();
    }
    instanceConnections_.clear();
    proxyById_.clear();
    partProxies_.clear();
    opaqueProxies_.clear();
    transparentProxies_.clear();
}

void RenderScene::AddRecursive(const std::shared_ptr<Core::Instance>& instance) {
    if (instance == nullptr) {
        return;
    }

    const QString id = instance->GetId();
    if (!instanceConnections_.contains(id)) {
        InstanceConnections connections;
        connections.ChildAdded = instance->ChildAdded.Connect([this](const auto& child) { AddRecursive(child); });
        connections.ChildRemoved = instance->ChildRemoved.Connect([this](const auto& child) { RemoveRecursive(child); });
        instanceConnections_.insert(id, std::move(connections));
    }

    if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance); part != nullptr) {
        if (!proxyById_.contains(id)) {
            auto proxy = std::make_shared<RenderPartProxy>(part);
            proxyById_.insert(id, proxy);
            partProxies_.push_back(proxy);
        }
    }

    for (const auto& child : instance->GetChildren()) {
        AddRecursive(child);
    }
}

void RenderScene::RemoveRecursive(const std::shared_ptr<Core::Instance>& instance) {
    if (instance == nullptr) {
        return;
    }

    for (const auto& child : instance->GetChildren()) {
        RemoveRecursive(child);
    }

    const QString id = instance->GetId();
    if (instanceConnections_.contains(id)) {
        auto& connections = instanceConnections_[id];
        connections.ChildAdded.Disconnect();
        connections.ChildRemoved.Disconnect();
        instanceConnections_.remove(id);
    }

    if (proxyById_.contains(id)) {
        const auto proxy = proxyById_.value(id);
        proxyById_.remove(id);
        partProxies_.erase(
            std::remove_if(
                partProxies_.begin(),
                partProxies_.end(),
                [&proxy](const auto& candidate) { return candidate == proxy; }
            ),
            partProxies_.end()
        );
    }
}

} // namespace Lvs::Engine::Rendering::Vulkan
