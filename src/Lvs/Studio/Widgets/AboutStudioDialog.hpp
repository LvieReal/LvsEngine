#pragma once

#include <QDialog>

namespace Lvs::Studio::Widgets {

class AboutStudioDialog final : public QDialog {
public:
    explicit AboutStudioDialog(QWidget* parent = nullptr);
};

} // namespace Lvs::Studio::Widgets
