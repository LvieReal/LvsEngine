#include "Lvs/Studio/Theme.hpp"

#include "Lvs/Engine/Utils/SourcePath.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QApplication>
#include <QBrush>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QColor>

namespace Lvs::Studio::Theme {

namespace {
Core::Settings::Connection g_themeConnection;

QPalette BuildLightPalette() {
    QPalette palette;
    const QColor window(245, 245, 245);
    const QColor base(255, 255, 255);
    const QColor alt(240, 240, 240);
    const QColor text(24, 24, 24);
    const QColor button(248, 248, 248);
    const QColor highlight(0, 120, 215);
    const QColor placeholder(120, 120, 120);
    const QColor inactiveText(88, 88, 88);
    const QColor disabledText(150, 150, 150);

    palette.setColor(QPalette::Active, QPalette::Window, window);
    palette.setColor(QPalette::Active, QPalette::WindowText, text);
    palette.setColor(QPalette::Active, QPalette::Base, base);
    palette.setColor(QPalette::Active, QPalette::AlternateBase, alt);
    palette.setColor(QPalette::Active, QPalette::Text, text);
    palette.setColor(QPalette::Active, QPalette::Button, button);
    palette.setColor(QPalette::Active, QPalette::ButtonText, text);
    palette.setColor(QPalette::Active, QPalette::Highlight, highlight);
    palette.setColor(QPalette::Active, QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Active, QPalette::ToolTipBase, QColor(255, 255, 220));
    palette.setColor(QPalette::Active, QPalette::ToolTipText, text);
    palette.setColor(QPalette::Active, QPalette::PlaceholderText, placeholder);
    palette.setColor(QPalette::Active, QPalette::Accent, highlight);

    palette.setColor(QPalette::Inactive, QPalette::Window, window);
    palette.setColor(QPalette::Inactive, QPalette::WindowText, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::Base, base);
    palette.setColor(QPalette::Inactive, QPalette::AlternateBase, alt);
    palette.setColor(QPalette::Inactive, QPalette::Text, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::Button, button);
    palette.setColor(QPalette::Inactive, QPalette::ButtonText, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(157, 188, 219));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(255, 255, 220));
    palette.setColor(QPalette::Inactive, QPalette::ToolTipText, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, QColor(148, 148, 148));
    palette.setColor(QPalette::Inactive, QPalette::Accent, highlight);

    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(242, 242, 242));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(245, 245, 245));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(237, 237, 237));
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(242, 242, 242));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(190, 210, 230));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(250, 250, 230));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(168, 168, 168));
    palette.setColor(QPalette::Disabled, QPalette::Accent, QColor(120, 160, 200));
    return palette;
}

QPalette BuildDarkPalette() {
    QPalette palette;
    const QColor window(45, 45, 45);
    const QColor base(30, 30, 30);
    const QColor alt(53, 53, 53);
    const QColor text(232, 232, 232);
    const QColor button(60, 60, 60);
    const QColor highlight(0, 120, 215);
    const QColor inactiveText(185, 185, 185);
    const QColor disabledText(128, 128, 128);

    palette.setColor(QPalette::Active, QPalette::Window, window);
    palette.setColor(QPalette::Active, QPalette::WindowText, text);
    palette.setColor(QPalette::Active, QPalette::Base, base);
    palette.setColor(QPalette::Active, QPalette::AlternateBase, alt);
    palette.setColor(QPalette::Active, QPalette::Text, text);
    palette.setColor(QPalette::Active, QPalette::Button, button);
    palette.setColor(QPalette::Active, QPalette::ButtonText, text);
    palette.setColor(QPalette::Active, QPalette::Highlight, highlight);
    palette.setColor(QPalette::Active, QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Active, QPalette::ToolTipBase, QColor(30, 30, 30));
    palette.setColor(QPalette::Active, QPalette::ToolTipText, text);
    palette.setColor(QPalette::Active, QPalette::PlaceholderText, QColor(140, 140, 140));
    palette.setColor(QPalette::Active, QPalette::Accent, highlight);

    palette.setColor(QPalette::Inactive, QPalette::Window, window);
    palette.setColor(QPalette::Inactive, QPalette::WindowText, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::Base, base);
    palette.setColor(QPalette::Inactive, QPalette::AlternateBase, alt);
    palette.setColor(QPalette::Inactive, QPalette::Text, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::Button, button);
    palette.setColor(QPalette::Inactive, QPalette::ButtonText, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QColor(40, 110, 170));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(30, 30, 30));
    palette.setColor(QPalette::Inactive, QPalette::ToolTipText, inactiveText);
    palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, QColor(120, 120, 120));
    palette.setColor(QPalette::Inactive, QPalette::Accent, highlight);

    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(42, 42, 42));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(35, 35, 35));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(44, 44, 44));
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(52, 52, 52));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(70, 70, 70));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(180, 180, 180));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(35, 35, 35));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipText, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(96, 96, 96));
    palette.setColor(QPalette::Disabled, QPalette::Accent, QColor(70, 110, 160));
    return palette;
}

void ApplyThemeValue(QApplication& app, const QString& theme) {
    const QString baseFontCss = QString(
        "QWidget { font-family: \"%1\"; font-size: %2px; }"
    ).arg(app.font().family()).arg(app.font().pixelSize());

    if (theme.compare("Dark", Qt::CaseInsensitive) == 0) {
        app.setPalette(BuildDarkPalette());
        app.setStyleSheet(
            baseFontCss +
            "QMenu::separator { height: 1px; background: #525252; margin: 4px 6px; }"
            "QMenuBar::item:selected, QMenu::item:selected { background-color: #3f3f3f; color: #e8e8e8; }"
            "QToolButton:hover { background-color: #3f3f3f; color: #e8e8e8; }"
            "QToolBar#TopBar { spacing: 2px; padding: 1px 4px; }"
            "QToolButton#TopBarAction { padding: 2px 6px; margin: 0px; }"
            "QToolButton#TopBarAction::menu-indicator { image: none; width: 0px; }"
            "QToolBar#EditModes { spacing: 2px; padding: 1px 4px; }"
            "QToolTip { color: #e8e8e8; background-color: #2f2f2f; border: 1px solid #4f4f4f; }"
        );
    } else {
        app.setPalette(BuildLightPalette());
        app.setStyleSheet(
            baseFontCss +
            "QMenu::separator { height: 1px; background: #d3d3d3; margin: 4px 6px; }"
            "QMenuBar::item:selected, QMenu::item:selected { background-color: #d7e7fb; color: #202020; }"
            "QToolButton:hover { background-color: #e8f1ff; color: #202020; }"
            "QToolBar#TopBar { spacing: 2px; padding: 1px 4px; }"
            "QToolButton#TopBarAction { padding: 2px 6px; margin: 0px; }"
            "QToolButton#TopBarAction::menu-indicator { image: none; width: 0px; }"
            "QToolBar#EditModes { spacing: 2px; padding: 1px 4px; }"
            "QToolTip { color: #202020; background-color: #fffddc; border: 1px solid #cfcfcf; }"
        );
    }
}
} // namespace

void ApplyTheme(QApplication& app) {
    app.setStyle("Fusion");

    const QString fontPath = Engine::Utils::SourcePath::GetResourcePath("Fonts/MontserratMedium.otf");
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId != -1) {
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            QFont font(families.front());
            font.setPixelSize(12);
            app.setFont(font);
            QApplication::setFont(font, "QMenu");
            QApplication::setFont(font, "QMenuBar");
            QApplication::setFont(font, "QToolTip");
        }
    }

    g_themeConnection.Disconnect();
    g_themeConnection = Core::Settings::Changed(
        "Theme",
        [&app](const QVariant& value) {
            ApplyThemeValue(app, value.toString());
        },
        true
    );
}

} // namespace Lvs::Studio::Theme

