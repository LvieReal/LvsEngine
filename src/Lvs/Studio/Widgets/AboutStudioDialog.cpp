#include "Lvs/Studio/Widgets/AboutStudioDialog.hpp"

#include "Lvs/Studio/Configuration.hpp"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

namespace Lvs::Studio::Widgets {

AboutStudioDialog::AboutStudioDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("About Studio");
    resize(420, 240);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    auto* topLayout = new QHBoxLayout();
    auto* logo = new QLabel(this);
    const QString logoPath = Configuration::GetLogoPathPNG();
    if (QFileInfo::exists(logoPath)) {
        const QPixmap pix(logoPath);
        logo->setPixmap(pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logo->setText("LVS");
    }

    auto* titleLayout = new QVBoxLayout();
    auto* title = new QLabel(Configuration::GetFullName(), this);
    // title->setStyleSheet("font-size: 18px; font-weight: bold;"); // pls don't do that
    auto* version = new QLabel(QString("Version: %1").arg(Configuration::GetVersion()), this);
    titleLayout->addWidget(title);
    titleLayout->addWidget(version);
    titleLayout->addStretch();

    topLayout->addWidget(logo);
    topLayout->addLayout(titleLayout);
    mainLayout->addLayout(topLayout);

    mainLayout->addWidget(new QLabel(QString("Created by: %1").arg(Configuration::GetFullCreatorName()), this));
    mainLayout->addWidget(new QLabel(QString("© %1 All rights reserved.").arg(Configuration::GetYear()), this));
    mainLayout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttons);
}

} // namespace Lvs::Studio::Widgets
