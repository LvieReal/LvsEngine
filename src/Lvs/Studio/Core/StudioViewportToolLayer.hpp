#pragma once

#include "Lvs/Studio/Core/ViewportToolLayer.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/GizmoSystem.hpp"
#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include <QPoint>
#include <QRect>
#include <Qt>

class QMouseEvent;
class QRubberBand;

namespace Lvs::Engine {
struct EngineContext;
using EngineContextPtr = std::shared_ptr<EngineContext>;
}

namespace Lvs::Engine::Core {
class Viewport;
}

namespace Lvs::Engine::DataModel {
class ChangeHistoryService;
class Place;
class Selection;
class Workspace;
}

namespace Lvs::Engine::Objects {
class BasePart;
class SelectionBox;
}

namespace Lvs::Studio::Core {

class StudioViewportToolLayer final : public Engine::Core::ViewportToolLayer {
public:
    StudioViewportToolLayer(Engine::Core::Viewport& viewport, const Engine::EngineContextPtr& context);
    ~StudioViewportToolLayer() override;

    void BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place) override;
    void Unbind() override;

    void OnFrame(double deltaSeconds, const std::optional<Engine::Utils::Ray>& cursorRay) override;
    void OnMousePress(QMouseEvent* event, const std::optional<Engine::Utils::Ray>& ray) override;
    void OnMouseRelease(QMouseEvent* event) override;
    void OnMouseMove(QMouseEvent* event, const std::optional<Engine::Utils::Ray>& ray) override;
    void OnFocusOut() override;

    void AppendOverlay(std::vector<Engine::Rendering::Common::OverlayPrimitive>& overlay) override;

    void SetGizmoAlwaysOnTop(bool value);
    void SetGizmoIgnoreDiffuseSpecular(bool value);
    void SetGizmoAlignByMagnitude(bool value);
    void SetSnapIncrement(double value);
    void SetGizmoSizeCollisions(bool value);

private:
    void InvalidateWorkspaceRaycastCache();
    [[nodiscard]] const Engine::Utils::PartBVH* GetWorkspaceRaycastBVH();
    [[nodiscard]] const Engine::Utils::PartBVH* GetWorkspaceSelectionBVH();

    void RebuildHoveredBoundsCache();
    void DisconnectSelectionCacheSignals();
    void RebuildSelectionCache();
    void RefreshSelectionCacheBoundsIfDirty();
    void InvalidateSelectionBoxCache();
    void RescanSelectionBoxCache();

    void PickSelection(const Engine::Utils::Ray& ray, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void UpdateViewportBoxSelection(const QRect& rect);
    bool CanDragGizmo() const;
    void EnsureGizmoSystem();
    void UpdateGizmo(const std::optional<Engine::Utils::Ray>& ray);
    bool CanDragPart() const;
    bool TryBeginPartDrag(const Engine::Utils::Ray& ray);
    void UpdatePartDrag(const Engine::Utils::Ray& ray);
    void EndPartDrag();
    Engine::Math::Vector3 SnapPosition(const Engine::Math::Vector3& value) const;
    std::vector<std::shared_ptr<Engine::Objects::BasePart>> CollectWorkspaceParts(
        const std::shared_ptr<Engine::Objects::BasePart>& ignore = nullptr
    ) const;
    void AppendGizmoSelectionBox(
        std::vector<Engine::Rendering::Common::OverlayPrimitive>& overlay
    ) const;
    void AppendSelectionBoxInstances(
        std::vector<Engine::Rendering::Common::OverlayPrimitive>& overlay
    ) const;
    void BeginGizmoHistory(const std::shared_ptr<Engine::Objects::BasePart>& targetOverride = nullptr);
    void CommitGizmoHistory();

    Engine::EngineContextPtr context_;
    Engine::Core::Viewport* viewport_{nullptr};
    std::weak_ptr<Engine::DataModel::Place> place_;
    std::shared_ptr<Engine::DataModel::Workspace> workspace_;
    std::shared_ptr<Engine::DataModel::Selection> selection_;
    std::shared_ptr<Engine::DataModel::ChangeHistoryService> historyService_;
    std::unique_ptr<Engine::Core::GizmoSystem> gizmoSystem_;
    bool leftMouseDown_{false};
    bool gizmoDragging_{false};
    bool partDragging_{false};
    struct PartDragState {
        std::shared_ptr<Engine::Objects::BasePart> Instance;
        struct TargetSnapshot {
            std::shared_ptr<Engine::Objects::BasePart> Part;
            Engine::Math::CFrame StartWorldCFrame;
        };
        std::vector<TargetSnapshot> Targets;
        Engine::Math::Vector3 StartPosition{};
        Engine::Math::Vector3 HalfExtents{};
        Engine::Math::Vector3 GrabOffset{};
        Engine::Math::Vector3 StartRayOrigin{};
        Engine::Math::Vector3 StartRayDirection{};
        bool HasMoved{false};
        double FallbackDepth{8.0};
    };
    std::optional<PartDragState> partDragState_;
    std::shared_ptr<Engine::Objects::BasePart> hoveredPart_;
    std::weak_ptr<Engine::Core::Instance> hoveredBoundsKey_;
    std::optional<Engine::Math::AABB> hoveredBoundsCache_;

    struct CachedSelectionEntry {
        std::shared_ptr<Engine::Core::Instance> Instance;
        bool IsBasePart{false};
        std::vector<std::shared_ptr<Engine::Objects::BasePart>> Parts;
        std::optional<Engine::Math::AABB> Bounds;
    };
    std::vector<CachedSelectionEntry> cachedTopLevelSelection_;
    std::unordered_set<const Engine::Objects::BasePart*> cachedSelectedParts_;
    bool cachedSelectionBoundsDirty_{true};
    Engine::Utils::Signal<const std::vector<std::shared_ptr<Engine::Core::Instance>>&>::Connection selectionChangedConnection_;
    std::vector<Engine::Core::Instance::PropertyChangedConnection> cachedSelectionPartPropertyChanged_;
    std::vector<Engine::Core::Instance::InstanceConnection> cachedSelectionPartAncestryChanged_;

    bool selectionBoxCacheDirty_{true};
    double selectionBoxRescanSeconds_{0.0};
    std::vector<std::weak_ptr<Engine::Objects::SelectionBox>> selectionBoxCache_;
    struct WorkspaceRaycastCache {
        Engine::Utils::PartBVH Bvh;
        Engine::Utils::PartBVH SelectionBvh;
        Engine::Core::Instance::InstanceConnection WorkspaceChildAdded;
        Engine::Core::Instance::InstanceConnection WorkspaceChildRemoved;
        Engine::Core::Instance::InstanceConnection WorkspaceAncestryChanged;
        std::vector<Engine::Core::Instance::PropertyChangedConnection> PartPropertyChanged;
        std::vector<Engine::Core::Instance::InstanceConnection> PartAncestryChanged;
    };
    std::optional<WorkspaceRaycastCache> workspaceRaycastCache_;
    bool workspaceRaycastCacheDirty_{false};
    bool workspaceSelectionCacheDirty_{false};

    bool boxSelectPending_{false};
    bool boxSelecting_{false};
    QPoint boxSelectStart_{};
    Qt::KeyboardModifiers boxSelectModifiers_{Qt::NoModifier};
    std::unique_ptr<QRubberBand> boxSelectRubberBand_;
    std::vector<std::shared_ptr<Engine::Core::Instance>> boxSelectInitialSelection_{};
    QRect boxSelectLastRect_{};
    bool boxSelectHasLastRect_{false};

    bool gizmoSizeCollisions_{true};
    struct GizmoHistorySnapshot {
        struct TransformSnapshot {
            std::shared_ptr<Engine::Objects::BasePart> Instance;
            Engine::Math::CFrame CFrame;
            Engine::Math::Vector3 Size;
        };
        std::vector<TransformSnapshot> Instances;
    };
    std::optional<GizmoHistorySnapshot> gizmoHistorySnapshot_;
    bool gizmoAlwaysOnTop_{true};
    bool gizmoIgnoreDiffuseSpecular_{true};
    bool gizmoAlignByMagnitude_{true};
    double snapIncrement_{1.0};
};

} // namespace Lvs::Studio::Core
