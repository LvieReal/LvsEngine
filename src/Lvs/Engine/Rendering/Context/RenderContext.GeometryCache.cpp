#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace Lvs::Engine::Rendering {

void RenderContext::ClearGeometryCache() {
    for (auto& [raw, watched] : watchedNodes_) {
        static_cast<void>(raw);
        watched.ChildAdded.Disconnect();
        watched.ChildRemoved.Disconnect();
        watched.Destroying.Disconnect();
    }
    watchedNodes_.clear();

    for (auto& [raw, renderable] : renderables_) {
        static_cast<void>(raw);
        renderable.PropertyChanged.Disconnect();
        renderable.AncestryChanged.Disconnect();
        renderable.Destroying.Disconnect();
    }
    renderables_.clear();

    workspaceRoot_.reset();
    geometryCacheInitialized_ = false;
    geometryLayoutDirty_ = true;
    geometryDataDirty_ = true;
    instanceBufferDirty_ = true;
    overlayDirty_ = true;

    cachedInstanceData_.clear();
    cachedOpaqueDraws_.clear();
    cachedTransparentDraws_.clear();
    cachedAlwaysOnTopDraws_.clear();
}

void RenderContext::EnsureGeometryCache() {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("RenderContext::EnsureGeometryCache");
    }
    if (geometryCacheInitialized_) {
        if (!workspaceRoot_.expired()) {
            return;
        }
        geometryCacheInitialized_ = false;
    }

    if (place_ == nullptr) {
        ClearGeometryCache();
        return;
    }

    const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
    if (workspaceService == nullptr) {
        ClearGeometryCache();
        return;
    }

    workspaceRoot_ = workspaceService;
    WatchNodeRecursive(workspaceService);
    geometryCacheInitialized_ = true;
    MarkGeometryLayoutDirty();
}

void RenderContext::MarkGeometryLayoutDirty() {
    geometryLayoutDirty_ = true;
    geometryDataDirty_ = true;
    instanceBufferDirty_ = true;
}

void RenderContext::MarkGeometryDataDirty() {
    geometryDataDirty_ = true;
    instanceBufferDirty_ = true;
}

bool RenderContext::IsUnderWorkspace(const std::shared_ptr<Core::Instance>& instance) const {
    const auto root = workspaceRoot_.lock();
    if (root == nullptr || instance == nullptr) {
        return false;
    }
    auto current = instance;
    while (current != nullptr) {
        if (current.get() == root.get()) {
            return true;
        }
        current = current->GetParent();
    }
    return false;
}

void RenderContext::WatchNodeRecursive(const std::shared_ptr<Core::Instance>& node) {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("RenderContext::WatchNodeRecursive");
    }
    if (node == nullptr) {
        return;
    }
    if (watchedNodes_.contains(node.get())) {
        return;
    }

    WatchedNode watched{};
    watched.Instance = node;
    watched.ChildAdded = node->ChildAdded.Connect([this](const std::shared_ptr<Core::Instance>& child) {
        WatchNodeRecursive(child);
        MarkGeometryLayoutDirty();
    });
    watched.ChildRemoved = node->ChildRemoved.Connect([this](const std::shared_ptr<Core::Instance>& child) {
        UnwatchNodeRecursive(child);
        MarkGeometryLayoutDirty();
    });
    watched.Destroying = node->Destroying.Connect([this, raw = node.get()]() {
        UntrackRenderable(raw);
        const auto it = watchedNodes_.find(raw);
        if (it == watchedNodes_.end()) {
            return;
        }
        it->second.ChildAdded.Disconnect();
        it->second.ChildRemoved.Disconnect();
        it->second.Destroying.Disconnect();
        watchedNodes_.erase(it);
        MarkGeometryLayoutDirty();
    });
    watchedNodes_.emplace(node.get(), std::move(watched));

    TrackRenderable(node);
    for (const auto& child : node->GetChildren()) {
        WatchNodeRecursive(child);
    }
}

void RenderContext::UnwatchNodeRecursive(const std::shared_ptr<Core::Instance>& node) {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("RenderContext::UnwatchNodeRecursive");
    }
    if (node == nullptr) {
        return;
    }

    std::vector<std::shared_ptr<Core::Instance>> nodes;
    nodes.reserve(1);
    nodes.push_back(node);
    const auto descendants = node->GetDescendants();
    nodes.insert(nodes.end(), descendants.begin(), descendants.end());

    for (const auto& current : nodes) {
        if (current == nullptr) {
            continue;
        }
        UntrackRenderable(current.get());
        const auto it = watchedNodes_.find(current.get());
        if (it == watchedNodes_.end()) {
            continue;
        }
        it->second.ChildAdded.Disconnect();
        it->second.ChildRemoved.Disconnect();
        it->second.Destroying.Disconnect();
        watchedNodes_.erase(it);
    }
}

void RenderContext::TrackRenderable(const std::shared_ptr<Core::Instance>& instance) {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("RenderContext::TrackRenderable");
    }
    const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance);
    if (part == nullptr) {
        return;
    }
    if (renderables_.contains(part.get())) {
        return;
    }

    CachedRenderable entry{};
    entry.Instance = part;
    entry.UnderWorkspace = IsUnderWorkspace(part);

    entry.PropertyChanged = part->PropertyChanged.Connect([this, raw = part.get()](const QString& name, const QVariant&) {
        auto it = renderables_.find(raw);
        if (it == renderables_.end()) {
            return;
        }

        const bool layoutChanged =
            name == "Renders" || name == "Transparency" || name == "AlwaysOnTop" || name == "CullMode" ||
            name == "Shape" || name == "ContentId" || name == "SmoothNormals";
        const bool dataChanged =
            name == "CFrame" || name == "Position" || name == "Rotation" || name == "Size" || name == "Color" ||
            name == "Metalness" || name == "Roughness" || name == "Emissive" ||
            name == "RightSurface" || name == "LeftSurface" || name == "TopSurface" || name == "BottomSurface" ||
            name == "FrontSurface" || name == "BackSurface";

        if (layoutChanged) {
            it->second.LayoutDirty = true;
            MarkGeometryLayoutDirty();
            return;
        }
        if (dataChanged) {
            it->second.DataDirty = true;
            MarkGeometryDataDirty();
        }
    });

    entry.AncestryChanged = part->AncestryChanged.Connect([this, raw = part.get()](const std::shared_ptr<Core::Instance>&) {
        auto it = renderables_.find(raw);
        if (it == renderables_.end()) {
            return;
        }
        const auto inst = it->second.Instance.lock();
        const bool underWorkspace = IsUnderWorkspace(inst);
        if (underWorkspace != it->second.UnderWorkspace) {
            it->second.UnderWorkspace = underWorkspace;
            it->second.LayoutDirty = true;
            MarkGeometryLayoutDirty();
            return;
        }
        it->second.DataDirty = true;
        MarkGeometryDataDirty();
    });

    entry.Destroying = part->Destroying.Connect([this, raw = part.get()]() {
        UntrackRenderable(raw);
    });

    renderables_.emplace(part.get(), std::move(entry));
    MarkGeometryLayoutDirty();
}

void RenderContext::UntrackRenderable(const Core::Instance* instance) {
    const auto it = renderables_.find(instance);
    if (it == renderables_.end()) {
        return;
    }
    it->second.PropertyChanged.Disconnect();
    it->second.AncestryChanged.Disconnect();
    it->second.Destroying.Disconnect();
    renderables_.erase(it);
    MarkGeometryLayoutDirty();
}

void RenderContext::UpdateDirtyInstanceData() {
    LVS_BENCH_SCOPE("RenderContext::UpdateDirtyInstanceData");
    if (!geometryDataDirty_ || geometryLayoutDirty_) {
        return;
    }

    for (auto& [raw, entry] : renderables_) {
        static_cast<void>(raw);
        if (!entry.DataDirty || entry.LayoutDirty) {
            continue;
        }
        if (!entry.UnderWorkspace || !entry.Visible || entry.Mesh == nullptr) {
            entry.DataDirty = false;
            continue;
        }

        const auto inst = entry.Instance.lock();
        const auto part = std::dynamic_pointer_cast<Objects::BasePart>(inst);
        if (part == nullptr) {
            entry.DataDirty = false;
            continue;
        }

        const double transparency = part->GetProperty("Transparency").toDouble();
        const float alpha = static_cast<float>(1.0 - std::clamp(transparency, 0.0, 1.0));
        const Math::Vector3 size = part->GetProperty("Size").value<Math::Vector3>();
        if (!part->GetProperty("Renders").toBool() || transparency >= 1.0 || size.x <= 0.0 || size.y <= 0.0 || size.z <= 0.0) {
            entry.LayoutDirty = true;
            MarkGeometryLayoutDirty();
            return;
        }

        const Math::Matrix4 model = part->GetWorldCFrame().ToMatrix4() * Math::Matrix4::Scale(size);
        const Math::Color3 color = part->GetProperty("Color").value<Math::Color3>();

        Common::DrawInstanceData drawInstance{};
        drawInstance.Model = Context::ToFloatMat4ColumnMajor(model);
        drawInstance.BaseColor = Context::ToVec4(color, alpha);
        drawInstance.Material = {
            static_cast<float>(std::clamp(part->GetProperty("Metalness").toDouble(), 0.0, 1.0)),
            static_cast<float>(std::clamp(part->GetProperty("Roughness").toDouble(), 0.0, 1.0)),
            static_cast<float>(std::max(0.0, part->GetProperty("Emissive").toDouble())),
            0.0F
        };
        drawInstance.SurfaceData0 = {0.0F, 0.0F, 0.0F, 0.0F};
        drawInstance.SurfaceData1 = {0.0F, 0.0F, hasSurfaceAtlas_ ? 1.0F : 0.0F, 0.0F};
        if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(inst); partInstance != nullptr) {
            drawInstance.SurfaceData0 = {
                static_cast<float>(partInstance->GetProperty("TopSurface").value<Enums::PartSurfaceType>()),
                static_cast<float>(partInstance->GetProperty("BottomSurface").value<Enums::PartSurfaceType>()),
                static_cast<float>(partInstance->GetProperty("FrontSurface").value<Enums::PartSurfaceType>()),
                static_cast<float>(partInstance->GetProperty("BackSurface").value<Enums::PartSurfaceType>())
            };
            drawInstance.SurfaceData1[0] = static_cast<float>(partInstance->GetProperty("LeftSurface").value<Enums::PartSurfaceType>());
            drawInstance.SurfaceData1[1] = static_cast<float>(partInstance->GetProperty("RightSurface").value<Enums::PartSurfaceType>());
        }

        if (entry.PackedInstanceIndex < cachedInstanceData_.size()) {
            cachedInstanceData_[entry.PackedInstanceIndex] = drawInstance;
        }
        entry.InstanceData = drawInstance;
        entry.Alpha = alpha;
        entry.DataDirty = false;
    }

    geometryDataDirty_ = false;
}

void RenderContext::RebuildGeometryBatchesAndInstances() {
    LVS_BENCH_SCOPE("RenderContext::RebuildGeometryBatchesAndInstances");
    cachedInstanceData_.clear();
    cachedOpaqueDraws_.clear();
    cachedTransparentDraws_.clear();
    cachedAlwaysOnTopDraws_.clear();

    std::unordered_map<BatchKey, std::vector<CachedRenderable*>, BatchKeyHash> opaqueBatches;
    std::unordered_map<BatchKey, std::vector<CachedRenderable*>, BatchKeyHash> onTopBatches;
    std::vector<CachedRenderable*> transparentItems;

    const auto getMeshRefForInstance = [this](const std::shared_ptr<Core::Instance>& inst) -> const SceneData::MeshRef* {
        if (const auto meshPart = std::dynamic_pointer_cast<Objects::MeshPart>(inst); meshPart != nullptr) {
            const std::string contentId = meshPart->GetProperty("ContentId").toString().toStdString();
            const bool smoothNormals = meshPart->GetProperty("SmoothNormals").toBool();
            GpuMesh* gpuMesh = GetOrCreateMeshPartMesh(contentId, smoothNormals);
            if (gpuMesh == nullptr) {
                return nullptr;
            }
            const auto resolvedPath = Context::ResolveContentPath(contentId);
            if (resolvedPath.empty()) {
                return nullptr;
            }
            const std::string key = resolvedPath.string() + (smoothNormals ? "|smooth" : "|flat");
            return GetOrCreateMeshRef("meshpart:" + key, *gpuMesh);
        }

        Enums::PartShape shape = Enums::PartShape::Cube;
        if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(inst); partInstance != nullptr) {
            shape = partInstance->GetProperty("Shape").value<Enums::PartShape>();
        }
        GpuMesh* gpuMesh = GetOrCreatePrimitiveMesh(shape);
        if (gpuMesh == nullptr) {
            return nullptr;
        }
        return GetOrCreateMeshRef("primitive:" + std::to_string(static_cast<int>(shape)), *gpuMesh);
    };

    for (auto& [raw, entry] : renderables_) {
        static_cast<void>(raw);
        const auto inst = entry.Instance.lock();
        if (inst == nullptr) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            continue;
        }

        entry.UnderWorkspace = IsUnderWorkspace(inst);
        if (!entry.UnderWorkspace) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            entry.LayoutDirty = false;
            entry.DataDirty = false;
            continue;
        }

        const auto part = std::dynamic_pointer_cast<Objects::BasePart>(inst);
        if (part == nullptr) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            continue;
        }

        if (!part->GetProperty("Renders").toBool()) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            entry.LayoutDirty = false;
            entry.DataDirty = false;
            continue;
        }

        const double transparency = part->GetProperty("Transparency").toDouble();
        if (transparency >= 1.0) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            entry.LayoutDirty = false;
            entry.DataDirty = false;
            continue;
        }

        const Math::Vector3 size = part->GetProperty("Size").value<Math::Vector3>();
        if (size.x <= 0.0 || size.y <= 0.0 || size.z <= 0.0) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            entry.LayoutDirty = false;
            entry.DataDirty = false;
            continue;
        }

        const auto* meshRef = getMeshRefForInstance(inst);
        if (meshRef == nullptr) {
            entry.Visible = false;
            entry.Mesh = nullptr;
            entry.LayoutDirty = false;
            entry.DataDirty = false;
            continue;
        }

        const float alpha = static_cast<float>(1.0 - std::clamp(transparency, 0.0, 1.0));
        const bool alwaysOnTop = part->GetProperty("AlwaysOnTop").toBool();

        entry.Mesh = meshRef;
        entry.CullMode = Context::ToRhiCullMode(part->GetProperty("CullMode").value<Enums::MeshCullMode>());
        entry.Visible = true;
        entry.Alpha = alpha;
        entry.AlwaysOnTop = alwaysOnTop;
        entry.Transparent = (alpha < 1.0F) || alwaysOnTop;
        entry.IgnoreLighting = false;
        entry.LayoutDirty = false;
        entry.DataDirty = false;

        const Math::Matrix4 model = part->GetWorldCFrame().ToMatrix4() * Math::Matrix4::Scale(size);
        const Math::Color3 color = part->GetProperty("Color").value<Math::Color3>();
        Common::DrawInstanceData drawInstance{};
        drawInstance.Model = Context::ToFloatMat4ColumnMajor(model);
        drawInstance.BaseColor = Context::ToVec4(color, alpha);
        drawInstance.Material = {
            static_cast<float>(std::clamp(part->GetProperty("Metalness").toDouble(), 0.0, 1.0)),
            static_cast<float>(std::clamp(part->GetProperty("Roughness").toDouble(), 0.0, 1.0)),
            static_cast<float>(std::max(0.0, part->GetProperty("Emissive").toDouble())),
            0.0F
        };
        drawInstance.SurfaceData0 = {0.0F, 0.0F, 0.0F, 0.0F};
        drawInstance.SurfaceData1 = {0.0F, 0.0F, hasSurfaceAtlas_ ? 1.0F : 0.0F, 0.0F};
        if (const auto partInstance = std::dynamic_pointer_cast<Objects::Part>(inst); partInstance != nullptr) {
            drawInstance.SurfaceData0 = {
                static_cast<float>(partInstance->GetProperty("TopSurface").value<Enums::PartSurfaceType>()),
                static_cast<float>(partInstance->GetProperty("BottomSurface").value<Enums::PartSurfaceType>()),
                static_cast<float>(partInstance->GetProperty("FrontSurface").value<Enums::PartSurfaceType>()),
                static_cast<float>(partInstance->GetProperty("BackSurface").value<Enums::PartSurfaceType>())
            };
            drawInstance.SurfaceData1[0] = static_cast<float>(partInstance->GetProperty("LeftSurface").value<Enums::PartSurfaceType>());
            drawInstance.SurfaceData1[1] = static_cast<float>(partInstance->GetProperty("RightSurface").value<Enums::PartSurfaceType>());
        }
        entry.InstanceData = drawInstance;

        if (alwaysOnTop) {
            onTopBatches[BatchKey{meshRef, entry.CullMode, true}].push_back(&entry);
        } else if (alpha < 1.0F) {
            transparentItems.push_back(&entry);
        } else {
            opaqueBatches[BatchKey{meshRef, entry.CullMode, false}].push_back(&entry);
        }
    }

    struct OverlayItem {
        const SceneData::MeshRef* Mesh{nullptr};
        RHI::CullMode CullMode{RHI::CullMode::Back};
        bool AlwaysOnTop{true};
        bool Transparent{true};
        bool IgnoreLighting{false};
        Common::DrawInstanceData Data{};
    };

    std::unordered_map<BatchKey, std::vector<OverlayItem>, BatchKeyHash> overlayOpaque;
    std::unordered_map<BatchKey, std::vector<OverlayItem>, BatchKeyHash> overlayOnTop;
    std::vector<OverlayItem> overlayTransparent;

    for (const auto& overlay : overlayPrimitives_) {
        const float alpha = std::clamp(overlay.Alpha, 0.0F, 1.0F);
        if (alpha <= 0.0F) {
            continue;
        }
        GpuMesh* gpuMesh = GetOrCreatePrimitiveMesh(overlay.Shape);
        if (gpuMesh == nullptr) {
            continue;
        }
        const SceneData::MeshRef* meshRef = GetOrCreateMeshRef(
            "primitive:" + std::to_string(static_cast<int>(overlay.Shape)),
            *gpuMesh
        );
        if (meshRef == nullptr) {
            continue;
        }

        OverlayItem item{};
        item.Mesh = meshRef;
        item.CullMode = RHI::CullMode::Back;
        item.AlwaysOnTop = overlay.AlwaysOnTop;
        item.Transparent = alpha < 1.0F || overlay.AlwaysOnTop;
        item.IgnoreLighting = overlay.IgnoreLighting;
        item.Data.Model = Context::ToFloatMat4ColumnMajor(overlay.Model);
        item.Data.BaseColor = Context::ToVec4(overlay.Color, alpha);
        item.Data.Material = {
            std::clamp(overlay.Metalness, 0.0F, 1.0F),
            std::clamp(overlay.Roughness, 0.0F, 1.0F),
            std::max(0.0F, overlay.Emissive),
            overlay.IgnoreLighting ? 1.0F : 0.0F
        };
        item.Data.SurfaceData0 = {0.0F, 0.0F, 0.0F, overlay.AlwaysOnTop ? 1.0F : 0.0F};
        item.Data.SurfaceData1 = {0.0F, 0.0F, 0.0F, 0.0F};

        if (item.AlwaysOnTop) {
            overlayOnTop[BatchKey{meshRef, item.CullMode, true}].push_back(item);
        } else if (item.Transparent) {
            overlayTransparent.push_back(item);
        } else {
            overlayOpaque[BatchKey{meshRef, item.CullMode, false}].push_back(item);
        }
    }

    const auto packBatch = [this](
                               const BatchKey& key,
                               const std::vector<CachedRenderable*>& items,
                               std::vector<SceneData::DrawPacket>& outDraws,
                               const bool transparentFlag,
                               const bool alwaysOnTopFlag
                           ) {
        if (items.empty()) {
            return;
        }
        const RHI::u32 baseInstance = static_cast<RHI::u32>(cachedInstanceData_.size());
        for (auto* item : items) {
            item->PackedInstanceIndex = cachedInstanceData_.size();
            cachedInstanceData_.push_back(item->InstanceData);
        }
        outDraws.push_back(SceneData::DrawPacket{
            .Mesh = key.Mesh,
            .BaseInstance = baseInstance,
            .InstanceCount = static_cast<RHI::u32>(items.size()),
            .CullMode = key.CullMode,
            .Transparent = transparentFlag,
            .AlwaysOnTop = alwaysOnTopFlag,
            .IgnoreLighting = false,
            .SortDepth = 0.0F
        });
    };

    const auto packOverlayBatch = [this](
                                      const BatchKey& key,
                                      const std::vector<OverlayItem>& items,
                                      std::vector<SceneData::DrawPacket>& outDraws,
                                      const bool transparentFlag,
                                      const bool alwaysOnTopFlag
                                  ) {
        if (items.empty()) {
            return;
        }
        const RHI::u32 baseInstance = static_cast<RHI::u32>(cachedInstanceData_.size());
        bool ignoreLighting = false;
        for (const auto& item : items) {
            ignoreLighting = ignoreLighting || item.IgnoreLighting;
            cachedInstanceData_.push_back(item.Data);
        }
        outDraws.push_back(SceneData::DrawPacket{
            .Mesh = key.Mesh,
            .BaseInstance = baseInstance,
            .InstanceCount = static_cast<RHI::u32>(items.size()),
            .CullMode = key.CullMode,
            .Transparent = transparentFlag,
            .AlwaysOnTop = alwaysOnTopFlag,
            .IgnoreLighting = ignoreLighting,
            .SortDepth = 0.0F
        });
    };

    auto batchKeyLess = [](const BatchKey& lhs, const BatchKey& rhs) {
        if (lhs.Mesh != rhs.Mesh) {
            return lhs.Mesh < rhs.Mesh;
        }
        if (lhs.CullMode != rhs.CullMode) {
            return static_cast<std::size_t>(lhs.CullMode) < static_cast<std::size_t>(rhs.CullMode);
        }
        return lhs.AlwaysOnTop < rhs.AlwaysOnTop;
    };

    std::vector<BatchKey> keys;
    keys.reserve(opaqueBatches.size());
    for (const auto& [key, items] : opaqueBatches) {
        static_cast<void>(items);
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end(), batchKeyLess);
    for (const auto& key : keys) {
        packBatch(key, opaqueBatches.at(key), cachedOpaqueDraws_, false, false);
    }

    for (auto* item : transparentItems) {
        const RHI::u32 baseInstance = static_cast<RHI::u32>(cachedInstanceData_.size());
        item->PackedInstanceIndex = cachedInstanceData_.size();
        cachedInstanceData_.push_back(item->InstanceData);
        cachedTransparentDraws_.push_back(SceneData::DrawPacket{
            .Mesh = item->Mesh,
            .BaseInstance = baseInstance,
            .InstanceCount = 1U,
            .CullMode = item->CullMode,
            .Transparent = true,
            .AlwaysOnTop = false,
            .IgnoreLighting = false,
            .SortDepth = 0.0F
        });
    }

    keys.clear();
    keys.reserve(onTopBatches.size());
    for (const auto& [key, items] : onTopBatches) {
        static_cast<void>(items);
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end(), batchKeyLess);
    for (const auto& key : keys) {
        packBatch(key, onTopBatches.at(key), cachedAlwaysOnTopDraws_, true, true);
    }

    cachedGeometryInstanceCount_ = cachedInstanceData_.size();
    cachedGeometryOpaqueDrawCount_ = cachedOpaqueDraws_.size();
    cachedGeometryTransparentDrawCount_ = cachedTransparentDraws_.size();
    cachedGeometryAlwaysOnTopDrawCount_ = cachedAlwaysOnTopDraws_.size();

    keys.clear();
    keys.reserve(overlayOpaque.size());
    for (const auto& [key, items] : overlayOpaque) {
        static_cast<void>(items);
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end(), batchKeyLess);
    for (const auto& key : keys) {
        packOverlayBatch(key, overlayOpaque.at(key), cachedOpaqueDraws_, false, false);
    }

    for (const auto& item : overlayTransparent) {
        const RHI::u32 baseInstance = static_cast<RHI::u32>(cachedInstanceData_.size());
        cachedInstanceData_.push_back(item.Data);
        cachedTransparentDraws_.push_back(SceneData::DrawPacket{
            .Mesh = item.Mesh,
            .BaseInstance = baseInstance,
            .InstanceCount = 1U,
            .CullMode = item.CullMode,
            .Transparent = true,
            .AlwaysOnTop = false,
            .IgnoreLighting = item.IgnoreLighting,
            .SortDepth = 0.0F
        });
    }

    keys.clear();
    keys.reserve(overlayOnTop.size());
    for (const auto& [key, items] : overlayOnTop) {
        static_cast<void>(items);
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end(), batchKeyLess);
    for (const auto& key : keys) {
        packOverlayBatch(key, overlayOnTop.at(key), cachedAlwaysOnTopDraws_, true, true);
    }

    geometryLayoutDirty_ = false;
    geometryDataDirty_ = false;
    instanceBufferDirty_ = true;
    overlayDirty_ = false;
}

void RenderContext::UpdateTransparentSortDepths() {
    LVS_BENCH_SCOPE("RenderContext::UpdateTransparentSortDepths");
    bool hasCameraPosition = false;
    Math::Vector3 cameraPosition{};
    if (place_ != nullptr) {
        if (const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
            workspaceService != nullptr) {
            if (const auto camera = workspaceService->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
                camera != nullptr) {
                cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
                hasCameraPosition = true;
            }
        }
    }

    if (!hasCameraPosition) {
        for (auto& draw : cachedTransparentDraws_) {
            draw.SortDepth = 0.0F;
        }
        return;
    }

    for (auto& draw : cachedTransparentDraws_) {
        if (draw.BaseInstance >= cachedInstanceData_.size()) {
            draw.SortDepth = 0.0F;
            continue;
        }
        const auto& inst = cachedInstanceData_[draw.BaseInstance];
        const float x = inst.Model[12];
        const float y = inst.Model[13];
        const float z = inst.Model[14];
        const double dx = static_cast<double>(x) - cameraPosition.x;
        const double dy = static_cast<double>(y) - cameraPosition.y;
        const double dz = static_cast<double>(z) - cameraPosition.z;
        draw.SortDepth = static_cast<float>(dx * dx + dy * dy + dz * dz);
    }
}

void RenderContext::RebuildOverlayBatchesAndInstances() {
    LVS_BENCH_SCOPE("RenderContext::RebuildOverlayBatchesAndInstances");

    // If geometry is dirty, fall back to full rebuild.
    if (geometryLayoutDirty_) {
        RebuildGeometryBatchesAndInstances();
        return;
    }

    cachedInstanceData_.resize(cachedGeometryInstanceCount_);
    cachedOpaqueDraws_.resize(cachedGeometryOpaqueDrawCount_);
    cachedTransparentDraws_.resize(cachedGeometryTransparentDrawCount_);
    cachedAlwaysOnTopDraws_.resize(cachedGeometryAlwaysOnTopDrawCount_);

    struct OverlayItem {
        const SceneData::MeshRef* Mesh{nullptr};
        RHI::CullMode CullMode{RHI::CullMode::Back};
        bool AlwaysOnTop{true};
        bool Transparent{true};
        bool IgnoreLighting{false};
        Common::DrawInstanceData Data{};
    };

    std::unordered_map<BatchKey, std::vector<OverlayItem>, BatchKeyHash> overlayOpaque;
    std::unordered_map<BatchKey, std::vector<OverlayItem>, BatchKeyHash> overlayOnTop;
    std::vector<OverlayItem> overlayTransparent;

    for (const auto& overlay : overlayPrimitives_) {
        const float alpha = std::clamp(overlay.Alpha, 0.0F, 1.0F);
        if (alpha <= 0.0F) {
            continue;
        }
        GpuMesh* gpuMesh = GetOrCreatePrimitiveMesh(overlay.Shape);
        if (gpuMesh == nullptr) {
            continue;
        }
        const SceneData::MeshRef* meshRef = GetOrCreateMeshRef(
            "primitive:" + std::to_string(static_cast<int>(overlay.Shape)),
            *gpuMesh
        );
        if (meshRef == nullptr) {
            continue;
        }

        OverlayItem item{};
        item.Mesh = meshRef;
        item.CullMode = RHI::CullMode::Back;
        item.AlwaysOnTop = overlay.AlwaysOnTop;
        item.Transparent = alpha < 1.0F || overlay.AlwaysOnTop;
        item.IgnoreLighting = overlay.IgnoreLighting;
        item.Data.Model = Context::ToFloatMat4ColumnMajor(overlay.Model);
        item.Data.BaseColor = Context::ToVec4(overlay.Color, alpha);
        item.Data.Material = {
            std::clamp(overlay.Metalness, 0.0F, 1.0F),
            std::clamp(overlay.Roughness, 0.0F, 1.0F),
            std::max(0.0F, overlay.Emissive),
            overlay.IgnoreLighting ? 1.0F : 0.0F
        };
        item.Data.SurfaceData0 = {0.0F, 0.0F, 0.0F, overlay.AlwaysOnTop ? 1.0F : 0.0F};
        item.Data.SurfaceData1 = {0.0F, 0.0F, 0.0F, 0.0F};

        if (item.AlwaysOnTop) {
            overlayOnTop[BatchKey{meshRef, item.CullMode, true}].push_back(item);
        } else if (item.Transparent) {
            overlayTransparent.push_back(item);
        } else {
            overlayOpaque[BatchKey{meshRef, item.CullMode, false}].push_back(item);
        }
    }

    const auto packOverlayBatch = [this](
                                      const BatchKey& key,
                                      const std::vector<OverlayItem>& items,
                                      std::vector<SceneData::DrawPacket>& outDraws,
                                      const bool transparentFlag,
                                      const bool alwaysOnTopFlag
                                  ) {
        if (items.empty()) {
            return;
        }
        const RHI::u32 baseInstance = static_cast<RHI::u32>(cachedInstanceData_.size());
        bool ignoreLighting = false;
        for (const auto& item : items) {
            ignoreLighting = ignoreLighting || item.IgnoreLighting;
            cachedInstanceData_.push_back(item.Data);
        }
        outDraws.push_back(SceneData::DrawPacket{
            .Mesh = key.Mesh,
            .BaseInstance = baseInstance,
            .InstanceCount = static_cast<RHI::u32>(items.size()),
            .CullMode = key.CullMode,
            .Transparent = transparentFlag,
            .AlwaysOnTop = alwaysOnTopFlag,
            .IgnoreLighting = ignoreLighting,
            .SortDepth = 0.0F
        });
    };

    auto batchKeyLess = [](const BatchKey& lhs, const BatchKey& rhs) {
        if (lhs.Mesh != rhs.Mesh) {
            return lhs.Mesh < rhs.Mesh;
        }
        if (lhs.CullMode != rhs.CullMode) {
            return static_cast<std::size_t>(lhs.CullMode) < static_cast<std::size_t>(rhs.CullMode);
        }
        return lhs.AlwaysOnTop < rhs.AlwaysOnTop;
    };

    std::vector<BatchKey> keys;
    keys.reserve(overlayOpaque.size());
    for (const auto& [key, items] : overlayOpaque) {
        static_cast<void>(items);
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end(), batchKeyLess);
    for (const auto& key : keys) {
        packOverlayBatch(key, overlayOpaque.at(key), cachedOpaqueDraws_, false, false);
    }

    for (const auto& item : overlayTransparent) {
        const RHI::u32 baseInstance = static_cast<RHI::u32>(cachedInstanceData_.size());
        cachedInstanceData_.push_back(item.Data);
        cachedTransparentDraws_.push_back(SceneData::DrawPacket{
            .Mesh = item.Mesh,
            .BaseInstance = baseInstance,
            .InstanceCount = 1U,
            .CullMode = item.CullMode,
            .Transparent = true,
            .AlwaysOnTop = false,
            .IgnoreLighting = item.IgnoreLighting,
            .SortDepth = 0.0F
        });
    }

    keys.clear();
    keys.reserve(overlayOnTop.size());
    for (const auto& [key, items] : overlayOnTop) {
        static_cast<void>(items);
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end(), batchKeyLess);
    for (const auto& key : keys) {
        packOverlayBatch(key, overlayOnTop.at(key), cachedAlwaysOnTopDraws_, true, true);
    }

    overlayDirty_ = false;
    instanceBufferDirty_ = true;
}

std::vector<SceneData::DrawPacket> RenderContext::BuildGeometryDraws() {
    LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws(Internal)");
    {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws::EnsureGeometryCache");
        EnsureGeometryCache();
    }

    if (geometryLayoutDirty_ || overlayDirty_) {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws::RebuildBatches");
        if (geometryLayoutDirty_) {
            RebuildGeometryBatchesAndInstances();
        } else {
            RebuildOverlayBatchesAndInstances();
        }
    } else if (geometryDataDirty_) {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws::UpdateDirtyInstances");
        UpdateDirtyInstanceData();
    }

    {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws::UpdateTransparentSortDepths");
        UpdateTransparentSortDepths();
    }

    std::vector<const SceneData::DrawPacket*> transparentPtrs;
    {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws::SortTransparent");
        transparentPtrs.reserve(cachedTransparentDraws_.size());
        for (const auto& draw : cachedTransparentDraws_) {
            transparentPtrs.push_back(&draw);
        }
        std::sort(transparentPtrs.begin(), transparentPtrs.end(), [](const auto* lhs, const auto* rhs) {
            return lhs->SortDepth > rhs->SortDepth;
        });
    }

    std::vector<SceneData::DrawPacket> draws;
    {
        LVS_BENCH_SCOPE("RenderContext::BuildGeometryDraws::AssembleDrawList");
        draws.reserve(cachedOpaqueDraws_.size() + cachedTransparentDraws_.size() + cachedAlwaysOnTopDraws_.size());
        draws.insert(draws.end(), cachedOpaqueDraws_.begin(), cachedOpaqueDraws_.end());
        for (const auto* draw : transparentPtrs) {
            draws.push_back(*draw);
        }
        draws.insert(draws.end(), cachedAlwaysOnTopDraws_.begin(), cachedAlwaysOnTopDraws_.end());
    }
    return draws;
}

} // namespace Lvs::Engine::Rendering

