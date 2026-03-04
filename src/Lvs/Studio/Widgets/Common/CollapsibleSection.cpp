#include "Lvs/Studio/Widgets/Common/CollapsibleSection.hpp"

#include <QToolButton>
#include <QVBoxLayout>
#include <QSizePolicy>

namespace Lvs::Studio::Widgets::Common {

CollapsibleSection::CollapsibleSection(
    const QString& title,
    QWidget* content,
    const bool startCollapsed,
    QWidget* parent
)
    : QWidget(parent),
      content_(content) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    toggleButton_ = new QToolButton(this);
    toggleButton_->setText(title);
    toggleButton_->setCheckable(true);
    toggleButton_->setChecked(!startCollapsed);
    toggleButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleButton_->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);

    layout->addWidget(toggleButton_);

    if (content_ != nullptr) {
        layout->addWidget(content_);
    }

    const QPointer<QToolButton> safeToggle = toggleButton_;
    const QPointer<QWidget> safeContent = content_;
    QObject::connect(toggleButton_, &QToolButton::toggled, toggleButton_, [safeToggle, safeContent](const bool expanded) {
        if (safeToggle != nullptr) {
            safeToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        }
        if (safeContent != nullptr) {
            safeContent->setVisible(expanded);
        }
    });
    SetCollapsed(startCollapsed);
}

void CollapsibleSection::SetCollapsed(const bool collapsed) {
    if (toggleButton_ == nullptr) {
        return;
    }
    toggleButton_->setArrowType(collapsed ? Qt::RightArrow : Qt::DownArrow);
    if (content_ != nullptr) {
        content_->setVisible(!collapsed);
    }
}

} // namespace Lvs::Studio::Widgets::Common
