#include "Lvs/Studio/Widgets/Properties/PropertyEditorUtils.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Studio/Widgets/Properties/PathEditor.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertyValueUtils.hpp"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>

#include <limits>
#include <memory>

namespace Lvs::Studio::Widgets::Properties::EditorUtils {

namespace {

void SetColorButtonPreview(QPushButton* button, const Lvs::Engine::Math::Color3& value) {
    if (button == nullptr) {
        return;
    }

    const QColor color = ValueUtils::ToQColor(value);
    button->setProperty("color3_value", QVariant::fromValue(value));
    QPalette palette = button->palette();
    palette.setColor(QPalette::Button, color);
    palette.setColor(QPalette::ButtonText, color.lightnessF() < 0.45 ? Qt::white : Qt::black);
    button->setPalette(palette);
    button->setAutoFillBackground(true);
    button->update();
}

std::shared_ptr<Lvs::Engine::Core::Instance> TryGetInstanceReference(
    const QVariant& value,
    const Engine::Core::PropertyDefinition& definition
) {
    if (!definition.IsInstanceReference) {
        return nullptr;
    }
    if (definition.Type.id() == QMetaType::fromType<std::shared_ptr<Lvs::Engine::Core::Instance>>().id()) {
        return value.value<std::shared_ptr<Lvs::Engine::Core::Instance>>();
    }
    if (definition.Type.id() == QMetaType::fromType<std::shared_ptr<Lvs::Engine::Objects::Camera>>().id()) {
        const auto camera = value.value<std::shared_ptr<Lvs::Engine::Objects::Camera>>();
        return std::static_pointer_cast<Lvs::Engine::Core::Instance>(camera);
    }
    return nullptr;
}

} // namespace

QWidget* CreateEditor(
    const QString& propertyName,
    const Engine::Core::PropertyDefinition& definition,
    const QVariant& value,
    QWidget* parent,
    const std::function<void(const QString&, const QVariant&)>& onEdited
) {
    const int typeId = definition.Type.id();

    if (typeId == QMetaType::Bool) {
        auto* editor = new QCheckBox(parent);
        editor->setChecked(value.toBool());
        QObject::connect(editor, &QCheckBox::toggled, editor, [propertyName, onEdited](const bool checked) {
            onEdited(propertyName, checked);
        });
        return editor;
    }
    if (typeId == QMetaType::Int) {
        auto* editor = new QSpinBox(parent);
        editor->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        editor->setKeyboardTracking(false);
        editor->setValue(value.toInt());
        const QPointer<QSpinBox> safeEditor(editor);
        QObject::connect(editor, &QAbstractSpinBox::editingFinished, editor, [propertyName, onEdited, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            onEdited(propertyName, safeEditor->value());
        });
        return editor;
    }
    if (typeId == QMetaType::Double) {
        auto* editor = new QDoubleSpinBox(parent);
        editor->setRange(-1'000'000.0, 1'000'000.0);
        editor->setDecimals(4);
        editor->setSingleStep(0.1);
        editor->setKeyboardTracking(false);
        editor->setValue(value.toDouble());
        const QPointer<QDoubleSpinBox> safeEditor(editor);
        QObject::connect(editor, &QAbstractSpinBox::editingFinished, editor, [propertyName, onEdited, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            onEdited(propertyName, safeEditor->value());
        });
        return editor;
    }

    const QList<ValueUtils::EnumOption> enumOptions = ValueUtils::EnumOptionsForType(typeId);
    if (!enumOptions.isEmpty()) {
        auto* editor = new QComboBox(parent);
        for (const auto& option : enumOptions) {
            editor->addItem(option.Name, option.Value);
        }
        const int currentValue = value.toInt();
        for (int i = 0; i < editor->count(); ++i) {
            if (editor->itemData(i).toInt() == currentValue) {
                editor->setCurrentIndex(i);
                break;
            }
        }
        const QPointer<QComboBox> safeEditor(editor);
        QObject::connect(editor, qOverload<int>(&QComboBox::currentIndexChanged), editor, [propertyName, onEdited, safeEditor, typeId](const int index) {
            if (safeEditor.isNull() || index < 0 || index >= safeEditor->count()) {
                return;
            }
            onEdited(propertyName, ValueUtils::EnumVariantFromTypeAndInt(typeId, safeEditor->itemData(index).toInt()));
        });
        return editor;
    }

    if (typeId == QMetaType::fromType<Lvs::Engine::Math::Color3>().id()) {
        auto* editor = new QPushButton(parent);
        editor->setFixedHeight(20);
        editor->setText("Pick...");
        editor->setProperty("keep_size_policy", true);
        editor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        SetColorButtonPreview(editor, value.value<Lvs::Engine::Math::Color3>());
        const QPointer<QPushButton> safeEditor(editor);
        QObject::connect(editor, &QPushButton::clicked, editor, [propertyName, onEdited, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            const auto current = safeEditor->property("color3_value").value<Lvs::Engine::Math::Color3>();
            const QColor selected = QColorDialog::getColor(ValueUtils::ToQColor(current), safeEditor, "Select Color");
            if (!selected.isValid()) {
                return;
            }
            const auto next = ValueUtils::FromQColor(selected);
            SetColorButtonPreview(safeEditor, next);
            onEdited(propertyName, QVariant::fromValue(next));
        });
        return editor;
    }

    if (typeId == QMetaType::QString && definition.CustomTags.contains("IsPath")) {
        return new PathEditor(
            value.toString(),
            definition.CustomAttributes,
            [propertyName, onEdited](const QString& nextPath) {
                onEdited(propertyName, nextPath);
            },
            parent
        );
    }

    auto* editor = new QLineEdit(parent);
    if (typeId == QMetaType::fromType<Lvs::Engine::Math::Vector3>().id()) {
        editor->setText(ValueUtils::FormatVector3(value.value<Lvs::Engine::Math::Vector3>()));
        const QPointer<QLineEdit> safeEditor(editor);
        QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, onEdited, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            Lvs::Engine::Math::Vector3 parsed;
            if (!ValueUtils::TryParseVector3(safeEditor->text(), parsed)) {
                return;
            }
            onEdited(propertyName, QVariant::fromValue(parsed));
        });
        return editor;
    }
    if (typeId == QMetaType::fromType<Lvs::Engine::Math::CFrame>().id()) {
        editor->setText(ValueUtils::FormatCFrame(value.value<Lvs::Engine::Math::CFrame>()));
        const QPointer<QLineEdit> safeEditor(editor);
        QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, onEdited, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            Lvs::Engine::Math::CFrame parsed;
            if (!ValueUtils::TryParseCFrame(safeEditor->text(), parsed)) {
                return;
            }
            onEdited(propertyName, QVariant::fromValue(parsed));
        });
        return editor;
    }

    if (definition.IsInstanceReference) {
        if (const auto target = TryGetInstanceReference(value, definition); target != nullptr) {
            editor->setText(target->GetFullPath());
        } else {
            editor->setText({});
        }
        editor->setReadOnly(true);
        return editor;
    }

    editor->setText(value.toString());
    const QPointer<QLineEdit> safeEditor(editor);
    QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, onEdited, safeEditor]() {
        if (safeEditor.isNull()) {
            return;
        }
        onEdited(propertyName, safeEditor->text());
    });
    return editor;
}

void SetEditorValue(
    QWidget* editor,
    const QVariant& value,
    const Engine::Core::PropertyDefinition& definition
) {
    if (auto* check = qobject_cast<QCheckBox*>(editor); check != nullptr) {
        const QSignalBlocker blocker(check);
        check->setChecked(value.toBool());
        return;
    }
    if (auto* spin = qobject_cast<QSpinBox*>(editor); spin != nullptr) {
        const QSignalBlocker blocker(spin);
        spin->setValue(value.toInt());
        return;
    }
    if (auto* doubleSpin = qobject_cast<QDoubleSpinBox*>(editor); doubleSpin != nullptr) {
        const QSignalBlocker blocker(doubleSpin);
        doubleSpin->setValue(value.toDouble());
        return;
    }
    if (auto* combo = qobject_cast<QComboBox*>(editor); combo != nullptr) {
        const QSignalBlocker blocker(combo);
        const int intValue = value.toInt();
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == intValue) {
                combo->setCurrentIndex(i);
                return;
            }
        }
        return;
    }
    if (auto* pathEditor = dynamic_cast<PathEditor*>(editor); pathEditor != nullptr) {
        pathEditor->setPath(value.toString());
        return;
    }
    if (auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
        const QSignalBlocker blocker(line);
        if (definition.IsInstanceReference) {
            if (const auto target = TryGetInstanceReference(value, definition); target != nullptr) {
                line->setText(target->GetFullPath());
            } else {
                line->setText({});
            }
            return;
        }
        if (definition.Type.id() == QMetaType::fromType<Lvs::Engine::Math::Vector3>().id()) {
            line->setText(ValueUtils::FormatVector3(value.value<Lvs::Engine::Math::Vector3>()));
            return;
        }
        if (definition.Type.id() == QMetaType::fromType<Lvs::Engine::Math::CFrame>().id()) {
            line->setText(ValueUtils::FormatCFrame(value.value<Lvs::Engine::Math::CFrame>()));
            return;
        }
        if (definition.Type.id() == QMetaType::fromType<Lvs::Engine::Math::Color3>().id()) {
            line->setText(ValueUtils::FormatColor3(value.value<Lvs::Engine::Math::Color3>()));
            return;
        }
        line->setText(value.toString());
        return;
    }
    if (auto* button = qobject_cast<QPushButton*>(editor); button != nullptr) {
        if (definition.Type.id() == QMetaType::fromType<Lvs::Engine::Math::Color3>().id()) {
            SetColorButtonPreview(button, value.value<Lvs::Engine::Math::Color3>());
        }
    }
}

} // namespace Lvs::Studio::Widgets::Properties::EditorUtils
