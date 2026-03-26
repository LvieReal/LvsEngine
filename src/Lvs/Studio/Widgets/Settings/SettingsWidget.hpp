#pragma once

#include <QDialog>

class QFormLayout;
class QLineEdit;
class QListWidget;
class QWidget;

namespace Lvs::Studio::Widgets::Settings {

class SettingsWidget final : public QDialog {
public:
    explicit SettingsWidget(QWidget* parent = nullptr);

private:
    void BuildUi();
    void ClearForm() const;
    void ReloadCategory();
    QWidget* CreateSettingRow(const QString& key);
    QWidget* CreateEditor(const QString& key);

    QLineEdit* search_{nullptr};
    QListWidget* categoryList_{nullptr};
    QWidget* settingsPanel_{nullptr};
    QFormLayout* form_{nullptr};
};

} // namespace Lvs::Studio::Widgets::Settings
