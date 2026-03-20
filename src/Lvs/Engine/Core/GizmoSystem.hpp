#pragma once

#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace Lvs::Engine::DataModel {
class Selection;
}

namespace Lvs::Engine::Objects {
class BasePart;
class Camera;
}

namespace Lvs::Engine::Core {

class GizmoSystem final {
public:
    using RenderPrimitive = Rendering::Common::OverlayPrimitive;

    GizmoSystem() = default;
    ~GizmoSystem() = default;

    void Bind(const std::shared_ptr<Objects::Camera>& camera);
    void Unbind();

    void Update(const std::shared_ptr<DataModel::Selection>& selection, Tool activeTool);
    void UpdateHover(const Utils::Ray& ray);
    bool TryBeginDrag(const Utils::Ray& ray);
    void UpdateDrag(const Utils::Ray& ray);
    void EndDrag();

    void Configure(bool alwaysOnTop, bool ignoreDiffuseSpecular, bool alignByMagnitude, double snapIncrement);
    void SetLocalSpace(bool enabled);
    [[nodiscard]] bool GetLocalSpace() const;
    [[nodiscard]] std::shared_ptr<Objects::BasePart> GetTargetPart() const;
    [[nodiscard]] const std::vector<RenderPrimitive>& GetRenderPrimitives() const;

private:
    struct AxisDef {
        String Name;
        Math::Vector3 Direction;
        Math::Color3 Color;
    };

    struct AxisState {
        AxisDef Axis;
        Math::Matrix4 MoveShaftModel{Math::Matrix4::Identity()};
        Math::Matrix4 MoveTipModel{Math::Matrix4::Identity()};
        Math::Matrix4 SizeTipModel{Math::Matrix4::Identity()};
    };

    void RefreshTransforms();
    void RefreshRenderPrimitives();
    void DisconnectSelectedPartSignals();

    [[nodiscard]] double DistanceScale() const;
    [[nodiscard]] Math::Vector3 Center() const;
    [[nodiscard]] Math::Vector3 AxisCenter(const AxisDef& axis) const;
    [[nodiscard]] Math::Vector3 AxisDirection(const AxisDef& axis) const;
    [[nodiscard]] std::optional<Math::Vector3> ClosestPointOnLineToRay(
        const Utils::Ray& ray,
        const Math::Vector3& linePoint,
        const Math::Vector3& lineDirection
    ) const;
    [[nodiscard]] std::optional<String> FindClosestAxis(const Utils::Ray& ray, bool includeMoveShaft) const;
    [[nodiscard]] Math::Color3 AxisColor(const String& axisName) const;

    std::shared_ptr<Objects::Camera> camera_;
    std::vector<AxisState> axes_;
    HashMap<String, AxisDef> axisByName_;
    std::vector<RenderPrimitive> renderPrimitives_;

    struct DragSnapshot {
        std::shared_ptr<Objects::BasePart> Part;
        Math::CFrame StartWorldCFrame;
        Math::Vector3 StartSize;
    };

    Tool activeTool_{Tool::SelectTool};
    bool visible_{false};
    bool alwaysOnTop_{true};
    bool ignoreDiffuseSpecular_{true};
    bool alignByMagnitude_{true};
    bool localSpace_{false};
    double snapIncrement_{1.0};
    std::shared_ptr<Objects::BasePart> targetPart_;
    std::vector<std::shared_ptr<Objects::BasePart>> selectedParts_;
    std::vector<const Instance*> lastSelectionRaw_;
    const Instance* lastSelectionPrimaryRaw_{nullptr};
    bool selectionDirty_{true};
    std::vector<Core::Instance::PropertyChangedConnection> selectedPartPropertyChanged_;
    std::vector<Core::Instance::InstanceConnection> selectedPartAncestryChanged_;
    String hoveredAxis_;
    String activeAxis_;
    Math::Vector3 activeAxisDirection_{};
    std::optional<Math::Vector3> dragStartPoint_;
    Math::Vector3 dragCenter_{};
    bool hasDragCenter_{false};
    std::vector<DragSnapshot> dragSnapshots_;
    Math::AABB dragStartBounds_{};
    bool hasDragStartBounds_{false};
    std::optional<Math::Vector3> startPosition_;
    std::optional<Math::Vector3> startSize_;
    double handleLength_{1.0};
    double handleRadius_{0.08};
    double tipRadius_{0.2};

    bool hasSelectionBounds_{false};
    Math::AABB selectionBounds_{};
    Math::Vector3 selectionCenter_{};
};

} // namespace Lvs::Engine::Core
