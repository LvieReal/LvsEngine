#pragma once

#include <QVariant>
#include <QWidget>

#include <functional>

class QLineEdit;
class QPushButton;

namespace Lvs::Studio::Widgets::Properties {

class PathEditor final : public QWidget {
public:
    PathEditor(
        const QString& value,
        const QVariantMap& customAttributes,
        std::function<void(const QString&)> onChanged,
        QWidget* parent = nullptr
    );
    ~PathEditor() override = default;

    [[nodiscard]] QString path() const;
    void setPath(const QString& value) const;

private:
    [[nodiscard]] QString BuildFileFilter() const;
    static QString AsVirtualPath(const QString& path);
    static QString AsOsPath(const QString& path);
    void OnBrowse();

    QLineEdit* lineEdit_{nullptr};
    QPushButton* browseButton_{nullptr};
    QVariantMap customAttributes_{};
    std::function<void(const QString&)> onChanged_{};
};

} // namespace Lvs::Studio::Widgets::Properties

