#include "Lvs/Studio/Widgets/Properties/PropertyEditorUtils.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/QtBridge.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Studio/Core/QtMetaTypes.hpp"
#include "Lvs/Studio/Widgets/Properties/PathEditor.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertyValueUtils.hpp"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
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

class CFrameEditor final : public QWidget {
public:
    CFrameEditor(
        const QString& propertyName,
        const Engine::Core::PropertyDefinition& definition,
        const std::function<void(const QString&, const QVariant&)>& onEdited,
        QWidget* parent
    )
        : QWidget(parent),
          propertyName_(propertyName),
          onEdited_(onEdited) {
        auto* layout = new QGridLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setHorizontalSpacing(6);
        layout->setVerticalSpacing(4);
        layout->setColumnStretch(1, 1);

        auto* positionLabel = new QLabel("Position", this);
        positionEdit_ = new QLineEdit(this);
        positionEdit_->setPlaceholderText("x, y, z");
        auto* rotationLabel = new QLabel("Rotation", this);
        rotationEdit_ = new QLineEdit(this);
        rotationEdit_->setPlaceholderText("x, y, z");

        layout->addWidget(positionLabel, 0, 0);
        layout->addWidget(positionEdit_, 0, 1);
        layout->addWidget(rotationLabel, 1, 0);
        layout->addWidget(rotationEdit_, 1, 1);

        if (definition.ReadOnly) {
            positionEdit_->setReadOnly(true);
            rotationEdit_->setReadOnly(true);
        }

        const QPointer<CFrameEditor> safeEditor(this);
        auto commit = [safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            safeEditor->CommitIfValid();
        };
        QObject::connect(positionEdit_, &QLineEdit::editingFinished, this, commit);
        QObject::connect(rotationEdit_, &QLineEdit::editingFinished, this, commit);
    }

    void SetValue(const Lvs::Engine::Math::CFrame& value) {
        const QSignalBlocker blockerPos(positionEdit_);
        const QSignalBlocker blockerRot(rotationEdit_);
        positionEdit_->setText(ValueUtils::FormatVector3(value.Position));
        rotationEdit_->setText(ValueUtils::FormatVector3(value.ToEulerXYZ()));
    }

private:
    void CommitIfValid() {
        Lvs::Engine::Math::Vector3 position;
        Lvs::Engine::Math::Vector3 rotation;
        if (!ValueUtils::TryParseVector3(positionEdit_->text(), position)) {
            return;
        }
        if (!ValueUtils::TryParseVector3(rotationEdit_->text(), rotation)) {
            return;
        }
        onEdited_(propertyName_, QVariant::fromValue(Lvs::Engine::Math::CFrame::FromPositionRotation(position, rotation)));
    }

    QString propertyName_;
    std::function<void(const QString&, const QVariant&)> onEdited_;
    QLineEdit* positionEdit_{nullptr};
    QLineEdit* rotationEdit_{nullptr};
};

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
    if (value.canConvert<std::shared_ptr<Lvs::Engine::Core::Instance>>()) {
        return value.value<std::shared_ptr<Lvs::Engine::Core::Instance>>();
    }
    return nullptr;
}

bool HasCustomTag(const Engine::Core::StringList& tags, const char* tag) {
    for (const auto& entry : tags) {
        if (entry == tag) {
            return true;
        }
    }
    return false;
}

QVariantMap ToQVariantMap(const Engine::Core::HashMap<Engine::Core::String, Engine::Core::Variant>& attributes) {
    QVariantMap out;
    for (const auto& [key, val] : attributes) {
        out.insert(QString::fromUtf8(key.c_str()), Engine::Core::QtBridge::ToQVariant(val));
    }
    return out;
}

QString EnumTypeName(const Engine::Core::PropertyDefinition& definition) {
    const auto it = definition.CustomAttributes.find("EnumType");
    if (it == definition.CustomAttributes.end()) {
        return {};
    }
    return Engine::Core::QtBridge::ToQString(it->second.toString());
}

} // namespace

QWidget* CreateEditor(
    const QString& propertyName,
    const Engine::Core::PropertyDefinition& definition,
    const QVariant& value,
    QWidget* parent,
    const std::function<void(const QString&, const QVariant&)>& onEdited
) {
    const Engine::Core::TypeId typeId = definition.Type;

    if (typeId == Engine::Core::TypeId::Bool) {
        auto* editor = new QCheckBox(parent);
        editor->setChecked(value.toBool());
        QObject::connect(editor, &QCheckBox::toggled, editor, [propertyName, onEdited](const bool checked) {
            onEdited(propertyName, checked);
        });
        return editor;
    }
    if (typeId == Engine::Core::TypeId::Enum) {
        const QString enumType = EnumTypeName(definition);
        const QList<ValueUtils::EnumOption> options = !enumType.isEmpty()
            ? ValueUtils::EnumOptionsForEnum(enumType)
            : QList<ValueUtils::EnumOption>{};

        if (!options.isEmpty()) {
            auto* editor = new QComboBox(parent);
            editor->setEditable(false);
            for (const auto& opt : options) {
                editor->addItem(opt.Name, opt.Value);
            }

            const int currentValue = value.toInt();
            for (int i = 0; i < editor->count(); ++i) {
                if (editor->itemData(i).toInt() == currentValue) {
                    editor->setCurrentIndex(i);
                    break;
                }
            }

            const QPointer<QComboBox> safeEditor(editor);
            QObject::connect(
                editor,
                qOverload<int>(&QComboBox::currentIndexChanged),
                editor,
                [propertyName, onEdited, safeEditor](const int index) {
                    if (safeEditor.isNull() || index < 0 || index >= safeEditor->count()) {
                        return;
                    }
                    onEdited(propertyName, safeEditor->itemData(index).toInt());
                }
            );
            return editor;
        }
    }
    if (typeId == Engine::Core::TypeId::Int || typeId == Engine::Core::TypeId::Enum) {
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
    if (typeId == Engine::Core::TypeId::Double) {
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

    if (typeId == Engine::Core::TypeId::Color3) {
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

    if (typeId == Engine::Core::TypeId::String && HasCustomTag(definition.CustomTags, "IsPath")) {
        return new PathEditor(
            value.toString(),
            ToQVariantMap(definition.CustomAttributes),
            [propertyName, onEdited](const QString& nextPath) {
                onEdited(propertyName, nextPath);
            },
            parent
        );
    }

    if (typeId == Engine::Core::TypeId::CFrame) {
        auto* cframeEditor = new CFrameEditor(propertyName, definition, onEdited, parent);
        cframeEditor->SetValue(value.value<Lvs::Engine::Math::CFrame>());
        return cframeEditor;
    }

    auto* editor = new QLineEdit(parent);
    if (typeId == Engine::Core::TypeId::Vector3) {
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
    if (definition.IsInstanceReference) {
        if (const auto target = TryGetInstanceReference(value, definition); target != nullptr) {
            editor->setText(Engine::Core::QtBridge::ToQString(target->GetFullPath()));
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
    if (auto* cframeEditor = dynamic_cast<CFrameEditor*>(editor); cframeEditor != nullptr) {
        cframeEditor->SetValue(value.value<Lvs::Engine::Math::CFrame>());
        return;
    }
    if (auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
        const QSignalBlocker blocker(line);
        if (definition.IsInstanceReference) {
            if (const auto target = TryGetInstanceReference(value, definition); target != nullptr) {
                line->setText(Engine::Core::QtBridge::ToQString(target->GetFullPath()));
            } else {
                line->setText({});
            }
            return;
        }
        if (definition.Type == Engine::Core::TypeId::Vector3) {
            line->setText(ValueUtils::FormatVector3(value.value<Lvs::Engine::Math::Vector3>()));
            return;
        }
        if (definition.Type == Engine::Core::TypeId::CFrame) {
            line->setText(ValueUtils::FormatCFrame(value.value<Lvs::Engine::Math::CFrame>()));
            return;
        }
        if (definition.Type == Engine::Core::TypeId::Color3) {
            line->setText(ValueUtils::FormatColor3(value.value<Lvs::Engine::Math::Color3>()));
            return;
        }
        line->setText(value.toString());
        return;
    }
    if (auto* button = qobject_cast<QPushButton*>(editor); button != nullptr) {
        if (definition.Type == Engine::Core::TypeId::Color3) {
            SetColorButtonPreview(button, value.value<Lvs::Engine::Math::Color3>());
        }
    }
}

} // namespace Lvs::Studio::Widgets::Properties::EditorUtils
