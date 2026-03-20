#include "Lvs/Studio/Core/Window.hpp"

#include <QApplication>
#include <QCloseEvent>
#include <QOpenGLContext>
#include <QProgressBar>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace Lvs::Engine::Core {

Window::Window(const QString& appName, QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(appName);

    central_ = new QWidget(this);
    layout_ = new QVBoxLayout(central_);
    layout_->setSpacing(0);
    layout_->setContentsMargins(0, 0, 0, 0);

    setCentralWidget(central_);

    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);

    busyProgress_ = new QProgressBar(this);
    busyProgress_->setRange(0, 0);
    busyProgress_->setVisible(false);
    busyProgress_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    statusBar_->addPermanentWidget(busyProgress_);
}

QWidget* Window::GetCentral() const {
    return central_;
}

void Window::AddWidget(QWidget* widget) const {
    layout_->addWidget(widget);
}

void Window::SetBeforeCloseHandler(std::function<bool()> handler) {
    beforeCloseHandler_ = std::move(handler);
}

void Window::ShowBusy(const QString& message) {
    ++statusClearToken_;
    statusBar_->showMessage(message);
    busyProgress_->setVisible(true);

    if (QOpenGLContext::currentContext() == nullptr) {
        QApplication::processEvents();
    }
}

void Window::HideBusy(const QString& message) {
    busyProgress_->setVisible(false);
    statusBar_->showMessage(message);

    ++statusClearToken_;
    const int clearToken = statusClearToken_;
    if (!message.isEmpty()) {
        QTimer::singleShot(3000, this, [this, clearToken]() {
            ClearStatusIfToken(clearToken);
        });
    }

    if (QOpenGLContext::currentContext() == nullptr) {
        QApplication::processEvents();
    }
}

void Window::closeEvent(QCloseEvent* event) {
    if (beforeCloseHandler_ && !beforeCloseHandler_()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void Window::ClearStatusIfToken(const int token) {
    if (token != statusClearToken_) {
        return;
    }
    if (busyProgress_->isVisible()) {
        return;
    }
    statusBar_->clearMessage();
}

} // namespace Lvs::Engine::Core
