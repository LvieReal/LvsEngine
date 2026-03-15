#include "Lvs/Studio/Widgets/Settings/SettingsWidget.hpp"

#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

#include <memory>

namespace Lvs::Studio::Widgets::Settings {

namespace {

void UpdateEditorValue(QWidget* editor, const QString& key, const QVariant& value) {
    if (auto* checkBox = qobject_cast<QCheckBox*>(editor); checkBox != nullptr) {
        const QSignalBlocker blocker(checkBox);
        checkBox->setChecked(value.toBool());
        return;
    }
    if (auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
        const QSignalBlocker blocker(line);
        line->setText(value.toString());
        return;
    }
    if (auto* combo = qobject_cast<QComboBox*>(editor); combo != nullptr) {
        const QSignalBlocker blocker(combo);
        static_cast<void>(key);
        if (Lvs::Engine::Enums::Metadata::IsRegisteredEnumType(value.typeId())) {
            const int currentValue = Lvs::Engine::Enums::Metadata::IntFromVariant(value);
            for (int i = 0; i < combo->count(); ++i) {
                if (combo->itemData(i).toInt() == currentValue) {
                    combo->setCurrentIndex(i);
                    return;
                }
            }
        }
        combo->setCurrentText(value.toString());
    }
}

} // namespace

SettingsWidget::SettingsWidget(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Studio Settings");
    resize(800, 600);
    BuildUi();
}

void SettingsWidget::BuildUi() {
    auto* layout = new QVBoxLayout(this);

    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search settings...");
    layout->addWidget(search_);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    categoryList_ = new QListWidget(splitter);
    settingsPanel_ = new QWidget(splitter);
    settingsPanel_->setObjectName("SettingsPanel");
    form_ = new QFormLayout(settingsPanel_);
    form_->setContentsMargins(8, 6, 8, 6);
    form_->setSpacing(6);

    splitter->addWidget(categoryList_);
    splitter->addWidget(settingsPanel_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    layout->addWidget(splitter);

    auto* resetAllBtn = new QPushButton("Reset All to Default", this);
    layout->addWidget(resetAllBtn);

    const QStringList orderedCategories = Core::Settings::CategoryOrder();
    if (!orderedCategories.isEmpty()) {
        for (const QString& category : orderedCategories) {
            if (Core::Settings::Categories().contains(category)) {
                categoryList_->addItem(category);
            }
        }
    } else {
        for (auto it = Core::Settings::Categories().cbegin(); it != Core::Settings::Categories().cend(); ++it) {
            categoryList_->addItem(it.key());
        }
    }

    connect(search_, &QLineEdit::textChanged, this, [this]() { ReloadCategory(); });
    connect(categoryList_, &QListWidget::currentTextChanged, this, [this](const QString&) { ReloadCategory(); });
    connect(resetAllBtn, &QPushButton::clicked, this, [this]() {
        const auto answer = QMessageBox::question(
            this,
            "Reset Settings",
            "Reset ALL settings to default values?",
            QMessageBox::Yes | QMessageBox::No
        );
        if (answer == QMessageBox::Yes) {
            Core::Settings::ResetAll();
            ReloadCategory();
        }
    });

    if (categoryList_->count() > 0) {
        categoryList_->setCurrentRow(0);
    }
}

void SettingsWidget::ClearForm() const {
    while (form_->rowCount() > 0) {
        form_->removeRow(0);
    }
}

void SettingsWidget::ReloadCategory() {
    ClearForm();

    const auto* currentItem = categoryList_->currentItem();
    if (currentItem == nullptr) {
        return;
    }

    const QString category = currentItem->text();
    const QStringList keys = Core::Settings::Categories().value(category);
    const QString searchText = search_->text().trimmed().toLower();

    for (const QString& key : keys) {
        const auto& meta = Core::Settings::All().value(key);
        const QString label = meta.Name.isEmpty() ? key : meta.Name;
        if (!searchText.isEmpty() && !label.toLower().contains(searchText)) {
            continue;
        }
        form_->addRow(label, CreateSettingRow(key, label, meta.Description));
    }
}

QWidget* SettingsWidget::CreateSettingRow(const QString& key, const QString& label, const QString& description) {
    auto* container = new QWidget(settingsPanel_);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    QWidget* editor = CreateEditor(key);
    editor->setToolTip(description.isEmpty() ? label : description);
    layout->addWidget(editor);

    auto* resetBtn = new QPushButton("↺", container);
    resetBtn->setFixedWidth(28);
    resetBtn->setToolTip("Reset to default");
    connect(resetBtn, &QPushButton::clicked, container, [this, key]() {
        Core::Settings::Reset(key);
    });

    const auto updateResetVisibility = [key, resetBtn](const QVariant& value) {
        const bool isDefault = (value == Core::Settings::GetDefault(key));
        resetBtn->setVisible(!isDefault);
    };
    auto settingUiConnection = std::make_shared<Core::Settings::Connection>(
        Core::Settings::Changed(key, [key, editor, updateResetVisibility](const QVariant& value) {
            updateResetVisibility(value);
            UpdateEditorValue(editor, key, value);
        }, true)
    );
    connect(container, &QObject::destroyed, container, [settingUiConnection]() {
        settingUiConnection->Disconnect();
    });

    layout->addWidget(resetBtn);

    return container;
}

QWidget* SettingsWidget::CreateEditor(const QString& key) {
    const auto& meta = Core::Settings::All().value(key);
    const QVariant defaultValue = Core::Settings::GetDefault(key);
    const QVariant value = Core::Settings::Get(key);

    if (defaultValue.typeId() == QMetaType::Bool) {
        auto* checkBox = new QCheckBox(settingsPanel_);
        checkBox->setChecked(value.toBool());
        connect(checkBox, &QCheckBox::checkStateChanged, checkBox, [key](Qt::CheckState state) {
            Core::Settings::Set(key, state == Qt::Checked);
        });
        return checkBox;
    }

    if (defaultValue.typeId() == QMetaType::Double || defaultValue.typeId() == QMetaType::Int) {
        auto* line = new QLineEdit(value.toString(), settingsPanel_);
        connect(line, &QLineEdit::editingFinished, line, [line, key, defaultValue]() {
            bool ok = false;
            if (defaultValue.typeId() == QMetaType::Double) {
                const double parsed = line->text().toDouble(&ok);
                if (ok) {
                    Core::Settings::Set(key, parsed);
                }
            } else {
                const int parsed = line->text().toInt(&ok);
                if (ok) {
                    Core::Settings::Set(key, parsed);
                }
            }
            if (!ok) {
                line->setText(Core::Settings::Get(key).toString());
            }
        });
        return line;
    }

    if (key == "ExplorerIconPack") {
        auto* combo = new QComboBox(settingsPanel_);
        combo->setEditable(true);
        const QStringList packs = Core::GetIconPackManager().GetAvailablePacks();
        for (const QString& pack : packs) {
            combo->addItem(pack);
        }
        combo->setCurrentText(value.toString());
        connect(combo, &QComboBox::currentTextChanged, combo, [key](const QString& current) {
            Core::Settings::Set(key, current);
        });
        return combo;
    }

    if (Engine::Enums::Metadata::IsRegisteredEnumType(defaultValue.typeId())) {
        auto* combo = new QComboBox(settingsPanel_);
        combo->setEditable(false);

        const int typeId = defaultValue.typeId();
        const auto options = Engine::Enums::Metadata::OptionsForType(typeId);
        for (const auto& opt : options) {
            combo->addItem(QString::fromUtf8(opt.Name), opt.Value);
        }

        QVariant current = value;
        if (current.typeId() != typeId) {
            current = Engine::Enums::Metadata::CoerceVariant(typeId, current);
        }
        if (!current.isValid()) {
            current = defaultValue;
        }
        const int currentValue = Engine::Enums::Metadata::IntFromVariant(current);
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == currentValue) {
                combo->setCurrentIndex(i);
                break;
            }
        }

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), combo, [combo, key, typeId](const int index) {
            if (index < 0) {
                return;
            }
            const int selectedValue = combo->itemData(index).toInt();
            Core::Settings::Set(key, Engine::Enums::Metadata::VariantFromInt(typeId, selectedValue));
        });

        return combo;
    }

    if (defaultValue.typeId() == QMetaType::QString && !meta.Options.isEmpty()) {
        auto* combo = new QComboBox(settingsPanel_);
        combo->setEditable(false);
        combo->addItems(meta.Options);
        combo->setCurrentText(value.toString());
        connect(combo, &QComboBox::currentTextChanged, combo, [key](const QString& current) {
            Core::Settings::Set(key, current);
        });
        return combo;
    }

    auto* line = new QLineEdit(value.toString(), settingsPanel_);
    connect(line, &QLineEdit::editingFinished, line, [line, key]() {
        Core::Settings::Set(key, line->text());
    });
    return line;
}

} // namespace Lvs::Studio::Widgets::Settings
