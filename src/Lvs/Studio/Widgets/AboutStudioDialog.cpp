#include "Lvs/Studio/Widgets/AboutStudioDialog.hpp"

#include "Lvs/Studio/Configuration.hpp"

#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QGroupBox>
#include <QRegularExpression>
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
    QFont fontBold;
    fontBold.setBold(true);
    fontBold.setPixelSize(fontBold.pixelSize() * 2);
    title->setFont(fontBold);
    title->setAlignment(Qt::AlignLeft);

    auto* version = new QLabel(QString("Version: %1").arg(Configuration::GetVersion()), this);
    version->setAlignment(Qt::AlignLeft);

    titleLayout->addWidget(title);
    titleLayout->addWidget(version);

    titleLayout->addStretch();

    topLayout->addWidget(logo);
    topLayout->addLayout(titleLayout);
    mainLayout->addLayout(topLayout);

    mainLayout->addWidget(new QLabel(QString("Created by: %1").arg(Configuration::GetFullCreatorName()), this));
    mainLayout->addWidget(new QLabel(QString("© %1 All rights reserved.").arg(Configuration::GetYear()), this));
    mainLayout->addStretch();

    const QStringList credits = Configuration::GetCredits();
    if (!credits.isEmpty()) {
        auto* creditsBox = new QGroupBox("Credits", this);
        auto* creditsLayout = new QVBoxLayout(creditsBox);
        creditsLayout->setContentsMargins(10, 8, 10, 8);
        creditsLayout->setSpacing(4);

        for (const auto& line : credits) {
            QString html = line.toHtmlEscaped();
            html.replace(
                QRegularExpression("(https?://\\S+?)([\\)\\]\\}\\.,;:]?)(?=\\s|$)"),
                "<a href=\"\\1\">\\1</a>\\2"
            );
            auto* label = new QLabel(html, creditsBox);
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextBrowserInteraction);
            label->setOpenExternalLinks(true);
            creditsLayout->addWidget(label);
        }

        mainLayout->addWidget(creditsBox);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(buttons);
}

} // namespace Lvs::Studio::Widgets
