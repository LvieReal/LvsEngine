#include "Lvs/Studio/Core/PlaceFileUtils.hpp"

#include <QFileInfo>

namespace Lvs::Studio::Core::PlaceFileUtils {

QString Extension() {
    return "lvsx";
}

QString DefaultUntitledFileName() {
    return QString("untitled.%1").arg(Extension());
}

QString FileDialogFilter() {
    return QString("Lvs Place Files (*.%1);;XML Files (*.xml);;All Files (*)").arg(Extension());
}

QString EnsureExtension(QString path) {
    QFileInfo info(path);
    if (info.suffix().compare(Extension(), Qt::CaseInsensitive) != 0) {
        path += QString(".%1").arg(Extension());
    }
    return path;
}

} // namespace Lvs::Studio::Core::PlaceFileUtils

