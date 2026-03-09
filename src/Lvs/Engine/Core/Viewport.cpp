#include "Lvs/Engine/Core/Viewport.hpp"

#include "Lvs/Engine/Core/CameraController.hpp"
#include "Lvs/Engine/Core/CriticalError.hpp"
#include "Lvs/Engine/Core/Cursor.hpp"
#include "Lvs/Engine/Core/SelectionBoxPrimitives.hpp"
#include "Lvs/Engine/DataModel/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Selection.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/SelectionBox.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Rendering/Common/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <Qt>

#include <cstdint>
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Lvs::Engine::Core {

namespace {

double SnapToStep(const double value, const double step) {
    if (step <= 0.0) {
        return value;
    }
    return std::round(value / step) * step;
}

bool RayChanged(const Utils::Ray& a, const Math::Vector3& startOrigin, const Math::Vector3& startDirection) {
    const Math::Vector3 originDelta = a.Origin - startOrigin;
    const Math::Vector3 directionDelta = a.Direction - startDirection;
    return originDelta.MagnitudeSquared() > 1e-10 || directionDelta.MagnitudeSquared() > 1e-10;
}

struct HitFace {
    int Axis{0}; // 0=x,1=y,2=z
    double Sign{1.0}; // +1 max face, -1 min face
};

HitFace FindClosestHitFace(const Math::AABB& aabb, const Math::Vector3& point) {
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

Viewport::Viewport(const EngineContextPtr& context, QWidget* parent)
    : QWidget(parent),
      context_(context) {
    Cursor::SetCustomCursor(this);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void Viewport::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
    workspace_ = place != nullptr ? std::dynamic_pointer_cast<DataModel::Workspace>(place->FindService("Workspace")) : nullptr;
    selection_ = place != nullptr ? std::dynamic_pointer_cast<DataModel::Selection>(place->FindService("Selection")) : nullptr;
    historyService_ = place != nullptr
        ? std::dynamic_pointer_cast<DataModel::ChangeHistoryService>(place->FindService("ChangeHistoryService"))
        : nullptr;
    cameraController_.reset();
    if (workspace_ != nullptr) {
        const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
        if (camera != nullptr) {
            cameraController_ = std::make_unique<CameraController>(camera);
            cameraController_->SetSpeed(cameraSpeed_);
            cameraController_->SetShiftSpeed(cameraShiftSpeed_);
        }
    }
    EnsureGizmoSystem();
    if (context_ != nullptr && context_->RenderContext != nullptr) {
        context_->RenderContext->BindToPlace(place);
    }
    update();
}

void Viewport::Unbind() {
    place_.reset();
    workspace_.reset();
    selection_.reset();
    historyService_.reset();
    gizmoHistorySnapshot_.reset();
    leftMouseDown_ = false;
    gizmoDragging_ = false;
    partDragging_ = false;
    partDragState_.reset();
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Unbind();
        gizmoSystem_.reset();
    }
    cameraController_.reset();
    pressedKeys_.clear();
    pressedScanCodes_.clear();
    rightMouseDown_ = false;
    rightMousePanned_ = false;
    if (context_ != nullptr && context_->RenderContext != nullptr) {
        context_->RenderContext->Unbind();
    }
}

void Viewport::SetGizmoAlwaysOnTop(const bool value) {
    gizmoAlwaysOnTop_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void Viewport::SetGizmoIgnoreDiffuseSpecular(const bool value) {
    gizmoIgnoreDiffuseSpecular_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void Viewport::SetGizmoAlignByMagnitude(const bool value) {
    gizmoAlignByMagnitude_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void Viewport::SetSnapIncrement(const double value) {
    snapIncrement_ = std::max(0.0, value);
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
    }
}

void Viewport::SetCameraSpeed(const double speed) {
    cameraSpeed_ = speed;
    if (cameraController_ != nullptr) {
        cameraController_->SetSpeed(speed);
    }
}

void Viewport::SetCameraShiftSpeed(const double speed) {
    cameraShiftSpeed_ = speed;
    if (cameraController_ != nullptr) {
        cameraController_->SetShiftSpeed(speed);
    }
}

void Viewport::SetRenderingApiPreference(const Rendering::RenderApi api) {
    RecreateRenderContext(api);
}

void Viewport::paintEvent(QPaintEvent* event) {
    static_cast<void>(event);
    if (graphicsUnavailable_) {
        return;
    }

    static auto previous = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> delta = now - previous;
    previous = now;

    UpdateCamera(delta.count());
    UpdateGizmo(std::nullopt);

    if (leftMouseDown_ && (gizmoDragging_ || partDragging_) && workspace_ != nullptr) {
        const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
        if (camera != nullptr) {
            const QPoint local = mapFromGlobal(QCursor::pos());
            const Utils::Ray dragRay = Utils::ScreenPointToRay(
                static_cast<double>(local.x()),
                static_cast<double>(local.y()),
                width(),
                height(),
                camera
            );
            if (gizmoDragging_ && gizmoSystem_ != nullptr) {
                gizmoSystem_->UpdateDrag(dragRay);
            } else if (partDragging_) {
                UpdatePartDrag(dragRay);
            }
        }
    }

    if (context_ != nullptr && context_->RenderContext != nullptr) {
        std::vector<Rendering::Common::OverlayPrimitive> overlay;
        if (gizmoSystem_ != nullptr) {
            overlay = gizmoSystem_->GetRenderPrimitives();
        }
        AppendGizmoSelectionBox(overlay);
        AppendSelectionBoxInstances(overlay);
        context_->RenderContext->SetOverlayPrimitives(std::move(overlay));
    }

    try {
        EnsureGraphicsBound();
        if (context_ != nullptr && context_->RenderContext != nullptr) {
            context_->RenderContext->Render();
        }
    } catch (const Rendering::RenderingInitializationError& ex) {
        graphicsUnavailable_ = true;
        CriticalError::ShowGraphicsUnsupportedError(QString::fromUtf8(ex.what()));
        return;
    } catch (const std::exception& ex) {
        CriticalError::ShowUnexpectedNoReturnError(QString::fromUtf8(ex.what()));
    } catch (...) {
        CriticalError::ShowUnexpectedNoReturnError("Unknown error.");
    }

    static_cast<void>(gizmoAlwaysOnTop_);
    static_cast<void>(gizmoIgnoreDiffuseSpecular_);
    static_cast<void>(gizmoAlignByMagnitude_);
    static_cast<void>(cameraSpeed_);
    static_cast<void>(cameraShiftSpeed_);

    if (isVisible()) {
        update();
    }
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (context_ != nullptr && context_->RenderContext != nullptr && graphicsBound_) {
        context_->RenderContext->Resize(
            static_cast<std::uint32_t>(event->size().width()),
            static_cast<std::uint32_t>(event->size().height())
        );
    }
}

QPaintEngine* Viewport::paintEngine() const {
    return nullptr;
}

void Viewport::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        rightMouseDown_ = true;
        rightMousePanned_ = false;
        lockedCursorPos_ = event->globalPosition().toPoint();
        setFocus();
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    leftMouseDown_ = true;
    setFocus();
    if (cameraController_ == nullptr) {
        return;
    }

    const auto camera = workspace_ != nullptr
        ? workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>()
        : nullptr;
    if (camera == nullptr) {
        return;
    }

    const Utils::Ray ray = Utils::ScreenPointToRay(
        event->position().x(),
        event->position().y(),
        width(),
        height(),
        camera
    );

    UpdateGizmo(ray);
    if (CanDragGizmo() && gizmoSystem_ != nullptr && gizmoSystem_->TryBeginDrag(ray)) {
        BeginGizmoHistory(gizmoSystem_->GetTargetPart());
        gizmoDragging_ = true;
        return;
    }

    if (CanDragPart() && TryBeginPartDrag(ray)) {
        BeginGizmoHistory(partDragState_.has_value() ? partDragState_->Instance : nullptr);
        partDragging_ = true;
        return;
    }

    const auto previousSelection = selection_ != nullptr ? selection_->GetPrimary() : nullptr;
    PickSelection(ray);

    // Allow "press-select-drag" in one continuous LMB gesture.
    const auto currentSelection = selection_ != nullptr ? selection_->GetPrimary() : nullptr;
    if (currentSelection != nullptr && currentSelection != previousSelection && CanDragPart() && TryBeginPartDrag(ray)) {
        BeginGizmoHistory(partDragState_.has_value() ? partDragState_->Instance : nullptr);
        partDragging_ = true;
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        static_cast<void>(rightMousePanned_);
        rightMouseDown_ = false;
        rightMousePanned_ = false;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
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
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    if (rightMouseDown_ && cameraController_ != nullptr) {
        const QPoint current = event->globalPosition().toPoint();
        const QPoint delta = current - lockedCursorPos_;
        if (delta.manhattanLength() > 0) {
            rightMousePanned_ = true;
            cameraController_->Rotate(static_cast<double>(delta.x()), static_cast<double>(delta.y()));
            QCursor::setPos(lockedCursorPos_);
        }
        event->accept();
        return;
    }
    const auto camera = workspace_ != nullptr
        ? workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>()
        : nullptr;
    if (camera != nullptr) {
        const Utils::Ray ray = Utils::ScreenPointToRay(
            event->position().x(),
            event->position().y(),
            width(),
            height(),
            camera
        );
        UpdateGizmo(ray);
        if (gizmoDragging_ && gizmoSystem_ != nullptr) {
            gizmoSystem_->UpdateDrag(ray);
        } else if (partDragging_) {
            UpdatePartDrag(ray);
        }
    }

    QWidget::mouseMoveEvent(event);
}

void Viewport::wheelEvent(QWheelEvent* event) {
    if (cameraController_ != nullptr) {
        const double delta = static_cast<double>(event->angleDelta().y()) / 120.0;
        cameraController_->Move(cameraController_->GetForward(), delta, false);
    }
    event->accept();
}

void Viewport::keyPressEvent(QKeyEvent* event) {
    pressedKeys_.insert(event->key());
    pressedScanCodes_.insert(event->nativeScanCode());
    QWidget::keyPressEvent(event);
}

void Viewport::keyReleaseEvent(QKeyEvent* event) {
    pressedKeys_.erase(event->key());
    pressedScanCodes_.erase(event->nativeScanCode());
    QWidget::keyReleaseEvent(event);
}

void Viewport::UpdateCamera(const double deltaSeconds) {
    if (cameraController_ == nullptr) {
        return;
    }

    Math::Vector3 direction{};
    if (IsMoveForwardPressed()) {
        direction = direction + cameraController_->GetForward();
    }
    if (IsMoveBackwardPressed()) {
        direction = direction - cameraController_->GetForward();
    }
    if (IsMoveRightPressed()) {
        direction = direction + cameraController_->GetRight();
    }
    if (IsMoveLeftPressed()) {
        direction = direction - cameraController_->GetRight();
    }
    if (IsMoveUpPressed()) {
        direction = direction + cameraController_->GetUp();
    }
    if (IsMoveDownPressed()) {
        direction = direction - cameraController_->GetUp();
    }

    if (direction.Magnitude() > 0.0) {
        cameraController_->Move(direction.Unit(), deltaSeconds, IsSlowMovePressed());
    }
}

bool Viewport::IsMoveForwardPressed() const {
    return HasKey(Qt::Key_W) || HasScanCode(0x11);
}

bool Viewport::IsMoveBackwardPressed() const {
    return HasKey(Qt::Key_S) || HasScanCode(0x1F);
}

bool Viewport::IsMoveLeftPressed() const {
    return HasKey(Qt::Key_A) || HasScanCode(0x1E);
}

bool Viewport::IsMoveRightPressed() const {
    return HasKey(Qt::Key_D) || HasScanCode(0x20);
}

bool Viewport::IsMoveDownPressed() const {
    return HasKey(Qt::Key_Q) || HasScanCode(0x10);
}

bool Viewport::IsMoveUpPressed() const {
    return HasKey(Qt::Key_E) || HasScanCode(0x12);
}

bool Viewport::IsSlowMovePressed() const {
    return HasKey(Qt::Key_Shift) || HasScanCode(0x2A) || HasScanCode(0x36);
}

bool Viewport::HasKey(const int key) const {
    return pressedKeys_.find(key) != pressedKeys_.end();
}

bool Viewport::HasScanCode(const std::uint32_t sc) const {
    return pressedScanCodes_.find(sc) != pressedScanCodes_.end();
}

void Viewport::PickSelection(const Utils::Ray& ray) {
    if (selection_ == nullptr) {
        return;
    }

    const auto parts = CollectWorkspaceParts();

    const auto [hitPart, distance] = Utils::RaycastParts(ray, parts);
    static_cast<void>(distance);
    selection_->Set(hitPart);
}

bool Viewport::CanDragGizmo() const {
    if (context_ == nullptr || context_->EditorToolState == nullptr) {
        return false;
    }
    const Tool activeTool = context_->EditorToolState->GetActiveTool();
    return activeTool == Tool::MoveTool || activeTool == Tool::SizeTool;
}

void Viewport::EnsureGizmoSystem() {
    if (gizmoSystem_ != nullptr || workspace_ == nullptr) {
        return;
    }
    const auto camera = workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
    if (camera == nullptr) {
        return;
    }

    gizmoSystem_ = std::make_unique<GizmoSystem>();
    gizmoSystem_->Bind(camera);
    gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_, snapIncrement_);
}

void Viewport::UpdateGizmo(const std::optional<Utils::Ray>& ray) {
    EnsureGizmoSystem();
    if (gizmoSystem_ == nullptr || context_ == nullptr || context_->EditorToolState == nullptr || selection_ == nullptr) {
        return;
    }

    gizmoSystem_->Update(selection_, context_->EditorToolState->GetActiveTool());
    if (ray.has_value() && !gizmoDragging_ && !partDragging_) {
        gizmoSystem_->UpdateHover(ray.value());
    }
}

bool Viewport::CanDragPart() const {
    if (context_ == nullptr || context_->EditorToolState == nullptr || selection_ == nullptr) {
        return false;
    }
    const Tool activeTool = context_->EditorToolState->GetActiveTool();
    if (activeTool != Tool::SelectTool && activeTool != Tool::MoveTool) {
        return false;
    }
    const auto selected = std::dynamic_pointer_cast<Objects::BasePart>(selection_->GetPrimary());
    return selected != nullptr && selected->GetParent() != nullptr;
}

bool Viewport::TryBeginPartDrag(const Utils::Ray& ray) {
    if (!CanDragPart()) {
        return false;
    }

    const auto selected = std::dynamic_pointer_cast<Objects::BasePart>(selection_->GetPrimary());
    if (selected == nullptr) {
        return false;
    }
    const auto hitDistance = Utils::RaycastPartAABB(ray, selected);
    if (!hitDistance.has_value()) {
        return false;
    }

    // Start dragging only when the selected part is the front-most hit.
    const auto parts = CollectWorkspaceParts();
    const auto [frontMostPart, frontMostDistance] = Utils::RaycastParts(ray, parts);
    static_cast<void>(frontMostDistance);
    if (frontMostPart != selected) {
        return false;
    }

    const auto aabb = Utils::BuildPartWorldAABB(selected);
    const Math::Vector3 halfExtents = (aabb.Max - aabb.Min) * 0.5;
    const Math::Vector3 startPosition = selected->GetWorldPosition();
    const Math::Vector3 hitPoint = ray.Origin + ray.Direction * hitDistance.value();
    const Math::Vector3 grabOffset = hitPoint - startPosition;
    const Math::Vector3 size = selected->GetProperty("Size").value<Math::Vector3>();
    const double maxAxis = std::max({std::abs(size.x), std::abs(size.y), std::abs(size.z)});
    const double fallbackDepth = std::max(0.1, maxAxis * 2.0);

    partDragState_ = PartDragState{
        .Instance = selected,
        .StartPosition = startPosition,
        .HalfExtents = halfExtents,
        .GrabOffset = grabOffset,
        .StartRayOrigin = ray.Origin,
        .StartRayDirection = ray.Direction,
        .HasMoved = false,
        .FallbackDepth = fallbackDepth
    };
    return true;
}

void Viewport::UpdatePartDrag(const Utils::Ray& ray) {
    if (!partDragState_.has_value()) {
        return;
    }

    auto& state = partDragState_.value();
    if (state.Instance == nullptr || state.Instance->GetParent() == nullptr) {
        EndPartDrag();
        return;
    }

    if (!state.HasMoved) {
        if (!RayChanged(ray, state.StartRayOrigin, state.StartRayDirection)) {
            return;
        }
        state.HasMoved = true;
    }

    const Math::Vector3 fallbackHitPoint = ray.Origin + ray.Direction * std::max(0.0, state.FallbackDepth);
    Math::Vector3 newPosition = fallbackHitPoint - state.GrabOffset;
    bool hasHitPlacement = false;

    const auto parts = CollectWorkspaceParts(state.Instance);
    if (!parts.empty()) {
        const auto [hitPart, hitDistance] = Utils::RaycastParts(ray, parts);
        if (hitPart != nullptr && std::isfinite(hitDistance)) {
            const auto hitAabb = Utils::BuildPartWorldAABB(hitPart);
            const Math::Vector3 hitPoint = ray.Origin + ray.Direction * hitDistance;
            const HitFace face = FindClosestHitFace(hitAabb, hitPoint);
            newPosition = hitPoint - state.GrabOffset;
            hasHitPlacement = true;

            // Face-to-face alignment like lego bricks: exact contact on the hit face axis.
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
        // Free drag snapping is relative to drag start (not world origin grid).
        const Math::Vector3 delta = newPosition - state.StartPosition;
        newPosition = state.StartPosition + SnapPosition(delta);
    }

    state.Instance->SetProperty("Position", QVariant::fromValue(newPosition));
}

void Viewport::EndPartDrag() {
    partDragging_ = false;
    partDragState_.reset();
}

Math::Vector3 Viewport::SnapPosition(const Math::Vector3& value) const {
    return {
        SnapToStep(value.x, snapIncrement_),
        SnapToStep(value.y, snapIncrement_),
        SnapToStep(value.z, snapIncrement_)
    };
}

std::vector<std::shared_ptr<Objects::BasePart>> Viewport::CollectWorkspaceParts(
    const std::shared_ptr<Objects::BasePart>& ignore
) const {
    std::vector<std::shared_ptr<Objects::BasePart>> parts;
    if (workspace_ == nullptr) {
        return parts;
    }
    for (const auto& descendant : workspace_->GetDescendants()) {
        const auto part = std::dynamic_pointer_cast<Objects::BasePart>(descendant);
        if (part == nullptr || part->GetParent() == nullptr || part == ignore) {
            continue;
        }
        parts.push_back(part);
    }
    return parts;
}

void Viewport::AppendGizmoSelectionBox(std::vector<Rendering::Common::OverlayPrimitive>& overlay) const {
    if (selection_ == nullptr) {
        return;
    }

    const auto selected = std::dynamic_pointer_cast<Objects::BasePart>(selection_->GetPrimary());
    if (selected == nullptr || selected->GetParent() == nullptr) {
        return;
    }

    const bool ignoreLighting = gizmoIgnoreDiffuseSpecular_;
    const SelectionBoxStyle style{
        .Color = {0.0, 0.5, 1.0},
        .Alpha = 1.0F,
        .Metalness = 0.0F,
        .Roughness = 1.0F,
        .Emissive = ignoreLighting ? 1.0F : 0.0F,
        .IgnoreLighting = ignoreLighting,
        .AlwaysOnTop = true,
        .Thickness = 0.06
    };

    AppendSelectionBoxOutlinePrimitives(Utils::BuildPartWorldAABB(selected), style, overlay);
}

void Viewport::AppendSelectionBoxInstances(std::vector<Rendering::Common::OverlayPrimitive>& overlay) const {
    if (workspace_ == nullptr) {
        return;
    }

    for (const auto& descendant : workspace_->GetDescendants()) {
        const auto box = std::dynamic_pointer_cast<Objects::SelectionBox>(descendant);
        if (box == nullptr || !box->GetProperty("Visible").toBool()) {
            continue;
        }

        std::shared_ptr<Core::Instance> adorneeInstance = box->GetProperty("Adornee").value<std::shared_ptr<Core::Instance>>();
        auto adorneePart = std::dynamic_pointer_cast<Objects::BasePart>(adorneeInstance);
        if (adorneePart == nullptr) {
            adorneePart = std::dynamic_pointer_cast<Objects::BasePart>(box->GetParent());
        }
        if (adorneePart == nullptr || adorneePart->GetParent() == nullptr) {
            continue;
        }

        const float alpha = std::clamp(
            static_cast<float>(1.0 - box->GetProperty("Transparency").toDouble()),
            0.0F,
            1.0F
        );
        const bool ignoreLighting = box->GetProperty("IgnoreLighting").toBool();

        const SelectionBoxStyle style{
            .Color = box->GetProperty("Color").value<Math::Color3>(),
            .Alpha = alpha,
            .Metalness = static_cast<float>(box->GetProperty("Metalness").toDouble()),
            .Roughness = static_cast<float>(box->GetProperty("Roughness").toDouble()),
            .Emissive = static_cast<float>(box->GetProperty("Emissive").toDouble()),
            .IgnoreLighting = ignoreLighting,
            .AlwaysOnTop = box->GetProperty("AlwaysOnTop").toBool(),
            .Thickness = std::max(0.001, box->GetProperty("LineThickness").toDouble())
        };

        AppendSelectionBoxOutlinePrimitives(Utils::BuildPartWorldAABB(adorneePart), style, overlay);
    }
}

void Viewport::BeginGizmoHistory(const std::shared_ptr<Objects::BasePart>& targetOverride) {
    std::shared_ptr<Objects::BasePart> target = targetOverride;
    if (target == nullptr && gizmoSystem_ != nullptr) {
        target = gizmoSystem_->GetTargetPart();
    }
    if (target == nullptr) {
        gizmoHistorySnapshot_.reset();
        return;
    }
    gizmoHistorySnapshot_ = GizmoHistorySnapshot{
        .Instance = target,
        .Position = target->GetProperty("Position").value<Math::Vector3>(),
        .Size = target->GetProperty("Size").value<Math::Vector3>()
    };
}

void Viewport::CommitGizmoHistory() {
    if (!gizmoHistorySnapshot_.has_value() || historyService_ == nullptr) {
        gizmoHistorySnapshot_.reset();
        return;
    }

    const auto snapshot = gizmoHistorySnapshot_.value();
    gizmoHistorySnapshot_.reset();

    const auto instance = snapshot.Instance;
    if (instance == nullptr || instance->GetParent() == nullptr) {
        return;
    }

    const auto newPos = instance->GetProperty("Position").value<Math::Vector3>();
    const auto newSize = instance->GetProperty("Size").value<Math::Vector3>();
    if (snapshot.Position == newPos && snapshot.Size == newSize) {
        return;
    }
    if (historyService_->IsRecording()) {
        return;
    }

    historyService_->BeginRecording("Gizmo Transform");
    try {
        if (!(snapshot.Size == newSize)) {
            historyService_->Record(
                std::make_shared<Utils::SetPropertyCommand>(
                    instance,
                    "Size",
                    QVariant::fromValue(snapshot.Size),
                    QVariant::fromValue(newSize)
                )
            );
        }
        if (!(snapshot.Position == newPos)) {
            historyService_->Record(
                std::make_shared<Utils::SetPropertyCommand>(
                    instance,
                    "Position",
                    QVariant::fromValue(snapshot.Position),
                    QVariant::fromValue(newPos)
                )
            );
        }
    } catch (const std::exception& ex) {
        CriticalError::ShowUnexpectedNoReturnError(QString::fromUtf8(ex.what()));
    }
    historyService_->FinishRecording();
}

void Viewport::EnsureGraphicsBound() {
    if (graphicsUnavailable_) {
        return;
    }
    if (graphicsBound_) {
        return;
    }
    if (context_ == nullptr || context_->RenderContext == nullptr) {
        return;
    }
    if (width() <= 0 || height() <= 0) {
        return;
    }

    const auto nativeHandle = reinterpret_cast<void*>(winId());
    context_->RenderContext->AttachToNativeWindow(
        nativeHandle,
        static_cast<std::uint32_t>(width()),
        static_cast<std::uint32_t>(height())
    );
    context_->RenderContext->SetClearColor(1.0F, 1.0F, 1.0F, 1.0F);
    graphicsBound_ = true;
}

void Viewport::RecreateRenderContext(const Rendering::RenderApi api) {
    if (context_ == nullptr) {
        return;
    }

    if (context_->RenderContext != nullptr) {
        context_->RenderContext->Unbind();
    }
    // Destroy old backend before recreating native window handle so GL/Vulkan
    // teardown can release resources against the original HWND.
    context_->RenderContext.reset();

    // OpenGL sets a pixel format on the viewport HDC that cannot be changed.
    // Recreate the native window to guarantee a fresh surface for Vulkan.
    if (testAttribute(Qt::WA_NativeWindow)) {
        const bool hadFocus = hasFocus();
        const bool wasVisible = isVisible();
        hide();
        destroy(true, true);
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        // Force immediate native handle recreation.
        static_cast<void>(winId());
        if (wasVisible) {
            show();
        }
        if (hadFocus) {
            setFocus();
        }
    }

    context_->RenderContext = Rendering::CreateRenderContext(api);
    graphicsUnavailable_ = false;
    graphicsBound_ = false;

    if (context_->RenderContext == nullptr) {
        return;
    }

    if (const auto place = place_.lock(); place != nullptr) {
        context_->RenderContext->BindToPlace(place);
    }

    update();
}

} // namespace Lvs::Engine::Core
