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
    selectedParts_.clear();
    hoveredAxis_.clear();
    activeAxis_.clear();
    dragStartPoint_.reset();
    hasDragCenter_ = false;
    dragCenter_ = {};
    dragSnapshots_.clear();
    hasDragStartBounds_ = false;
    startPosition_.reset();
    startSize_.reset();
}

void GizmoSystem::Update(const std::shared_ptr<DataModel::Selection>& selection, const Tool activeTool) {
    activeTool_ = activeTool;

    hasSelectionBounds_ = false;
    selectionBounds_ = {};
    selectionCenter_ = {};
    selectedParts_.clear();

    std::shared_ptr<Objects::BasePart> primaryPart;
    if (selection != nullptr) {
        primaryPart = std::dynamic_pointer_cast<Objects::BasePart>(selection->GetPrimary());
        const auto instances = selection->Get();
        for (const auto& instance : instances) {
            const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance);
            if (part == nullptr || part->GetParent() == nullptr) {
                continue;
            }
            selectedParts_.push_back(part);
            const auto aabb = Utils::BuildPartWorldAABB(part);
            if (!hasSelectionBounds_) {
                selectionBounds_ = aabb;
                hasSelectionBounds_ = true;
            } else {
                selectionBounds_.Min.x = std::min(selectionBounds_.Min.x, aabb.Min.x);
                selectionBounds_.Min.y = std::min(selectionBounds_.Min.y, aabb.Min.y);
                selectionBounds_.Min.z = std::min(selectionBounds_.Min.z, aabb.Min.z);
                selectionBounds_.Max.x = std::max(selectionBounds_.Max.x, aabb.Max.x);
                selectionBounds_.Max.y = std::max(selectionBounds_.Max.y, aabb.Max.y);
                selectionBounds_.Max.z = std::max(selectionBounds_.Max.z, aabb.Max.z);
            }
        }
    }

    if (hasSelectionBounds_) {
        selectionCenter_ = selectionBounds_.Centroid();
    }

    targetPart_ = primaryPart;
    if (targetPart_ == nullptr && selection != nullptr) {
        for (const auto& instance : selection->Get()) {
            const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance);
            if (part != nullptr && part->GetParent() != nullptr) {
                targetPart_ = part;
                break;
            }
        }
    }
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
    activeAxisDirection_ = AxisDirection(axisByName_.value(activeAxis_));
    startPosition_ = targetPart_->GetProperty("Position").value<Math::Vector3>();
    startSize_ = targetPart_->GetProperty("Size").value<Math::Vector3>();

    dragSnapshots_.clear();
    dragSnapshots_.reserve(selectedParts_.size());
    for (const auto& part : selectedParts_) {
        if (part == nullptr || part->GetParent() == nullptr) {
            continue;
        }
        DragSnapshot snap{
            .Part = part,
            .StartWorldCFrame = part->GetWorldCFrame(),
            .StartSize = part->GetProperty("Size").value<Math::Vector3>()
        };
        dragSnapshots_.push_back(snap);
    }

    hasDragStartBounds_ = hasSelectionBounds_;
    dragStartBounds_ = hasSelectionBounds_ ? selectionBounds_ : Math::AABB{};
    hasDragCenter_ = true;
    dragCenter_ = Center();

    dragStartPoint_ = ClosestPointOnLineToRay(ray, dragCenter_, activeAxisDirection_);
    RefreshRenderPrimitives();
    return dragStartPoint_.has_value();
}

void GizmoSystem::UpdateDrag(const Utils::Ray& ray) {
    if (targetPart_ == nullptr || activeAxis_.isEmpty() || !dragStartPoint_.has_value() || !hasDragCenter_) {
        return;
    }

    const auto currentPoint = ClosestPointOnLineToRay(ray, dragCenter_, activeAxisDirection_);
    if (!currentPoint.has_value()) {
        return;
    }

    const Math::Vector3 delta = currentPoint.value() - dragStartPoint_.value();
    double amount = delta.Dot(activeAxisDirection_);
    amount = SnapToStep(amount, snapIncrement_);

    if (activeTool_ == Tool::MoveTool) {
        const Math::Vector3 translation = activeAxisDirection_ * amount;
        for (const auto& snap : dragSnapshots_) {
            if (snap.Part == nullptr || snap.Part->GetParent() == nullptr) {
                continue;
            }
            auto next = snap.StartWorldCFrame;
            next.Position = next.Position + translation;

            const auto parent = snap.Part->GetParent();
            if (const auto parentPart = std::dynamic_pointer_cast<Objects::BasePart>(parent); parentPart != nullptr) {
                const auto local = parentPart->GetWorldCFrame().Inverse() * next;
                snap.Part->SetProperty("CFrame", QVariant::fromValue(local));
            } else {
                snap.Part->SetProperty("CFrame", QVariant::fromValue(next));
            }
        }
        return;
    }

    if (activeTool_ != Tool::SizeTool) {
        return;
    }

    if (dragSnapshots_.size() > 1) {
        if (!hasDragStartBounds_) {
            return;
        }
        const Math::Vector3 halfExtents = (dragStartBounds_.Max - dragStartBounds_.Min) * 0.5;
        const Math::Vector3 dir = activeAxisDirection_.Unit();
        const Math::Vector3 absDir{std::abs(dir.x), std::abs(dir.y), std::abs(dir.z)};
        const double extentAlongAxis = std::max(0.001, 2.0 * (absDir.x * halfExtents.x + absDir.y * halfExtents.y + absDir.z * halfExtents.z));

        const double nextExtent = std::max(0.05, extentAlongAxis + amount * 2.0);
        const double factor = nextExtent / extentAlongAxis;

        for (const auto& snap : dragSnapshots_) {
            if (snap.Part == nullptr || snap.Part->GetParent() == nullptr) {
                continue;
            }

            Math::Vector3 newSize{
                std::max(0.05, snap.StartSize.x * factor),
                std::max(0.05, snap.StartSize.y * factor),
                std::max(0.05, snap.StartSize.z * factor)
            };

            auto next = snap.StartWorldCFrame;
            const Math::Vector3 offset = snap.StartWorldCFrame.Position - dragCenter_;
            next.Position = dragCenter_ + offset * factor;

            const auto parent = snap.Part->GetParent();
            if (const auto parentPart = std::dynamic_pointer_cast<Objects::BasePart>(parent); parentPart != nullptr) {
                const auto local = parentPart->GetWorldCFrame().Inverse() * next;
                snap.Part->SetProperty("CFrame", QVariant::fromValue(local));
            } else {
                snap.Part->SetProperty("CFrame", QVariant::fromValue(next));
            }
            snap.Part->SetProperty("Size", QVariant::fromValue(newSize));
        }
        return;
    }

    if (dragSnapshots_.empty()) {
        return;
    }

    auto snap = dragSnapshots_.front();
    if (snap.Part == nullptr || snap.Part->GetParent() == nullptr) {
        return;
    }

    Math::Vector3 newSize = snap.StartSize;
    const bool axisX = activeAxis_.contains("X");
    const bool axisY = activeAxis_.contains("Y");
    const bool axisZ = activeAxis_.contains("Z");
    if (axisX) {
        newSize.x = std::max(0.05, snap.StartSize.x + amount);
        if (snapIncrement_ > 0.0) {
            newSize.x = std::max(0.05, SnapToStep(newSize.x, snapIncrement_));
        }
    } else if (axisY) {
        newSize.y = std::max(0.05, snap.StartSize.y + amount);
        if (snapIncrement_ > 0.0) {
            newSize.y = std::max(0.05, SnapToStep(newSize.y, snapIncrement_));
        }
    } else if (axisZ) {
        newSize.z = std::max(0.05, snap.StartSize.z + amount);
        if (snapIncrement_ > 0.0) {
            newSize.z = std::max(0.05, SnapToStep(newSize.z, snapIncrement_));
        }
    }

    double appliedAmount = 0.0;
    if (axisX) {
        appliedAmount = newSize.x - snap.StartSize.x;
    } else if (axisY) {
        appliedAmount = newSize.y - snap.StartSize.y;
    } else if (axisZ) {
        appliedAmount = newSize.z - snap.StartSize.z;
    }

    auto next = snap.StartWorldCFrame;
    next.Position = next.Position + activeAxisDirection_.Unit() * (appliedAmount * 0.5);

    const auto parent = snap.Part->GetParent();
    if (const auto parentPart = std::dynamic_pointer_cast<Objects::BasePart>(parent); parentPart != nullptr) {
        const auto local = parentPart->GetWorldCFrame().Inverse() * next;
        snap.Part->SetProperty("CFrame", QVariant::fromValue(local));
    } else {
        snap.Part->SetProperty("CFrame", QVariant::fromValue(next));
    }
    snap.Part->SetProperty("Size", QVariant::fromValue(newSize));
}

void GizmoSystem::EndDrag() {
    activeAxis_.clear();
    dragStartPoint_.reset();
    hasDragCenter_ = false;
    dragCenter_ = {};
    dragSnapshots_.clear();
    hasDragStartBounds_ = false;
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

void GizmoSystem::SetLocalSpace(const bool enabled) {
    if (localSpace_ == enabled) {
        return;
    }
    localSpace_ = enabled;
    RefreshTransforms();
    RefreshRenderPrimitives();
}

bool GizmoSystem::GetLocalSpace() const {
    return localSpace_;
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

        const Math::Vector3 up = AxisDirection(axis.Axis).Unit();
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
    if (camera_ == nullptr) {
        return 1.0;
    }
    const Math::Vector3 targetPos = hasSelectionBounds_ ? selectionCenter_
                                                        : (targetPart_ != nullptr ? targetPart_->GetWorldCFrame().Position : Math::Vector3{});
    const Math::Vector3 cameraPos = camera_->GetProperty("CFrame").value<Math::CFrame>().Position;
    return std::max(0.1, (cameraPos - targetPos).Magnitude() / 10.0);
}

Math::Vector3 GizmoSystem::Center() const {
    if (hasSelectionBounds_) {
        return selectionCenter_;
    }
    if (targetPart_ == nullptr) {
        return {};
    }
    return Utils::BuildPartWorldAABB(targetPart_).Centroid();
}

Math::Vector3 GizmoSystem::AxisCenter(const AxisDef& axis) const {
    if (targetPart_ == nullptr && !hasSelectionBounds_) {
        return {};
    }

    const auto aabb = hasSelectionBounds_ ? selectionBounds_ : Utils::BuildPartWorldAABB(targetPart_);
    const Math::Vector3 pos = hasSelectionBounds_ ? selectionCenter_ : aabb.Centroid();
    const double halfLength = (handleLength_ * 0.5 + tipRadius_ * 0.5) * (1.0 + DistanceScale());

    const Math::Vector3 dir = AxisDirection(axis).Unit();
    const Math::Vector3 size = aabb.Max - aabb.Min;

    double radius = 0.0;
    if (alignByMagnitude_) {
        radius = std::max(0.05, size.Magnitude() * 0.1);
    } else {
        const Math::Vector3 halfExtents = size * 0.5;
        const Math::Vector3 absDir{std::abs(dir.x), std::abs(dir.y), std::abs(dir.z)};
        radius = std::max(0.05, absDir.x * halfExtents.x + absDir.y * halfExtents.y + absDir.z * halfExtents.z);
    }
    return pos + dir * (radius + halfLength);
}

Math::Vector3 GizmoSystem::AxisDirection(const AxisDef& axis) const {
    const Math::Vector3 base = axis.Direction.Unit();
    if (!localSpace_ || targetPart_ == nullptr) {
        return base;
    }

    const Math::CFrame world = targetPart_->GetWorldCFrame().RotationOnly();
    const Math::Vector3 x = world.RightVector().Unit();
    const Math::Vector3 y = world.UpVector().Unit();
    const Math::Vector3 z = world.LookVector().Unit();
    return (x * base.x + y * base.y + z * base.z).Unit();
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
