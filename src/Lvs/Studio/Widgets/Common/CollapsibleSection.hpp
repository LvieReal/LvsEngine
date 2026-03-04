#pragma once

#include <QPointer>
#include <QWidget>

class QToolButton;

namespace Lvs::Studio::Widgets::Common {

class CollapsibleSection final : public QWidget {
public:
    explicit CollapsibleSection(const QString& title, QWidget* content, bool startCollapsed = false, QWidget* parent = nullptr);
    ~CollapsibleSection() override = default;

private:
    void SetCollapsed(bool collapsed);

    QPointer<QToolButton> toggleButton_;
    QPointer<QWidget> content_;
};

} // namespace Lvs::Studio::Widgets::Common
