#include "Lvs/Engine/Rendering/Common/RenderScene.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"

#include <algorithm>

namespace Lvs::Engine::Rendering::Common {

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

void RenderScene::SyncProxies(SceneRenderer& renderer) {
    for (auto& proxy : partProxies_) {
        proxy->SyncFromRenderer(renderer);
    }
}

std::vector<std::shared_ptr<RenderProxy>> RenderScene::GetRenderProxies() const {
    std::vector<std::shared_ptr<RenderProxy>> proxies;
    proxies.reserve(partProxies_.size());
    for (const auto& proxy : partProxies_) {
        proxies.push_back(proxy);
    }
    return proxies;
}

void RenderScene::Clear() {
    for (auto it = instanceConnections_.begin(); it != instanceConnections_.end(); ++it) {
        it->second.ChildAdded.Disconnect();
        it->second.ChildRemoved.Disconnect();
    }
    instanceConnections_.clear();
    proxyById_.clear();
    partProxies_.clear();
}

void RenderScene::AddRecursive(const std::shared_ptr<Core::Instance>& instance) {
    if (instance == nullptr) {
        return;
    }

    const std::string id = instance->GetId().toStdString();
    if (!instanceConnections_.contains(id)) {
        InstanceConnections connections;
        connections.ChildAdded = instance->ChildAdded.Connect([this](const auto& child) { AddRecursive(child); });
        connections.ChildRemoved = instance->ChildRemoved.Connect([this](const auto& child) { RemoveRecursive(child); });
        instanceConnections_.emplace(id, std::move(connections));
    }

    if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance); part != nullptr) {
        if (!proxyById_.contains(id)) {
            auto proxy = std::make_shared<RenderPartProxy>(part);
            proxyById_.emplace(id, proxy);
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

    const std::string id = instance->GetId().toStdString();
    if (instanceConnections_.contains(id)) {
        auto& connections = instanceConnections_.at(id);
        connections.ChildAdded.Disconnect();
        connections.ChildRemoved.Disconnect();
        instanceConnections_.erase(id);
    }

    if (proxyById_.contains(id)) {
        const auto proxy = proxyById_.at(id);
        proxyById_.erase(id);
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

} // namespace Lvs::Engine::Rendering::Common
