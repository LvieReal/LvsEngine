#include "Lvs/Studio/Core/StudioViewportToolLayer.hpp"

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/EditorToolState.hpp"
#include "Lvs/Engine/Core/CriticalError.hpp"
#include "Lvs/Engine/Core/SelectionBoxPrimitives.hpp"
#include "Lvs/Engine/Core/Viewport.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Selection.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/SelectionBox.hpp"
#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <QMouseEvent>
#include <QVariant>
#include <Qt>

#include <algorithm>
#include <array>
#include <cmath>

namespace Lvs::Studio::Core {

namespace {

double SnapToStep(const double value, const double step) {
    if (step <= 0.0) {
        return value;
    }
    return std::round(value / step) * step;
}

bool RayChanged(const Engine::Utils::Ray& a, const Engine::Math::Vector3& startOrigin, const Engine::Math::Vector3& startDirection) {
    const Engine::Math::Vector3 originDelta = a.Origin - startOrigin;
    const Engine::Math::Vector3 directionDelta = a.Direction - startDirection;
    return originDelta.MagnitudeSquared() > 1e-10 || directionDelta.MagnitudeSquared() > 1e-10;
}

struct HitFace {
    int Axis{0};
    double Sign{1.0};
};

HitFace FindClosestHitFace(const Engine::Math::AABB& aabb, const Engine::Math::Vector3& point) {
    const std::array<double, 6> distances{
        std::abs(point.x - aabb.Min.x),
        std::abs(point.x - aabb.Max.x),
        std::abs(point.y - aabb.Min.y),
        std::abs(point.y - aabb.Max.y),
        std::abs(point.z - aabb.Min.z),
        std::abs(point.z - aabb.Max.z)
    };

    std::size_t best = 0;
    double bestDistance = distances[0];
    for (std::size_t i = 1; i < distances.size(); ++i) {
        if (distances[i] < bestDistance) {
            bestDistance = distances[i];
            best = i;
        }
    }

    switch (best) {
        case 0: return HitFace{.Axis = 0, .Sign = -1.0};
        case 1: return HitFace{.Axis = 0, .Sign = 1.0};
        case 2: return HitFace{.Axis = 1, .Sign = -1.0};
        case 3: return HitFace{.Axis = 1, .Sign = 1.0};
        case 4: return HitFace{.Axis = 2, .Sign = -1.0};
        case 5: return HitFace{.Axis = 2, .Sign = 1.0};
        default: return HitFace{};
    }
}

} // namespace

StudioViewportToolLayer::StudioViewportToolLayer(Engine::Core::Viewport& viewport, const Engine::EngineContextPtr& context)
    : context_(context),
      viewport_(&viewport) {}

void StudioViewportToolLayer::BindToPlace(const std::shared_ptr<Engine::DataModel::Place>& place) {
    place_ = place;
    workspace_ = place != nullptr
        ? std::dynamic_pointer_cast<Engine::DataModel::Workspace>(place->FindService("Workspace"))
        : nullptr;
    selection_ = place != nullptr
        ? std::dynamic_pointer_cast<Engine::DataModel::Selection>(place->FindService("Selection"))
        : nullptr;
    historyService_ = place != nullptr
        ? std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(place->FindService("ChangeHistoryService"))
        : nullptr;
    gizmoHistorySnapshot_.reset();
    hoveredPart_.reset();
    partDragState_.reset();
    gizmoDragging_ = false;
    partDragging_ = false;
    leftMouseDown_ = false;

    EnsureGizmoSystem();
}

void StudioViewportToolLayer::Unbind() {
    place_.reset();
    workspace_.reset();
    selection_.reset();
    historyService_.reset();
    gizmoHistorySnapshot_.reset();
    leftMouseDown_ = false;
    gizmoDragging_ = false;
    partDragging_ = false;
    partDragState_.reset();
    hoveredPart_.reset();
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Unbind();
        gizmoSystem_.reset();
    }
}

void StudioViewportToolLayer::SetGizmoAlwaysOnTop(const bool value) {
    gizmoAlwaysOnTop_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void StudioViewportToolLayer::SetGizmoIgnoreDiffuseSpecular(const bool value) {
    gizmoIgnoreDiffuseSpecular_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void StudioViewportToolLayer::SetGizmoAlignByMagnitude(const bool value) {
    gizmoAlignByMagnitude_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void StudioViewportToolLayer::SetSnapIncrement(const double value) {
    snapIncrement_ = std::max(0.0, value);
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void StudioViewportToolLayer::OnFrame(const double deltaSeconds, const std::optional<Engine::Utils::Ray>& cursorRay) {
    static_cast<void>(deltaSeconds);

    UpdateGizmo(cursorRay);

    if (!leftMouseDown_ && !gizmoDragging_ && !partDragging_ && workspace_ != nullptr) {
        if (cursorRay.has_value()) {
            const auto parts = CollectWorkspaceParts();
            const auto [hitPart, distance] = Engine::Utils::RaycastParts(cursorRay.value(), parts);
            static_cast<void>(distance);
            hoveredPart_ = hitPart;
        } else {
            hoveredPart_.reset();
        }
    }

    if (leftMouseDown_ && (gizmoDragging_ || partDragging_) && cursorRay.has_value() && workspace_ != nullptr) {
        if (gizmoDragging_ && gizmoSystem_ != nullptr) {
            gizmoSystem_->UpdateDrag(cursorRay.value());
        } else if (partDragging_) {
            UpdatePartDrag(cursorRay.value());
        }
    }
}

void StudioViewportToolLayer::OnMousePress(QMouseEvent* event, const std::optional<Engine::Utils::Ray>& ray) {
    if (event->button() != Qt::LeftButton) {
        return;
    }

    leftMouseDown_ = true;
    if (!ray.has_value()) {
        return;
    }

    UpdateGizmo(ray);
    if (CanDragGizmo() && gizmoSystem_ != nullptr && gizmoSystem_->TryBeginDrag(ray.value())) {
        hoveredPart_.reset();
        BeginGizmoHistory(gizmoSystem_->GetTargetPart());
        gizmoDragging_ = true;
        return;
    }

    if (CanDragPart() && TryBeginPartDrag(ray.value())) {
        hoveredPart_.reset();
        BeginGizmoHistory(partDragState_.has_value() ? partDragState_->Instance : nullptr);
        partDragging_ = true;
        return;
    }

    const auto previousSelection = selection_ != nullptr ? selection_->GetPrimary() : nullptr;
    PickSelection(ray.value(), event->modifiers());

    const auto currentSelection = selection_ != nullptr ? selection_->GetPrimary() : nullptr;
    if (currentSelection != nullptr && currentSelection != previousSelection && CanDragPart() && TryBeginPartDrag(ray.value())) {
        hoveredPart_.reset();
        BeginGizmoHistory(partDragState_.has_value() ? partDragState_->Instance : nullptr);
        partDragging_ = true;
    }
}

void StudioViewportToolLayer::OnMouseRelease(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }

    leftMouseDown_ = false;
    if (gizmoDragging_ && gizmoSystem_ != nullptr) {
        CommitGizmoHistory();
        gizmoSystem_->EndDrag();
    }
    gizmoDragging_ = false;
    if (partDragging_) {
        CommitGizmoHistory();
        EndPartDrag();
    }
}

void StudioViewportToolLayer::OnMouseMove(QMouseEvent* event, const std::optional<Engine::Utils::Ray>& ray) {
    static_cast<void>(event);
    if (!ray.has_value()) {
        return;
    }

    UpdateGizmo(ray);
    if (gizmoDragging_ && gizmoSystem_ != nullptr) {
        gizmoSystem_->UpdateDrag(ray.value());
    } else if (partDragging_) {
        UpdatePartDrag(ray.value());
    }
}

void StudioViewportToolLayer::OnFocusOut() {
    hoveredPart_.reset();
}

void StudioViewportToolLayer::AppendOverlay(std::vector<Engine::Rendering::Common::OverlayPrimitive>& overlay) {
    if (gizmoSystem_ != nullptr) {
        const auto gizmoPrimitives = gizmoSystem_->GetRenderPrimitives();
        overlay.insert(overlay.end(), gizmoPrimitives.begin(), gizmoPrimitives.end());
    }
    AppendGizmoSelectionBox(overlay);
    AppendSelectionBoxInstances(overlay);
}

void StudioViewportToolLayer::PickSelection(const Engine::Utils::Ray& ray, const Qt::KeyboardModifiers modifiers) {
    if (selection_ == nullptr) {
        return;
    }

    const auto parts = CollectWorkspaceParts();

    const auto [hitPart, distance] = Engine::Utils::RaycastParts(ray, parts);
    static_cast<void>(distance);

    if (hitPart == nullptr) {
        if (!(modifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
            selection_->Clear();
        }
        return;
    }

    if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
        auto current = selection_->Get();

        const auto it = std::find(current.begin(), current.end(), hitPart);
        if ((modifiers & Qt::ControlModifier) && it != current.end()) {
            current.erase(it);
            selection_->Set(current);
            return;
        }

        current.erase(std::remove(current.begin(), current.end(), hitPart), current.end());
        current.insert(current.begin(), hitPart);
        selection_->Set(current);
        return;
    }

    selection_->Set(hitPart);
}

bool StudioViewportToolLayer::CanDragGizmo() const {
    if (context_ == nullptr || context_->EditorToolState == nullptr) {
        return false;
    }
    const Engine::Core::Tool activeTool = context_->EditorToolState->GetActiveTool();
    return activeTool == Engine::Core::Tool::MoveTool || activeTool == Engine::Core::Tool::SizeTool;
}

void StudioViewportToolLayer::EnsureGizmoSystem() {
    if (gizmoSystem_ != nullptr || workspace_ == nullptr) {
        return;
    }
    const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Engine::Objects::Camera>>();
    if (camera == nullptr) {
        return;
    }

    gizmoSystem_ = std::make_unique<Engine::Core::GizmoSystem>();
    gizmoSystem_->Bind(camera);
    gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
}

void StudioViewportToolLayer::UpdateGizmo(const std::optional<Engine::Utils::Ray>& ray) {
    EnsureGizmoSystem();
    if (gizmoSystem_ == nullptr || context_ == nullptr || context_->EditorToolState == nullptr || selection_ == nullptr) {
        return;
    }

    gizmoSystem_->SetLocalSpace(context_->EditorToolState->GetLocalSpace());
    gizmoSystem_->Update(selection_, context_->EditorToolState->GetActiveTool());
    if (ray.has_value() && !gizmoDragging_ && !partDragging_) {
        gizmoSystem_->UpdateHover(ray.value());
    }
}

bool StudioViewportToolLayer::CanDragPart() const {
    if (context_ == nullptr || context_->EditorToolState == nullptr || selection_ == nullptr) {
        return false;
    }
    const auto selected = std::dynamic_pointer_cast<Engine::Objects::BasePart>(selection_->GetPrimary());
    return selected != nullptr && selected->GetParent() != nullptr;
}

bool StudioViewportToolLayer::TryBeginPartDrag(const Engine::Utils::Ray& ray) {
    if (!CanDragPart()) {
        return false;
    }

    const auto selected = std::dynamic_pointer_cast<Engine::Objects::BasePart>(selection_->GetPrimary());
    if (selected == nullptr) {
        return false;
    }
    const auto hitDistance = Engine::Utils::RaycastPartAABB(ray, selected);
    if (!hitDistance.has_value()) {
        return false;
    }

    const auto parts = CollectWorkspaceParts();
    const auto [frontMostPart, frontMostDistance] = Engine::Utils::RaycastParts(ray, parts);
    static_cast<void>(frontMostDistance);
    if (frontMostPart != selected) {
        return false;
    }

    const auto aabb = Engine::Utils::BuildPartWorldAABB(selected);
    const Engine::Math::Vector3 halfExtents = (aabb.Max - aabb.Min) * 0.5;
    const Engine::Math::Vector3 startPosition = selected->GetWorldPosition();
    const Engine::Math::Vector3 hitPoint = ray.Origin + ray.Direction * hitDistance.value();
    const Engine::Math::Vector3 grabOffset = hitPoint - startPosition;
    const Engine::Math::Vector3 size = selected->GetProperty("Size").value<Engine::Math::Vector3>();
    const double maxAxis = std::max({std::abs(size.x), std::abs(size.y), std::abs(size.z)});
    const double fallbackDepth = std::max(0.1, maxAxis * 2.0);

    PartDragState state{
        .Instance = selected,
        .Targets = {},
        .StartPosition = startPosition,
        .HalfExtents = halfExtents,
        .GrabOffset = grabOffset,
        .StartRayOrigin = ray.Origin,
        .StartRayDirection = ray.Direction,
        .HasMoved = false,
        .FallbackDepth = fallbackDepth
    };

    if (selection_ != nullptr) {
        for (const auto& inst : selection_->Get()) {
            const auto part = std::dynamic_pointer_cast<Engine::Objects::BasePart>(inst);
            if (part == nullptr || part->GetParent() == nullptr) {
                continue;
            }
            state.Targets.push_back(PartDragState::TargetSnapshot{.Part = part, .StartWorldCFrame = part->GetWorldCFrame()});
        }
    }
    if (state.Targets.empty()) {
        state.Targets.push_back(PartDragState::TargetSnapshot{.Part = selected, .StartWorldCFrame = selected->GetWorldCFrame()});
    }

    partDragState_ = state;
    return true;
}

void StudioViewportToolLayer::UpdatePartDrag(const Engine::Utils::Ray& ray) {
    if (!partDragState_.has_value()) {
        return;
    }

    auto& state = partDragState_.value();
    if (state.Instance == nullptr) {
        EndPartDrag();
        return;
    }

    if (!state.HasMoved) {
        if (!RayChanged(ray, state.StartRayOrigin, state.StartRayDirection)) {
            return;
        }
        state.HasMoved = true;
    }

    const Engine::Math::Vector3 fallbackHitPoint = ray.Origin + ray.Direction * std::max(0.0, state.FallbackDepth);
    Engine::Math::Vector3 newPosition = fallbackHitPoint - state.GrabOffset;
    bool hasHitPlacement = false;

    const auto parts = CollectWorkspaceParts(state.Instance);
    if (!parts.empty()) {
        std::vector<std::shared_ptr<Engine::Objects::BasePart>> descendantsToExclude;
        descendantsToExclude.push_back(state.Instance);
        const auto descendants = state.Instance->GetDescendants();
        for (const auto& desc : descendants) {
            if (const auto descPart = std::dynamic_pointer_cast<Engine::Objects::BasePart>(desc);
                descPart != nullptr) {
                descendantsToExclude.push_back(descPart);
            }
        }

        const auto [hitPart, hitDistance] = Engine::Utils::RaycastPartsWithFilter(
            ray,
            parts,
            descendantsToExclude,
            Engine::Utils::DescendantFilterType::Exclude
        );

        if (hitPart != nullptr && std::isfinite(hitDistance)) {
            const auto hitAabb = Engine::Utils::BuildPartWorldAABB(hitPart);
            const Engine::Math::Vector3 hitPoint = ray.Origin + ray.Direction * hitDistance;
            const HitFace face = FindClosestHitFace(hitAabb, hitPoint);
            newPosition = hitPoint - state.GrabOffset;
            hasHitPlacement = true;

            if (face.Axis == 0) {
                newPosition.x = face.Sign > 0.0
                    ? hitAabb.Max.x + state.HalfExtents.x
                    : hitAabb.Min.x - state.HalfExtents.x;
                if (snapIncrement_ > 0.0) {
                    const double yAnchor = hitAabb.Min.y + state.HalfExtents.y;
                    const double zAnchor = hitAabb.Min.z + state.HalfExtents.z;
                    newPosition.y = yAnchor + SnapToStep(newPosition.y - yAnchor, snapIncrement_);
                    newPosition.z = zAnchor + SnapToStep(newPosition.z - zAnchor, snapIncrement_);
                }
            } else if (face.Axis == 1) {
                newPosition.y = face.Sign > 0.0
                    ? hitAabb.Max.y + state.HalfExtents.y
                    : hitAabb.Min.y - state.HalfExtents.y;
                if (snapIncrement_ > 0.0) {
                    const double xAnchor = hitAabb.Min.x + state.HalfExtents.x;
                    const double zAnchor = hitAabb.Min.z + state.HalfExtents.z;
                    newPosition.x = xAnchor + SnapToStep(newPosition.x - xAnchor, snapIncrement_);
                    newPosition.z = zAnchor + SnapToStep(newPosition.z - zAnchor, snapIncrement_);
                }
            } else {
                newPosition.z = face.Sign > 0.0
                    ? hitAabb.Max.z + state.HalfExtents.z
                    : hitAabb.Min.z - state.HalfExtents.z;
                if (snapIncrement_ > 0.0) {
                    const double xAnchor = hitAabb.Min.x + state.HalfExtents.x;
                    const double yAnchor = hitAabb.Min.y + state.HalfExtents.y;
                    newPosition.x = xAnchor + SnapToStep(newPosition.x - xAnchor, snapIncrement_);
                    newPosition.y = yAnchor + SnapToStep(newPosition.y - yAnchor, snapIncrement_);
                }
            }
        }
    }

    if (!hasHitPlacement) {
        const Engine::Math::Vector3 delta = newPosition - state.StartPosition;
        newPosition = state.StartPosition + SnapPosition(delta);
    }

    const Engine::Math::Vector3 deltaWorld = newPosition - state.StartPosition;
    for (const auto& target : state.Targets) {
        if (target.Part == nullptr || target.Part->GetParent() == nullptr) {
            continue;
        }
        auto nextWorld = target.StartWorldCFrame;
        nextWorld.Position = nextWorld.Position + deltaWorld;

        const auto parent = target.Part->GetParent();
        if (const auto parentPart = std::dynamic_pointer_cast<Engine::Objects::BasePart>(parent); parentPart != nullptr) {
            const auto local = parentPart->GetWorldCFrame().Inverse() * nextWorld;
            target.Part->SetProperty("CFrame", QVariant::fromValue(local));
        } else {
            target.Part->SetProperty("CFrame", QVariant::fromValue(nextWorld));
        }
    }
}

void StudioViewportToolLayer::EndPartDrag() {
    partDragging_ = false;
    partDragState_.reset();
}

Engine::Math::Vector3 StudioViewportToolLayer::SnapPosition(const Engine::Math::Vector3& value) const {
    return {
        SnapToStep(value.x, snapIncrement_),
        SnapToStep(value.y, snapIncrement_),
        SnapToStep(value.z, snapIncrement_)
    };
}

std::vector<std::shared_ptr<Engine::Objects::BasePart>> StudioViewportToolLayer::CollectWorkspaceParts(
    const std::shared_ptr<Engine::Objects::BasePart>& ignore
) const {
    std::vector<std::shared_ptr<Engine::Objects::BasePart>> parts;
    if (workspace_ == nullptr) {
        return parts;
    }
    for (const auto& descendant : workspace_->GetDescendants()) {
        const auto part = std::dynamic_pointer_cast<Engine::Objects::BasePart>(descendant);
        if (part == nullptr || part->GetParent() == nullptr || part == ignore) {
            continue;
        }
        parts.push_back(part);
    }
    return parts;
}

void StudioViewportToolLayer::AppendGizmoSelectionBox(
    std::vector<Engine::Rendering::Common::OverlayPrimitive>& overlay
) const {
    if (workspace_ == nullptr) {
        return;
    }

    const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Engine::Objects::Camera>>();
    Engine::Math::Vector3 cameraPos{0.0, 0.0, 0.0};
    if (camera != nullptr) {
        cameraPos = camera->GetProperty("CFrame").value<Engine::Math::CFrame>().Position;
    }

    const bool localSpace = context_ != nullptr && context_->EditorToolState != nullptr && context_->EditorToolState->GetLocalSpace();
    const Engine::Math::Color3 selectionColor{0.0, 0.5, 1.0};

    if (selection_ != nullptr) {
        const auto selectedInstances = selection_->Get();
        for (const auto& instance : selectedInstances) {
            const auto selected = std::dynamic_pointer_cast<Engine::Objects::BasePart>(instance);
            if (selected == nullptr || selected->GetParent() == nullptr) {
                continue;
            }

            const auto aabb = Engine::Utils::BuildPartWorldAABB(selected);
            const Engine::Math::Vector3 center = (aabb.Min + aabb.Max) * 0.5;
            const double distance = (center - cameraPos).Magnitude();

            const bool ignoreLighting = gizmoIgnoreDiffuseSpecular_;
            const Engine::Core::SelectionBoxStyle style{
                .Color = selectionColor,
                .Alpha = 1.0F,
                .Metalness = 0.0F,
                .Roughness = 1.0F,
                .Emissive = ignoreLighting ? 1.0F : 0.0F,
                .IgnoreLighting = ignoreLighting,
                .AlwaysOnTop = true,
                .Thickness = 0.001,
                .ScaleWithDistance = true
            };

            if (localSpace) {
                Engine::Core::AppendSelectionBoxOutlinePrimitivesRotated(
                    selected->GetWorldCFrame(),
                    selected->GetProperty("Size").value<Engine::Math::Vector3>(),
                    style,
                    overlay,
                    distance
                );
            } else {
                Engine::Core::AppendSelectionBoxOutlinePrimitives(aabb, style, overlay, distance);
            }
        }
    }

    if (hoveredPart_ != nullptr && hoveredPart_->GetParent() != nullptr) {
        bool isHoveredSelected = false;
        if (selection_ != nullptr) {
            for (const auto& instance : selection_->Get()) {
                if (hoveredPart_ == std::dynamic_pointer_cast<Engine::Objects::BasePart>(instance)) {
                    isHoveredSelected = true;
                    break;
                }
            }
        }

        if (!isHoveredSelected) {
            const auto aabb = Engine::Utils::BuildPartWorldAABB(hoveredPart_);
            const Engine::Math::Vector3 center = (aabb.Min + aabb.Max) * 0.5;
            const double distance = (center - cameraPos).Magnitude();

            const bool ignoreLighting = gizmoIgnoreDiffuseSpecular_;
            const Engine::Core::SelectionBoxStyle style{
                .Color = selectionColor,
                .Alpha = 1.0F,
                .Metalness = 0.0F,
                .Roughness = 1.0F,
                .Emissive = ignoreLighting ? 1.0F : 0.0F,
                .IgnoreLighting = ignoreLighting,
                .AlwaysOnTop = true,
                .Thickness = 0.001,
                .ScaleWithDistance = true
            };

            if (localSpace) {
                Engine::Core::AppendSelectionBoxOutlinePrimitivesRotated(
                    hoveredPart_->GetWorldCFrame(),
                    hoveredPart_->GetProperty("Size").value<Engine::Math::Vector3>(),
                    style,
                    overlay,
                    distance
                );
            } else {
                Engine::Core::AppendSelectionBoxOutlinePrimitives(aabb, style, overlay, distance);
            }
        }
    }
}

void StudioViewportToolLayer::AppendSelectionBoxInstances(
    std::vector<Engine::Rendering::Common::OverlayPrimitive>& overlay
) const {
    if (workspace_ == nullptr) {
        return;
    }

    const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Engine::Objects::Camera>>();
    Engine::Math::Vector3 cameraPos{0.0, 0.0, 0.0};
    if (camera != nullptr) {
        cameraPos = camera->GetProperty("CFrame").value<Engine::Math::CFrame>().Position;
    }

    const bool localSpace = context_ != nullptr && context_->EditorToolState != nullptr && context_->EditorToolState->GetLocalSpace();

    for (const auto& descendant : workspace_->GetDescendants()) {
        const auto box = std::dynamic_pointer_cast<Engine::Objects::SelectionBox>(descendant);
        if (box == nullptr || !box->GetProperty("Visible").toBool()) {
            continue;
        }

        std::shared_ptr<Engine::Core::Instance> adorneeInstance = box->GetProperty("Adornee").value<std::shared_ptr<Engine::Core::Instance>>();
        auto adorneePart = std::dynamic_pointer_cast<Engine::Objects::BasePart>(adorneeInstance);
        if (adorneePart == nullptr) {
            adorneePart = std::dynamic_pointer_cast<Engine::Objects::BasePart>(box->GetParent());
        }
        if (adorneePart == nullptr || adorneePart->GetParent() == nullptr) {
            continue;
        }

        const auto aabb = Engine::Utils::BuildPartWorldAABB(adorneePart);
        const Engine::Math::Vector3 center = (aabb.Min + aabb.Max) * 0.5;
        const double distance = (center - cameraPos).Magnitude();

        const float alpha = std::clamp(
            static_cast<float>(1.0 - box->GetProperty("Transparency").toDouble()),
            0.0F,
            1.0F
        );
        const bool ignoreLighting = box->GetProperty("IgnoreLighting").toBool();
        const bool scaleWithDistance = box->GetProperty("ScaleWithDistance").toBool();

        const Engine::Core::SelectionBoxStyle style{
            .Color = box->GetProperty("Color").value<Engine::Math::Color3>(),
            .Alpha = alpha,
            .Metalness = static_cast<float>(box->GetProperty("Metalness").toDouble()),
            .Roughness = static_cast<float>(box->GetProperty("Roughness").toDouble()),
            .Emissive = static_cast<float>(box->GetProperty("Emissive").toDouble()),
            .IgnoreLighting = ignoreLighting,
            .AlwaysOnTop = box->GetProperty("AlwaysOnTop").toBool(),
            .Thickness = std::max(0.001, box->GetProperty("LineThickness").toDouble()),
            .ScaleWithDistance = scaleWithDistance
        };

        if (localSpace) {
            Engine::Core::AppendSelectionBoxOutlinePrimitivesRotated(
                adorneePart->GetWorldCFrame(),
                adorneePart->GetProperty("Size").value<Engine::Math::Vector3>(),
                style,
                overlay,
                distance
            );
        } else {
            Engine::Core::AppendSelectionBoxOutlinePrimitives(aabb, style, overlay, distance);
        }
    }
}

void StudioViewportToolLayer::BeginGizmoHistory(const std::shared_ptr<Engine::Objects::BasePart>& targetOverride) {
    GizmoHistorySnapshot snapshot;

    if (selection_ != nullptr) {
        for (const auto& inst : selection_->Get()) {
            const auto part = std::dynamic_pointer_cast<Engine::Objects::BasePart>(inst);
            if (part == nullptr || part->GetParent() == nullptr) {
                continue;
            }
            snapshot.Instances.push_back(GizmoHistorySnapshot::TransformSnapshot{
                .Instance = part,
                .CFrame = part->GetProperty("CFrame").value<Engine::Math::CFrame>(),
                .Size = part->GetProperty("Size").value<Engine::Math::Vector3>()
            });
        }
    }

    if (snapshot.Instances.empty()) {
        std::shared_ptr<Engine::Objects::BasePart> target = targetOverride;
        if (target == nullptr && gizmoSystem_ != nullptr) {
            target = gizmoSystem_->GetTargetPart();
        }
        if (target != nullptr && target->GetParent() != nullptr) {
            snapshot.Instances.push_back(GizmoHistorySnapshot::TransformSnapshot{
                .Instance = target,
                .CFrame = target->GetProperty("CFrame").value<Engine::Math::CFrame>(),
                .Size = target->GetProperty("Size").value<Engine::Math::Vector3>()
            });
        }
    }

    if (snapshot.Instances.empty()) {
        gizmoHistorySnapshot_.reset();
        return;
    }
    gizmoHistorySnapshot_ = snapshot;
}

void StudioViewportToolLayer::CommitGizmoHistory() {
    if (!gizmoHistorySnapshot_.has_value() || historyService_ == nullptr) {
        gizmoHistorySnapshot_.reset();
        return;
    }

    const auto snapshot = gizmoHistorySnapshot_.value();
    gizmoHistorySnapshot_.reset();

    bool anyChanged = false;
    struct Change {
        std::shared_ptr<Engine::Objects::BasePart> Instance;
        Engine::Math::CFrame OldCFrame;
        Engine::Math::CFrame NewCFrame;
        Engine::Math::Vector3 OldSize;
        Engine::Math::Vector3 NewSize;
    };
    std::vector<Change> changes;
    changes.reserve(snapshot.Instances.size());

    for (const auto& entry : snapshot.Instances) {
        const auto instance = entry.Instance;
        if (instance == nullptr || instance->GetParent() == nullptr) {
            continue;
        }

        const auto newCFrame = instance->GetProperty("CFrame").value<Engine::Math::CFrame>();
        const auto newSize = instance->GetProperty("Size").value<Engine::Math::Vector3>();
        if (entry.CFrame == newCFrame && entry.Size == newSize) {
            continue;
        }
        anyChanged = true;
        changes.push_back(Change{
            .Instance = instance,
            .OldCFrame = entry.CFrame,
            .NewCFrame = newCFrame,
            .OldSize = entry.Size,
            .NewSize = newSize
        });
    }
    if (!anyChanged) {
        return;
    }

    if (historyService_->IsRecording()) {
        return;
    }

    historyService_->BeginRecording("Gizmo Transform");
    try {
        for (const auto& change : changes) {
            if (change.Instance == nullptr) {
                continue;
            }
            if (!(change.OldSize == change.NewSize)) {
                historyService_->Record(
                    std::make_shared<Engine::Utils::SetPropertyCommand>(
                        change.Instance,
                        "Size",
                        QVariant::fromValue(change.OldSize),
                        QVariant::fromValue(change.NewSize)
                    )
                );
            }
            if (!(change.OldCFrame == change.NewCFrame)) {
                historyService_->Record(
                    std::make_shared<Engine::Utils::SetPropertyCommand>(
                        change.Instance,
                        "CFrame",
                        QVariant::fromValue(change.OldCFrame),
                        QVariant::fromValue(change.NewCFrame)
                    )
                );
            }
        }
    } catch (const std::exception& ex) {
        Engine::Core::CriticalError::ShowUnexpectedNoReturnError(QString::fromUtf8(ex.what()));
    }
    historyService_->FinishRecording();
}

} // namespace Lvs::Studio::Core
