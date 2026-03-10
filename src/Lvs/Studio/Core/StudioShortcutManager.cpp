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

} // namespace

QString StudioShortcutManager::SettingKey(const StudioShortcutAction action) {
    switch (action) {
        case StudioShortcutAction::ToolSelect:
            return "Shortcut.Tool.Select";
        case StudioShortcutAction::ToolMove:
            return "Shortcut.Tool.Move";
        case StudioShortcutAction::ToolSize:
            return "Shortcut.Tool.Size";
        case StudioShortcutAction::Duplicate:
            return "Shortcut.Edit.Duplicate";
        case StudioShortcutAction::Delete:
            return "Shortcut.Edit.Delete";
        case StudioShortcutAction::Copy:
            return "Shortcut.Edit.Copy";
        case StudioShortcutAction::Cut:
            return "Shortcut.Edit.Cut";
        case StudioShortcutAction::Paste:
            return "Shortcut.Edit.Paste";
        case StudioShortcutAction::PasteInto:
            return "Shortcut.Edit.PasteInto";
        default:
            return {};
    }
}

QList<QKeySequence> StudioShortcutManager::Shortcuts(const StudioShortcutAction action) {
    const QString key = SettingKey(action);
    if (key.isEmpty()) {
        return {};
    }
    return ParseShortcuts(Settings::Get(key).toString());
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

