#pragma once

#include "Lvs/Engine/Context.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"

#include <QWidget>

#include <QPoint>
#include <unordered_set>

#include <memory>
#include <optional>
#include <vector>

class QPaintEngine;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QWheelEvent;

namespace Lvs::Engine::DataModel {
class Place;
class Workspace;
}

namespace Lvs::Engine::Objects {
class BasePart;
class Camera;
}

namespace Lvs::Engine::Rendering {
enum class RenderApi;
}

namespace Lvs::Engine::Core {

class CameraController;
class ViewportToolLayer;

class Viewport final : public QWidget {
public:
    Viewport(const EngineContextPtr& context, QWidget* parent = nullptr);
    ~Viewport() override;

    void BindToPlace(const std::shared_ptr<DataModel::Place>& place);
    void Unbind();

    void SetCameraSpeed(double speed);
    void SetCameraShiftSpeed(double speed);
    void SetRenderingApiPreference(Rendering::RenderApi api);
    void SetToolLayer(std::unique_ptr<ViewportToolLayer> layer);
    [[nodiscard]] ViewportToolLayer* GetToolLayer() const;
    void FocusOnPart(const std::shared_ptr<Objects::BasePart>& part);

    [[nodiscard]] bool WasRightMousePanned() const;

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
    void focusOutEvent(QFocusEvent* event) override;

private:
    void EnsureGraphicsBound();
    void UpdateCamera(double deltaSeconds);
    bool IsMoveForwardPressed() const;
    bool IsMoveBackwardPressed() const;
    bool IsMoveLeftPressed() const;
    bool IsMoveRightPressed() const;
    bool IsMoveDownPressed() const;
    bool IsMoveUpPressed() const;
    bool IsSlowMovePressed() const;
    bool HasKey(int key) const;
    bool HasScanCode(std::uint32_t sc) const;
    void RecreateRenderContext(Rendering::RenderApi api);
    [[nodiscard]] std::shared_ptr<Objects::Camera> GetCurrentCamera() const;
    [[nodiscard]] std::optional<Utils::Ray> BuildRay(double x, double y) const;

    EngineContextPtr context_;
    std::weak_ptr<DataModel::Place> place_;
    std::shared_ptr<DataModel::Workspace> workspace_;
    std::unique_ptr<CameraController> cameraController_;
    std::unique_ptr<ViewportToolLayer> toolLayer_;
    std::unordered_set<int> pressedKeys_;
    std::unordered_set<std::uint32_t> pressedScanCodes_;
    QPoint lockedCursorPos_{};
    bool rightMouseDown_{false};
    bool rightMousePanned_{false};
    bool graphicsBound_{false};
    bool graphicsUnavailable_{false};
    double cameraSpeed_{15.0};
    double cameraShiftSpeed_{5.0};
};

} // namespace Lvs::Engine::Core
