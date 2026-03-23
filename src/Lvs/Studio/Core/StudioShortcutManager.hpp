#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

class QAction;
class QKeyEvent;

namespace Lvs::Studio::Core {

enum class StudioShortcutAction {
    ToolSelect,
    ToolMove,
    ToolSize,
    ToggleLocalSpace,
    Undo,
    Redo,
    Duplicate,
    Delete,
    SelectAll,
    Group,
    Ungroup,
    Copy,
    Cut,
    Paste,
    PasteInto,
    FocusOnSelection
};

class StudioShortcutManager final {
public:
    [[nodiscard]] static QString SettingKey(StudioShortcutAction action);
    [[nodiscard]] static QList<QKeySequence> Shortcuts(StudioShortcutAction action);
    [[nodiscard]] static bool Matches(StudioShortcutAction action, const QKeyEvent& event);
    static void ApplyToAction(QAction& action, StudioShortcutAction shortcut);
    static void ApplyIconToAction(QAction& action, StudioShortcutAction shortcut);
};

} // namespace Lvs::Studio::Core
