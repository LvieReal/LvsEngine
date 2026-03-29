#include "Lvs/Studio/Core/Cursor.hpp"

#include "Lvs/Qt/QtBridge.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <QCursor>
#include <QPixmap>
#include <QWidget>
#include <Qt>

namespace Lvs::Engine::Core::Cursor {

void SetCustomCursor(QWidget* widget) {
    if (widget == nullptr) {
        return;
    }

    const QString cursorPath = QtBridge::ToQString(Utils::SourcePath::GetResourcePath("Cursor/cursor.png"));
    const QPixmap cursorPixmap(cursorPath);
    if (cursorPixmap.isNull()) {
        widget->setCursor(Qt::ArrowCursor);
        return;
    }

    widget->setCursor(QCursor(cursorPixmap));
}

} // namespace Lvs::Engine::Core::Cursor
