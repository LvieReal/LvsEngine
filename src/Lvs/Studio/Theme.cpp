#include "Lvs/Studio/Theme.hpp"

#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Engine/Enums/Theme.hpp"
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

void ApplyThemeValue(const Engine::Enums::Theme theme)
{
    if (theme == Engine::Enums::Theme::Auto) {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
        return;
    }

    QGuiApplication::styleHints()->setColorScheme(theme == Engine::Enums::Theme::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
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
            static_cast<void>(app);
            const int typeId = QMetaType::fromType<Engine::Enums::Theme>().id();
            QVariant coerced = Engine::Enums::Metadata::CoerceVariant(typeId, value);
            if (!coerced.isValid()) {
                coerced = QVariant::fromValue(Engine::Enums::Theme::Auto);
            }
            ApplyThemeValue(coerced.value<Engine::Enums::Theme>());
        },
        true
    );
}

} // namespace Lvs::Studio::Theme
