#pragma once

#include "Lvs/Engine/Core/ViewportToolLayer.hpp"

#include "Lvs/Engine/Core/GizmoSystem.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"

#include <memory>
#include <optional>
#include <vector>

class QMouseEvent;

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
}

namespace Lvs::Studio::Core {

class StudioViewportToolLayer final : public Engine::Core::ViewportToolLayer {
public:
    StudioViewportToolLayer(Engine::Core::Viewport& viewport, const Engine::EngineContextPtr& context);

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

private:
    void PickSelection(const Engine::Utils::Ray& ray);
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
    struct GizmoHistorySnapshot {
        std::shared_ptr<Engine::Objects::BasePart> Instance;
        Engine::Math::Vector3 Position;
        Engine::Math::Vector3 Size;
    };
    std::optional<GizmoHistorySnapshot> gizmoHistorySnapshot_;
    bool gizmoAlwaysOnTop_{true};
    bool gizmoIgnoreDiffuseSpecular_{true};
    bool gizmoAlignByMagnitude_{true};
    double snapIncrement_{1.0};
};

} // namespace Lvs::Studio::Core
