#include "Lvs/Studio/Core/StudioShortcutManager.hpp"

#include "Lvs/Studio/Core/Settings.hpp"

#include <QAction>
#include <QKeyEvent>
#include <QRegularExpression>

namespace Lvs::Studio::Core {

namespace {

QList<QKeySequence> ParseShortcuts(const QString& raw) {
    QList<QKeySequence> out;
    const QStringList tokens = raw.split(QRegularExpression("[;,]"), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        const QKeySequence seq = QKeySequence::fromString(token.trimmed(), QKeySequence::PortableText);
        if (!seq.isEmpty()) {
            out.push_back(seq);
        }
    }
    return out;
}

QKeySequence PressedSequence(const QKeyEvent& event) {
    return QKeySequence(event.key() | int(event.modifiers()));
}

QList<QKeySequence> DefaultShortcuts(const StudioShortcutAction action) {
    switch (action) {
        case StudioShortcutAction::ToolSelect:
            return ParseShortcuts("1;Shift+1");
        case StudioShortcutAction::ToolMove:
            return ParseShortcuts("2;Shift+2");
        case StudioShortcutAction::ToolSize:
            return ParseShortcuts("3;Shift+3");
        case StudioShortcutAction::Undo:
            return {QKeySequence(QKeySequence::Undo), QKeySequence("Ctrl+Z")};
        case StudioShortcutAction::Redo:
            return {QKeySequence(QKeySequence::Redo), QKeySequence("Ctrl+Y"), QKeySequence("Ctrl+Shift+Z")};
        case StudioShortcutAction::Duplicate:
            return ParseShortcuts("Ctrl+D");
        case StudioShortcutAction::Delete:
            return ParseShortcuts("Delete");
        case StudioShortcutAction::Copy:
            return ParseShortcuts("Ctrl+C");
        case StudioShortcutAction::Cut:
            return ParseShortcuts("Ctrl+X");
        case StudioShortcutAction::Paste:
            return ParseShortcuts("Ctrl+V");
        case StudioShortcutAction::PasteInto:
            return ParseShortcuts("Ctrl+Shift+V");
        default:
            return {};
    }
}

} // namespace

QString StudioShortcutManager::SettingKey(const StudioShortcutAction action) {
    switch (action) {
        case StudioShortcutAction::ToolSelect:
            return "Shortcut.Tool.Select";
        case StudioShortcutAction::ToolMove:
            return "Shortcut.Tool.Move";
        case StudioShortcutAction::ToolSize:
            return "Shortcut.Tool.Size";
        default:
            return {};
    }
}

QList<QKeySequence> StudioShortcutManager::Shortcuts(const StudioShortcutAction action) {
    const QString key = SettingKey(action);
    if (key.isEmpty() || !Settings::All().contains(key)) {
        return DefaultShortcuts(action);
    }

    const QList<QKeySequence> configured = ParseShortcuts(Settings::Get(key).toString());
    if (!configured.isEmpty()) {
        return configured;
    }
    return DefaultShortcuts(action);
}

bool StudioShortcutManager::Matches(const StudioShortcutAction action, const QKeyEvent& event) {
    const QKeySequence pressed = PressedSequence(event);
    const QList<QKeySequence> shortcuts = Shortcuts(action);
    for (const QKeySequence& shortcut : shortcuts) {
        if (shortcut.matches(pressed) == QKeySequence::ExactMatch) {
            return true;
        }
    }
    return false;
}

void StudioShortcutManager::ApplyToAction(QAction& action, const StudioShortcutAction shortcut) {
    const QList<QKeySequence> shortcuts = Shortcuts(shortcut);
    if (shortcuts.isEmpty()) {
        return;
    }
    action.setShortcuts(shortcuts);
}

} // namespace Lvs::Studio::Core
