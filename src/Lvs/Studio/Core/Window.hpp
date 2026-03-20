#pragma once

#include <QMainWindow>

#include <functional>

class QVBoxLayout;
class QWidget;
class QStatusBar;
class QProgressBar;
class QCloseEvent;

namespace Lvs::Engine::Core {

class Window final : public QMainWindow {
public:
    explicit Window(const QString& appName, QWidget* parent = nullptr);

    QWidget* GetCentral() const;
    void AddWidget(QWidget* widget) const;
    void SetBeforeCloseHandler(std::function<bool()> handler);
    void ShowBusy(const QString& message = "Working...");
    void HideBusy(const QString& message = {});

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void ClearStatusIfToken(int token);

    QWidget* central_{nullptr};
    QVBoxLayout* layout_{nullptr};
    std::function<bool()> beforeCloseHandler_{};
    int statusClearToken_{0};
    QStatusBar* statusBar_{nullptr};
    QProgressBar* busyProgress_{nullptr};
};

} // namespace Lvs::Engine::Core
