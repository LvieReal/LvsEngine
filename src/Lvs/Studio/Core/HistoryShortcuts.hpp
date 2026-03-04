#pragma once

#include <QObject>
#include <memory>

class QAction;
class QApplication;
class QEvent;
class QKeyEvent;
class QWidget;

namespace Lvs::Engine {
struct EngineContext;
using EngineContextPtr = std::shared_ptr<EngineContext>;
}

namespace Lvs::Studio::Core {

class HistoryShortcuts final : public QObject {
public:
    HistoryShortcuts(QApplication& app, QWidget& window, const Engine::EngineContextPtr& context);
    ~HistoryShortcuts() override;

    void SetContext(const Engine::EngineContextPtr& context);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool IsUndoKey(const QKeyEvent& event) const;
    bool IsRedoKey(const QKeyEvent& event) const;
    void Undo() const;
    void Redo() const;

    Engine::EngineContextPtr context_;
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
};

} // namespace Lvs::Studio::Core
