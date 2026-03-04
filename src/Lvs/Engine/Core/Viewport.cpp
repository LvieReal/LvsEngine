#include "Lvs/Engine/Core/Viewport.hpp"

#include "Lvs/Engine/Core/CameraController.hpp"
#include "Lvs/Engine/Core/Cursor.hpp"
#include "Lvs/Engine/Core/RegularError.hpp"
#include "Lvs/Engine/DataModel/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Selection.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Rendering/Vulkan/OverlayPrimitive.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
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
#include <optional>
#include <utility>
#include <vector>

namespace Lvs::Engine::Core {

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
    if (context_ != nullptr && context_->Vulkan != nullptr) {
        context_->Vulkan->BindToPlace(place);
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
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Unbind();
        gizmoSystem_.reset();
    }
    cameraController_.reset();
    pressedKeys_.clear();
    pressedScanCodes_.clear();
    rightMouseDown_ = false;
    if (context_ != nullptr && context_->Vulkan != nullptr) {
        context_->Vulkan->Unbind();
    }
}

void Viewport::SetGizmoAlwaysOnTop(const bool value) {
    gizmoAlwaysOnTop_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_);
    }
}

void Viewport::SetGizmoIgnoreDiffuseSpecular(const bool value) {
    gizmoIgnoreDiffuseSpecular_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_);
    }
}

void Viewport::SetGizmoAlignByMagnitude(const bool value) {
    gizmoAlignByMagnitude_ = value;
    if (gizmoSystem_ != nullptr) {
        gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_);
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

void Viewport::paintEvent(QPaintEvent* event) {
    static_cast<void>(event);
    static auto previous = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> delta = now - previous;
    previous = now;

    UpdateCamera(delta.count());
    UpdateGizmo(std::nullopt);

    if (context_ != nullptr && context_->Vulkan != nullptr) {
        std::vector<Rendering::Vulkan::OverlayPrimitive> overlay;
        if (gizmoSystem_ != nullptr) {
            const auto& primitives = gizmoSystem_->GetRenderPrimitives();
            overlay.reserve(primitives.size());
            for (const auto& p : primitives) {
                overlay.push_back(Rendering::Vulkan::OverlayPrimitive{
                    .Model = p.Model,
                    .Shape = p.Shape,
                    .Color = p.Color,
                    .Alpha = p.Alpha,
                    .Metalness = p.Metalness,
                    .Roughness = p.Roughness,
                    .Emissive = p.Emissive,
                    .IgnoreLighting = p.IgnoreLighting,
                    .AlwaysOnTop = p.AlwaysOnTop
                });
            }
        }
        context_->Vulkan->SetOverlayPrimitives(std::move(overlay));
    }

    try {
        EnsureVulkanBound();
        if (context_ != nullptr && context_->Vulkan != nullptr) {
            context_->Vulkan->Render();
        }
    } catch (const std::exception& ex) {
        RegularError::ShowErrorFromException(ex);
    } catch (...) {
        RegularError::ShowUnknownError();
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
    if (context_ != nullptr && context_->Vulkan != nullptr && vulkanBound_) {
        context_->Vulkan->Resize(
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
        BeginGizmoHistory();
        gizmoDragging_ = true;
        return;
    }

    PickSelection(ray);
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        rightMouseDown_ = false;
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
    if (workspace_ == nullptr || selection_ == nullptr) {
        return;
    }

    std::vector<std::shared_ptr<Objects::BasePart>> parts;
    for (const auto& descendant : workspace_->GetDescendants()) {
        const auto part = std::dynamic_pointer_cast<Objects::BasePart>(descendant);
        if (part == nullptr) {
            continue;
        }
        parts.push_back(part);
    }

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
    gizmoSystem_->Configure(gizmoAlwaysOnTop_, gizmoIgnoreDiffuseSpecular_, gizmoAlignByMagnitude_);
}

void Viewport::UpdateGizmo(const std::optional<Utils::Ray>& ray) {
    EnsureGizmoSystem();
    if (gizmoSystem_ == nullptr || context_ == nullptr || context_->EditorToolState == nullptr || selection_ == nullptr) {
        return;
    }

    gizmoSystem_->Update(selection_, context_->EditorToolState->GetActiveTool());
    if (ray.has_value() && !gizmoDragging_) {
        gizmoSystem_->UpdateHover(ray.value());
    }
}

void Viewport::BeginGizmoHistory() {
    if (gizmoSystem_ == nullptr) {
        gizmoHistorySnapshot_.reset();
        return;
    }
    const auto target = gizmoSystem_->GetTargetPart();
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
        RegularError::ShowErrorFromException(ex);
    }
    historyService_->FinishRecording();
}

void Viewport::EnsureVulkanBound() {
    if (vulkanBound_) {
        return;
    }
    if (context_ == nullptr || context_->Vulkan == nullptr) {
        return;
    }
    if (width() <= 0 || height() <= 0) {
        return;
    }

    const auto nativeHandle = reinterpret_cast<void*>(winId());
    context_->Vulkan->AttachToNativeWindow(
        nativeHandle,
        static_cast<std::uint32_t>(width()),
        static_cast<std::uint32_t>(height())
    );
    context_->Vulkan->SetClearColor(1.0F, 1.0F, 1.0F, 1.0F);
    vulkanBound_ = true;
}

} // namespace Lvs::Engine::Core
