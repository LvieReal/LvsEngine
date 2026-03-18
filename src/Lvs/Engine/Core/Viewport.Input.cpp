#include "Lvs/Engine/Core/Viewport.hpp"

#include "Lvs/Engine/Core/CameraController.hpp"
#include "Lvs/Engine/Core/ViewportToolLayer.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <QCursor>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <Qt>

namespace Lvs::Engine::Core {

void Viewport::mousePressEvent(QMouseEvent* event) {
    LVS_BENCH_SCOPE("Viewport::mousePressEvent");
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

    setFocus();
    if (toolLayer_ != nullptr) {
        LVS_BENCH_SCOPE("Viewport::ToolLayer::OnMousePress");
        toolLayer_->OnMousePress(event, BuildRay(event->position().x(), event->position().y()));
    }
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
    LVS_BENCH_SCOPE("Viewport::mouseReleaseEvent");
    if (event->button() == Qt::RightButton) {
        static_cast<void>(rightMousePanned_);
        rightMouseDown_ = false;
        rightMousePanned_ = false;
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (toolLayer_ != nullptr) {
            LVS_BENCH_SCOPE("Viewport::ToolLayer::OnMouseRelease");
            toolLayer_->OnMouseRelease(event);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
    LVS_BENCH_SCOPE("Viewport::mouseMoveEvent");
    if (rightMouseDown_ && cameraController_ != nullptr) {
        const QPoint current = event->globalPosition().toPoint();
        const QPoint delta = current - lockedCursorPos_;
        if (delta.manhattanLength() > 0) {
            rightMousePanned_ = true;
            {
                LVS_BENCH_SCOPE("Viewport::CameraController::Rotate");
                cameraController_->Rotate(static_cast<double>(delta.x()), static_cast<double>(delta.y()));
            }
            QCursor::setPos(lockedCursorPos_);
        }
        event->accept();
        return;
    }

    if (toolLayer_ != nullptr) {
        LVS_BENCH_SCOPE("Viewport::ToolLayer::OnMouseMove");
        toolLayer_->OnMouseMove(event, BuildRay(event->position().x(), event->position().y()));
    }

    QWidget::mouseMoveEvent(event);
}

void Viewport::wheelEvent(QWheelEvent* event) {
    LVS_BENCH_SCOPE("Viewport::wheelEvent");
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

void Viewport::focusOutEvent(QFocusEvent* event) {
    pressedKeys_.clear();
    pressedScanCodes_.clear();
    if (toolLayer_ != nullptr) {
        toolLayer_->OnFocusOut();
    }
    QWidget::focusOutEvent(event);
}

} // namespace Lvs::Engine::Core
