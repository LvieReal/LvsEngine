#pragma once

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Core/GizmoSystem.hpp"
#include "Lvs/Engine/Rendering/Vulkan/OverlayPrimitive.hpp"

#include <QWidget>

#include <QPoint>
#include <unordered_set>

#include <memory>
#include <vector>

class QPaintEngine;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;

namespace Lvs::Engine::DataModel {
class ChangeHistoryService;
class Place;
class Selection;
class Workspace;
}

namespace Lvs::Engine::Objects {
class BasePart;
}

namespace Lvs::Engine::Core {

class CameraController;

class Viewport final : public QWidget {
public:
    Viewport(const EngineContextPtr& context, QWidget* parent = nullptr);

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();

    void SetGizmoAlwaysOnTop(bool value);
    void SetGizmoIgnoreDiffuseSpecular(bool value);
    void SetGizmoAlignByMagnitude(bool value);
    void SetSnapIncrement(double value);
    void SetCameraSpeed(double speed);
    void SetCameraShiftSpeed(double speed);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    QPaintEngine* paintEngine() const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void EnsureVulkanBound();
    void UpdateCamera(double deltaSeconds);
    void PickSelection(const Utils::Ray& ray);
    bool CanDragGizmo() const;
    void EnsureGizmoSystem();
    void UpdateGizmo(const std::optional<Utils::Ray>& ray);
    bool CanDragPart() const;
    bool TryBeginPartDrag(const Utils::Ray& ray);
    void UpdatePartDrag(const Utils::Ray& ray);
    void EndPartDrag();
    Math::Vector3 SnapPosition(const Math::Vector3& value) const;
    std::vector<std::shared_ptr<Objects::BasePart>> CollectWorkspaceParts(
        const std::shared_ptr<Objects::BasePart>& ignore = nullptr
    ) const;
    void AppendGizmoSelectionBox(
        std::vector<Rendering::Vulkan::OverlayPrimitive>& overlay
    ) const;
    void AppendSelectionBoxInstances(
        std::vector<Rendering::Vulkan::OverlayPrimitive>& overlay
    ) const;
    void BeginGizmoHistory(const std::shared_ptr<Objects::BasePart>& targetOverride = nullptr);
    void CommitGizmoHistory();
    bool IsMoveForwardPressed() const;
    bool IsMoveBackwardPressed() const;
    bool IsMoveLeftPressed() const;
    bool IsMoveRightPressed() const;
    bool IsMoveDownPressed() const;
    bool IsMoveUpPressed() const;
    bool IsSlowMovePressed() const;
    bool HasKey(int key) const;
    bool HasScanCode(std::uint32_t sc) const;

    EngineContextPtr context_;
    std::weak_ptr<DataModel::Place> place_;
    std::shared_ptr<DataModel::Workspace> workspace_;
    std::shared_ptr<DataModel::Selection> selection_;
    std::shared_ptr<DataModel::ChangeHistoryService> historyService_;
    std::unique_ptr<CameraController> cameraController_;
    std::unique_ptr<GizmoSystem> gizmoSystem_;
    bool leftMouseDown_{false};
    bool gizmoDragging_{false};
    bool partDragging_{false};
    struct PartDragState {
        std::shared_ptr<Objects::BasePart> Instance;
        Math::Vector3 StartPosition{};
        Math::Vector3 HalfExtents{};
        Math::Vector3 GrabOffset{};
        Math::Vector3 StartRayOrigin{};
        Math::Vector3 StartRayDirection{};
        bool HasMoved{false};
        double FallbackDepth{8.0};
    };
    std::optional<PartDragState> partDragState_;
    struct GizmoHistorySnapshot {
        std::shared_ptr<Objects::BasePart> Instance;
        Math::Vector3 Position;
        Math::Vector3 Size;
    };
    std::optional<GizmoHistorySnapshot> gizmoHistorySnapshot_;
    std::unordered_set<int> pressedKeys_;
    std::unordered_set<std::uint32_t> pressedScanCodes_;
    QPoint lockedCursorPos_{};
    bool rightMouseDown_{false};
    bool rightMousePanned_{false};
    bool vulkanBound_{false};
    bool gizmoAlwaysOnTop_{true};
    bool gizmoIgnoreDiffuseSpecular_{true};
    bool gizmoAlignByMagnitude_{true};
    double snapIncrement_{1.0};
    double cameraSpeed_{15.0};
    double cameraShiftSpeed_{5.0};
};

} // namespace Lvs::Engine::Core
