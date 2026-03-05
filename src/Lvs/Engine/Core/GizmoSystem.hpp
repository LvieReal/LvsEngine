#pragma once

#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <QHash>
#include <QString>

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
    struct RenderPrimitive {
        Math::Matrix4 Model{Math::Matrix4::Identity()};
        Enums::PartShape Shape{Enums::PartShape::Cube};
        Math::Color3 Color{};
        float Alpha{1.0F};
        float Metalness{0.0F};
        float Roughness{1.0F};
        float Emissive{1.0F};
        bool IgnoreLighting{false};
        bool AlwaysOnTop{true};
    };

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
    [[nodiscard]] std::shared_ptr<Objects::BasePart> GetTargetPart() const;
    [[nodiscard]] const std::vector<RenderPrimitive>& GetRenderPrimitives() const;

private:
    struct AxisDef {
        QString Name;
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

    [[nodiscard]] double DistanceScale() const;
    [[nodiscard]] Math::Vector3 Center() const;
    [[nodiscard]] Math::Vector3 AxisCenter(const AxisDef& axis) const;
    [[nodiscard]] std::optional<Math::Vector3> ClosestPointOnLineToRay(
        const Utils::Ray& ray,
        const Math::Vector3& linePoint,
        const Math::Vector3& lineDirection
    ) const;
    [[nodiscard]] std::optional<QString> FindClosestAxis(const Utils::Ray& ray, bool includeMoveShaft) const;
    [[nodiscard]] Math::Color3 AxisColor(const QString& axisName) const;

    std::shared_ptr<Objects::Camera> camera_;
    std::vector<AxisState> axes_;
    QHash<QString, AxisDef> axisByName_;
    std::vector<RenderPrimitive> renderPrimitives_;

    Tool activeTool_{Tool::SelectTool};
    bool visible_{false};
    bool alwaysOnTop_{true};
    bool ignoreDiffuseSpecular_{true};
    bool alignByMagnitude_{true};
    double snapIncrement_{1.0};
    std::shared_ptr<Objects::BasePart> targetPart_;
    QString hoveredAxis_;
    QString activeAxis_;
    Math::Vector3 activeAxisDirection_{};
    std::optional<Math::Vector3> dragStartPoint_;
    std::optional<Math::Vector3> startPosition_;
    std::optional<Math::Vector3> startSize_;
    double handleLength_{1.0};
    double handleRadius_{0.08};
    double tipRadius_{0.2};
};

} // namespace Lvs::Engine::Core
