#include "Lvs/Studio/Theme.hpp"

#include "Lvs/Engine/Utils/SourcePath.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QApplication>
#include <QBrush>
#include <QFont>
#include <QFontDatabase>
#include <QPalette>
#include <QColor>
#include <QStyleHints>

namespace Lvs::Studio::Theme {

namespace {
Core::Settings::Connection g_themeConnection;

void ApplyThemeValue(const QString& theme)
{
    QGuiApplication::styleHints()->setColorScheme(theme.compare("Dark", Qt::CaseInsensitive) == 0 ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
}
} // namespace

void ApplyTheme(QApplication& app) {
    app.setStyle("Fusion");

    const QString fontPath = Engine::Utils::SourcePath::GetResourcePath("Fonts/MontserratMedium.otf");
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId != -1)
    {
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
        {
            QFont font(families.front());
            font.setPixelSize(12);
            app.setFont(font);
        }
    }

    g_themeConnection.Disconnect();
    g_themeConnection = Core::Settings::Changed(
        "Theme",
        [&app](const QVariant& value) {
            ApplyThemeValue(value.toString());
        },
        true
    );
}

} // namespace Lvs::Studio::Theme
