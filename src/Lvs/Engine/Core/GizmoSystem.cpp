#include "Lvs/Engine/Core/GizmoSystem.hpp"

#include "Lvs/Engine/DataModel/Services/Selection.hpp"
#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"

#include <QVariant>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Lvs::Engine::Core {

namespace {

double SnapToStep(const double value, const double step) {
    if (step <= 0.0) {
        return value;
    }
    return std::round(value / step) * step;
}

} // namespace

void GizmoSystem::Bind(const std::shared_ptr<Objects::Camera>& camera) {
    camera_ = camera;
    axes_.clear();
    axisByName_.clear();

    const std::vector<AxisDef> defs = {
        {"X", {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
        {"-X", {-1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
        {"Y", {0.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
        {"-Y", {0.0, -1.0, 0.0}, {0.0, 1.0, 0.0}},
        {"Z", {0.0, 0.0, 1.0}, {0.0, 0.5, 1.0}},
        {"-Z", {0.0, 0.0, -1.0}, {0.0, 0.5, 1.0}},
    };
    axes_.reserve(defs.size());
    for (const auto& axis : defs) {
        axisByName_.insert(axis.Name, axis);
        axes_.push_back(AxisState{.Axis = axis});
    }
}

void GizmoSystem::Unbind() {
    camera_.reset();
    axes_.clear();
    axisByName_.clear();
    renderPrimitives_.clear();
    targetPart_.reset();
    hoveredAxis_.clear();
    activeAxis_.clear();
    dragStartPoint_.reset();
    startPosition_.reset();
    startSize_.reset();
}

void GizmoSystem::Update(const std::shared_ptr<DataModel::Selection>& selection, const Tool activeTool) {
    activeTool_ = activeTool;

    std::shared_ptr<Objects::BasePart> selectedPart;
    if (selection != nullptr) {
        selectedPart = std::dynamic_pointer_cast<Objects::BasePart>(selection->GetPrimary());
    }

    targetPart_ = selectedPart;
    visible_ = targetPart_ != nullptr && (activeTool_ == Tool::MoveTool || activeTool_ == Tool::SizeTool);

    if (!visible_) {
        hoveredAxis_.clear();
        activeAxis_.clear();
        dragStartPoint_.reset();
        startPosition_.reset();
        startSize_.reset();
    }

    RefreshTransforms();
    RefreshRenderPrimitives();
}

void GizmoSystem::UpdateHover(const Utils::Ray& ray) {
    if (!visible_ || !activeAxis_.isEmpty()) {
        return;
    }
    const auto axis = FindClosestAxis(ray, activeTool_ == Tool::MoveTool);
    hoveredAxis_ = axis.has_value() ? axis.value() : QString{};
    RefreshRenderPrimitives();
}

bool GizmoSystem::TryBeginDrag(const Utils::Ray& ray) {
    if (!visible_ || targetPart_ == nullptr) {
        return false;
    }

    const auto axis = FindClosestAxis(ray, activeTool_ == Tool::MoveTool);
    if (!axis.has_value()) {
        return false;
    }

    activeAxis_ = axis.value();
    activeAxisDirection_ = axisByName_.value(activeAxis_).Direction;
    startPosition_ = targetPart_->GetProperty("Position").value<Math::Vector3>();
    startSize_ = targetPart_->GetProperty("Size").value<Math::Vector3>();
    dragStartPoint_ = ClosestPointOnLineToRay(ray, Center(), activeAxisDirection_);
    RefreshRenderPrimitives();
    return dragStartPoint_.has_value();
}

void GizmoSystem::UpdateDrag(const Utils::Ray& ray) {
    if (targetPart_ == nullptr || activeAxis_.isEmpty() || !dragStartPoint_.has_value() || !startPosition_.has_value()) {
        return;
    }

    const auto currentPoint = ClosestPointOnLineToRay(ray, Center(), activeAxisDirection_);
    if (!currentPoint.has_value()) {
        return;
    }

    const Math::Vector3 delta = currentPoint.value() - dragStartPoint_.value();
    double amount = delta.Dot(activeAxisDirection_);
    amount = SnapToStep(amount, snapIncrement_);

    if (activeTool_ == Tool::MoveTool) {
        targetPart_->SetProperty("Position", QVariant::fromValue(startPosition_.value() + activeAxisDirection_ * amount));
        return;
    }

    if (activeTool_ == Tool::SizeTool && startSize_.has_value()) {
        const auto& startSize = startSize_.value();
        Math::Vector3 newSize{
            std::max(0.05, startSize.x + std::abs(activeAxisDirection_.x) * amount),
            std::max(0.05, startSize.y + std::abs(activeAxisDirection_.y) * amount),
            std::max(0.05, startSize.z + std::abs(activeAxisDirection_.z) * amount)
        };
        if (snapIncrement_ > 0.0) {
            if (std::abs(activeAxisDirection_.x) > 0.0) {
                newSize.x = std::max(0.05, SnapToStep(newSize.x, snapIncrement_));
            } else if (std::abs(activeAxisDirection_.y) > 0.0) {
                newSize.y = std::max(0.05, SnapToStep(newSize.y, snapIncrement_));
            } else if (std::abs(activeAxisDirection_.z) > 0.0) {
                newSize.z = std::max(0.05, SnapToStep(newSize.z, snapIncrement_));
            }
        }

        double appliedAmount = 0.0;
        if (std::abs(activeAxisDirection_.x) > 0.0) {
            appliedAmount = newSize.x - startSize.x;
        } else if (std::abs(activeAxisDirection_.y) > 0.0) {
            appliedAmount = newSize.y - startSize.y;
        } else if (std::abs(activeAxisDirection_.z) > 0.0) {
            appliedAmount = newSize.z - startSize.z;
        }

        targetPart_->SetProperty("Size", QVariant::fromValue(newSize));
        targetPart_->SetProperty(
            "Position",
            QVariant::fromValue(startPosition_.value() + activeAxisDirection_ * (appliedAmount * 0.5))
        );
    }
}

void GizmoSystem::EndDrag() {
    activeAxis_.clear();
    dragStartPoint_.reset();
    startPosition_.reset();
    startSize_.reset();
    RefreshRenderPrimitives();
}

void GizmoSystem::Configure(const bool alwaysOnTop, const bool ignoreDiffuseSpecular, const bool alignByMagnitude, const double snapIncrement) {
    alwaysOnTop_ = alwaysOnTop;
    ignoreDiffuseSpecular_ = ignoreDiffuseSpecular;
    alignByMagnitude_ = alignByMagnitude;
    snapIncrement_ = std::max(0.0, snapIncrement);
    RefreshTransforms();
    RefreshRenderPrimitives();
}

std::shared_ptr<Objects::BasePart> GizmoSystem::GetTargetPart() const {
    return targetPart_;
}

const std::vector<GizmoSystem::RenderPrimitive>& GizmoSystem::GetRenderPrimitives() const {
    return renderPrimitives_;
}

void GizmoSystem::RefreshTransforms() {
    if (!visible_) {
        return;
    }

    for (auto& axis : axes_) {
        const double scale = 1.0 + DistanceScale();
        const Math::Vector3 axisCenter = AxisCenter(axis.Axis);

        const Math::Vector3 up = axis.Axis.Direction.Unit();
        const Math::Vector3 helper = std::abs(up.z) < 0.99 ? Math::Vector3{0.0, 0.0, 1.0} : Math::Vector3{0.0, 1.0, 0.0};
        const Math::Vector3 right = helper.Cross(up).Unit();
        const Math::Vector3 back = right.Cross(up).Unit();
        const Math::CFrame base(axisCenter, right, up, back);

        axis.MoveShaftModel = base.ToMatrix4() * Math::Matrix4::Scale({handleRadius_ * scale, handleLength_ * scale, handleRadius_ * scale});

        const double offset = (handleLength_ * 0.5 + tipRadius_ * 0.5) * scale;
        axis.MoveTipModel = base.ToMatrix4() * Math::Matrix4::Translation({0.0, offset, 0.0}) *
                            Math::Matrix4::Scale({tipRadius_ * scale, tipRadius_ * scale  * 1.5, tipRadius_ * scale});

        axis.SizeTipModel = base.ToMatrix4() * Math::Matrix4::Scale({tipRadius_ * scale, tipRadius_ * scale, tipRadius_ * scale});
    }
}

void GizmoSystem::RefreshRenderPrimitives() {
    renderPrimitives_.clear();
    if (!visible_) {
        return;
    }

    const bool showMove = activeTool_ == Tool::MoveTool;
    const bool showSize = activeTool_ == Tool::SizeTool;

    for (const auto& axis : axes_) {
        const Math::Color3 color = AxisColor(axis.Axis.Name);
        const float emissive = ignoreDiffuseSpecular_ ? 1.0F : 0.0F;
        const bool ignoreLighting = ignoreDiffuseSpecular_;

        if (showMove) {
            renderPrimitives_.push_back(RenderPrimitive{
                .Model = axis.MoveShaftModel,
                .Shape = Enums::PartShape::Cylinder,
                .Color = color,
                .Alpha = 1.0F,
                .Metalness = 0.0F,
                .Roughness = 1.0F,
                .Emissive = emissive,
                .IgnoreLighting = ignoreLighting,
                .AlwaysOnTop = alwaysOnTop_
            });
            renderPrimitives_.push_back(RenderPrimitive{
                .Model = axis.MoveTipModel,
                .Shape = Enums::PartShape::Cone,
                .Color = color,
                .Alpha = 1.0F,
                .Metalness = 0.0F,
                .Roughness = 1.0F,
                .Emissive = emissive,
                .IgnoreLighting = ignoreLighting,
                .AlwaysOnTop = alwaysOnTop_
            });
        }

        if (showSize) {
            renderPrimitives_.push_back(RenderPrimitive{
                .Model = axis.SizeTipModel,
                .Shape = Enums::PartShape::Sphere,
                .Color = color,
                .Alpha = 1.0F,
                .Metalness = 0.0F,
                .Roughness = 1.0F,
                .Emissive = emissive,
                .IgnoreLighting = ignoreLighting,
                .AlwaysOnTop = alwaysOnTop_
            });
        }
    }

    if (camera_ == nullptr) {
        return;
    }
    const Math::Vector3 cameraPos = camera_->GetProperty("CFrame").value<Math::CFrame>().Position;
    std::sort(renderPrimitives_.begin(), renderPrimitives_.end(), [&cameraPos](const RenderPrimitive& a, const RenderPrimitive& b) {
        const auto& ar = a.Model.Rows();
        const auto& br = b.Model.Rows();
        const Math::Vector3 aPos{ar[0][3], ar[1][3], ar[2][3]};
        const Math::Vector3 bPos{br[0][3], br[1][3], br[2][3]};
        const double aDist = (aPos - cameraPos).Magnitude();
        const double bDist = (bPos - cameraPos).Magnitude();
        return aDist > bDist;
    });
}

double GizmoSystem::DistanceScale() const {
    if (targetPart_ == nullptr || camera_ == nullptr) {
        return 1.0;
    }
    const Math::Vector3 targetPos = targetPart_->GetWorldCFrame().Position;
    const Math::Vector3 cameraPos = camera_->GetProperty("CFrame").value<Math::CFrame>().Position;
    return std::max(0.1, (cameraPos - targetPos).Magnitude() / 10.0);
}

Math::Vector3 GizmoSystem::Center() const {
    if (targetPart_ == nullptr) {
        return {};
    }
    return Utils::BuildPartWorldAABB(targetPart_).Centroid();
}

Math::Vector3 GizmoSystem::AxisCenter(const AxisDef& axis) const {
    if (targetPart_ == nullptr) {
        return {};
    }

    const auto aabb = Utils::BuildPartWorldAABB(targetPart_);
    const Math::Vector3 pos = aabb.Centroid();
    const double halfLength = (handleLength_ * 0.5 + tipRadius_ * 0.5) * (1.0 + DistanceScale());

    if (alignByMagnitude_) {
        const Math::Vector3 size = aabb.Max - aabb.Min;
        const double radius = std::max(0.05, size.Magnitude() * 0.1);
        return pos + axis.Direction * (radius + halfLength);
    }

    Math::Vector3 axisPos = pos;
    if (axis.Name == "X") {
        axisPos.x = aabb.Max.x + halfLength;
    } else if (axis.Name == "-X") {
        axisPos.x = aabb.Min.x - halfLength;
    } else if (axis.Name == "Y") {
        axisPos.y = aabb.Max.y + halfLength;
    } else if (axis.Name == "-Y") {
        axisPos.y = aabb.Min.y - halfLength;
    } else if (axis.Name == "Z") {
        axisPos.z = aabb.Max.z + halfLength;
    } else if (axis.Name == "-Z") {
        axisPos.z = aabb.Min.z - halfLength;
    }
    return axisPos;
}

std::optional<Math::Vector3> GizmoSystem::ClosestPointOnLineToRay(
    const Utils::Ray& ray,
    const Math::Vector3& linePoint,
    const Math::Vector3& lineDirection
) const {
    const Math::Vector3 d = ray.Direction;
    const Math::Vector3 a = lineDirection.Unit();
    const Math::Vector3 w = linePoint - ray.Origin;

    const double dd = d.Dot(d);
    const double aa = a.Dot(a);
    const double da = d.Dot(a);
    const double den = dd * aa - da * da;
    if (std::abs(den) < 1e-8) {
        return std::nullopt;
    }

    const double b1 = d.Dot(w);
    const double b2 = a.Dot(w);
    const double s = (b1 * da - b2 * dd) / den;
    return linePoint + a * s;
}

std::optional<QString> GizmoSystem::FindClosestAxis(const Utils::Ray& ray, const bool includeMoveShaft) const {
    static const Math::AABB localUnitAabb{{-0.5, -0.5, -0.5}, {0.5, 0.5, 0.5}};
    QString closestAxis;
    double closestDistance = std::numeric_limits<double>::infinity();

    for (const auto& axis : axes_) {
        const auto tipAabb = Math::TransformAABB(localUnitAabb, activeTool_ == Tool::MoveTool ? axis.MoveTipModel : axis.SizeTipModel);
        if (const auto dist = Math::IntersectRayAABB(ray.Origin, ray.Direction, tipAabb); dist.has_value() && dist.value() < closestDistance) {
            closestDistance = dist.value();
            closestAxis = axis.Axis.Name;
        }

        if (includeMoveShaft) {
            const auto shaftAabb = Math::TransformAABB(localUnitAabb, axis.MoveShaftModel);
            if (const auto dist = Math::IntersectRayAABB(ray.Origin, ray.Direction, shaftAabb); dist.has_value() &&
                                                                                               dist.value() < closestDistance) {
                closestDistance = dist.value();
                closestAxis = axis.Axis.Name;
            }
        }
    }

    if (closestAxis.isEmpty()) {
        return std::nullopt;
    }
    return closestAxis;
}

Math::Color3 GizmoSystem::AxisColor(const QString& axisName) const {
    const Math::Color3 base = axisByName_.value(axisName).Color;
    if (activeAxis_ == axisName) {
        return {1.0, 1.0, 0.1};
    }
    if (hoveredAxis_ == axisName) {
        return {
            std::min(1.0, base.r + 0.25),
            std::min(1.0, base.g + 0.25),
            std::min(1.0, base.b + 0.25)
        };
    }
    return base;
}

} // namespace Lvs::Engine::Core
