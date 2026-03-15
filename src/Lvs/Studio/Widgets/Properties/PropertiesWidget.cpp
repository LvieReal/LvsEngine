#include "Lvs/Studio/Widgets/Properties/PropertiesWidget.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/Property.hpp"
#include "Lvs/Engine/Core/PropertyTags.hpp"
#include "Lvs/Engine/Core/RegularError.hpp"
#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Studio/Core/IconPackManager.hpp"
#include "Lvs/Studio/Widgets/Properties/PathEditor.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertyEditorUtils.hpp"
#include "Lvs/Studio/Widgets/Properties/PropertyValueUtils.hpp"
#include "Lvs/Studio/Widgets/Common/CollapsibleSection.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleValidator>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QIntValidator>
#include <QLineEdit>
#include <QPointer>
#include <QPixmap>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

namespace Lvs::Studio::Widgets::Properties {

namespace {
constexpr int COLUMN_WIDTH = 100;

} // namespace

PropertiesWidget::PropertiesWidget(const std::shared_ptr<Engine::DataModel::Place>& place, QWidget* parent)
    : QWidget(parent) {
    if (place != nullptr) {
        historyService_ = std::dynamic_pointer_cast<Engine::DataModel::ChangeHistoryService>(
            place->FindService("ChangeHistoryService")
        );
    }

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setBackgroundRole(QPalette::Base);
    if (scroll_->viewport() != nullptr) {
        scroll_->viewport()->setAutoFillBackground(true);
        scroll_->viewport()->setBackgroundRole(QPalette::Base);
    }

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(scroll_);

    ResetContentRoot();

    iconPackConnection_ = std::make_shared<Core::Settings::Connection>(
        Core::Settings::Changed("StudioIconPack", [this](const QVariant&) {
            if (instance_ == nullptr || headerIconLabel_ == nullptr) {
                return;
            }
            headerIconLabel_->setPixmap(Core::GetIconPackManager().GetPixmapForInstance(instance_));
        })
    );
}

PropertiesWidget::~PropertiesWidget() {
    if (iconPackConnection_ != nullptr) {
        iconPackConnection_->Disconnect();
        iconPackConnection_.reset();
    }
    isBinding_ = false;
    clearQueued_ = false;
    bindQueued_ = false;
    queuedInstances_.clear();
    ClearInternal(false);
}

void PropertiesWidget::Clear() {
    if (isBinding_) {
        clearQueued_ = true;
        bindQueued_ = false;
        queuedInstances_.clear();
        return;
    }
    ClearInternal();
}

void PropertiesWidget::ClearInternal(const bool resetContentRoot) {
    if (propertyChangedConnection_.has_value()) {
        propertyChangedConnection_->Disconnect();
        propertyChangedConnection_.reset();
    }

    instance_.reset();
    instances_.clear();
    editors_.clear();
    editorDefinitions_.clear();
    visibilityDependencies_.clear();
    headerIconLabel_ = nullptr;
    headerTitleLabel_ = nullptr;

    if (resetContentRoot) {
        ResetContentRoot();
    }
}

void PropertiesWidget::ProcessQueuedBinding() {
    if (clearQueued_) {
        clearQueued_ = false;
        Clear();
        return;
    }
    if (bindQueued_) {
        auto next = queuedInstances_;
        bindQueued_ = false;
        queuedInstances_.clear();
        BindInstances(next);
    }
}

void PropertiesWidget::BindInstance(const std::shared_ptr<Engine::Core::Instance>& instance) {
    if (instance == nullptr) {
        BindInstances({});
        return;
    }
    BindInstances({instance});
}

void PropertiesWidget::BindInstances(const std::vector<std::shared_ptr<Engine::Core::Instance>>& instances) {
    if (isBinding_) {
        bindQueued_ = true;
        clearQueued_ = false;
        queuedInstances_ = instances;
        return;
    }

    isBinding_ = true;
    ClearInternal();

    instances_.clear();
    instances_.reserve(instances.size());
    std::unordered_set<const Engine::Core::Instance*> seen;
    for (const auto& inst : instances) {
        if (inst == nullptr) {
            continue;
        }
        if (!seen.insert(inst.get()).second) {
            continue;
        }
        instances_.push_back(inst);
    }

    if (instances_.empty()) {
        isBinding_ = false;
        ProcessQueuedBinding();
        return;
    }

    instance_ = instances_.front();

    struct OrderedProperty {
        QString Name;
        QString Category;
        int RegistrationOrder;
    };
    std::vector<OrderedProperty> orderedProperties;
    orderedProperties.reserve(static_cast<std::size_t>(instance_->GetClassDescriptor().PropertyDefinitions().size()));
    for (auto it = instance_->GetClassDescriptor().PropertyDefinitions().cbegin();
         it != instance_->GetClassDescriptor().PropertyDefinitions().cend();
         ++it) {
        const QString propertyName = it.key();
        const auto primaryDef = it.value();

        bool common = true;
        for (const auto& otherInstance : instances_) {
            const auto& defs = otherInstance->GetClassDescriptor().PropertyDefinitions();
            if (!defs.contains(propertyName)) {
                common = false;
                break;
            }
            const auto otherDef = defs.value(propertyName);
            if (otherDef.Type.id() != primaryDef.Type.id() || otherDef.IsInstanceReference != primaryDef.IsInstanceReference) {
                common = false;
                break;
            }
            if (!ShouldShowProperty(otherInstance, otherDef)) {
                common = false;
                break;
            }
        }
        if (!common) {
            continue;
        }

        orderedProperties.push_back({propertyName, primaryDef.Category, primaryDef.RegistrationOrder});
    }
    std::sort(
        orderedProperties.begin(),
        orderedProperties.end(),
        [](const OrderedProperty& left, const OrderedProperty& right) {
            if (left.RegistrationOrder != right.RegistrationOrder) {
                return left.RegistrationOrder < right.RegistrationOrder;
            }
            return left.Name < right.Name;
        }
    );

    QHash<QString, QList<QString>> categoryProperties;
    QStringList categoryOrder;
    for (const auto& ordered : orderedProperties) {
        const auto& property = instance_->GetPropertyObject(ordered.Name);
        const auto& definition = property.Definition();
        if (instances_.size() == 1) {
            for (const auto& tag : definition.CustomTags) {
                const auto parsed = Engine::Core::PropertyTags::ParseVisibleIfTag(tag);
                if (parsed.has_value()) {
                    visibilityDependencies_.insert(parsed->first);
                }
            }
        }

        if (!categoryProperties.contains(definition.Category)) {
            categoryProperties.insert(definition.Category, {});
            categoryOrder.push_back(definition.Category);
        }
        categoryProperties[definition.Category].push_back(ordered.Name);
    }

    for (const auto& category : categoryOrder) {
        auto* contentWidget = new QWidget();
        QGridLayout* grid = nullptr;
        if (category == "Data") {
            auto* contentLayout = new QVBoxLayout(contentWidget);
            contentLayout->setContentsMargins(5, 5, 5, 5);
            contentLayout->setSpacing(6);
            contentLayout->addWidget(
                instances_.size() == 1 ? CreateInstanceHeader(instance_, contentWidget)
                                       : CreateMultiInstanceHeader(static_cast<int>(instances_.size()), contentWidget)
            );
            grid = new QGridLayout();
            grid->setContentsMargins(0, 0, 0, 0);
            contentLayout->addLayout(grid);
        } else {
            grid = new QGridLayout(contentWidget);
            grid->setContentsMargins(5, 5, 5, 5);
        }
        grid->setColumnMinimumWidth(0, COLUMN_WIDTH);
        grid->setColumnMinimumWidth(1, COLUMN_WIDTH);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);

        for (const auto& propertyName : categoryProperties.value(category)) {
            const auto& property = instance_->GetPropertyObject(propertyName);
            const auto& definition = property.Definition();
            QVariant primaryValue;
            try {
                primaryValue = property.Get();
            } catch (...) {
                continue;
            }

            bool mixed = false;
            for (std::size_t i = 1; i < instances_.size(); ++i) {
                QVariant otherValue;
                try {
                    otherValue = instances_[i]->GetProperty(propertyName);
                } catch (...) {
                    mixed = true;
                    break;
                }
                if (otherValue != primaryValue) {
                    mixed = true;
                    break;
                }
            }

            QWidget* editor = CreateEditor(propertyName, definition, primaryValue, mixed, contentWidget);
            if (editor == nullptr) {
                continue;
            }
            if (definition.ReadOnly) {
                editor->setEnabled(false);
            }

            if (!editor->property("keep_size_policy").toBool()) {
                QSizePolicy editorPolicy = editor->sizePolicy();
                editorPolicy.setHorizontalPolicy(QSizePolicy::Expanding);
                editor->setSizePolicy(editorPolicy);
            }

            auto* label = new QLabel(definition.Name, contentWidget);
            label->setWordWrap(false);
            label->setMinimumWidth(0);
            label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            QString tooltip = definition.Name;
            if (!definition.Description.isEmpty()) {
                tooltip += "\n\n" + definition.Description;
            }
            label->setToolTip(tooltip);

            const int row = grid->rowCount();
            if (definition.CustomTags.contains("IsSeparateBox")) {
                grid->addWidget(label, row, 0, 1, 2);
                grid->addWidget(editor, row + 1, 0, 1, 2);
            } else {
                grid->addWidget(label, row, 0);
                grid->addWidget(editor, row, 1);
            }
            editors_.insert(propertyName, editor);
            editorDefinitions_.insert(propertyName, definition);
        }

        auto* section = new Common::CollapsibleSection(category, contentWidget, false, contentRoot_);
        layout_->insertWidget(layout_->count() - 1, section);
    }

    if (instances_.size() == 1) {
        const QPointer<PropertiesWidget> safeThis(this);
        const std::weak_ptr<Engine::Core::Instance> boundInstance = instance_;
        propertyChangedConnection_ = instance_->PropertyChanged.Connect(
            [safeThis, boundInstance](const QString& propertyName, const QVariant& value) {
                if (safeThis.isNull()) {
                    return;
                }
                QMetaObject::invokeMethod(
                    safeThis,
                    [safeThis, boundInstance, propertyName, value]() {
                        if (safeThis.isNull() || safeThis->instance_ != boundInstance.lock()) {
                            return;
                        }
                        safeThis->OnPropertyChanged(propertyName, value);
                    },
                    Qt::QueuedConnection
                );
            }
        );
    }

    isBinding_ = false;
    ProcessQueuedBinding();
}

QWidget* PropertiesWidget::CreateInstanceHeader(
    const std::shared_ptr<Engine::Core::Instance>& instance,
    QWidget* parent
) {
    auto* header = new QWidget(parent);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 6, 8, 6);
    headerLayout->setSpacing(6);

    headerIconLabel_ = new QLabel(header);
    headerIconLabel_->setFixedSize(16, 16);
    const QPixmap pixmap = Core::GetIconPackManager().GetPixmapForInstance(instance);
    if (!pixmap.isNull()) {
        headerIconLabel_->setPixmap(pixmap);
    }
    headerLayout->addWidget(headerIconLabel_);

    headerTitleLabel_ = new QLabel(
        QString("%1 (%2)").arg(instance->GetProperty("Name").toString(), instance->GetClassName()),
        header
    );
    headerLayout->addWidget(headerTitleLabel_);
    headerLayout->addStretch(1);
    return header;
}

QWidget* PropertiesWidget::CreateMultiInstanceHeader(const int count, QWidget* parent) {
    auto* header = new QWidget(parent);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 6, 8, 6);
    headerLayout->setSpacing(6);

    headerIconLabel_ = nullptr;
    headerTitleLabel_ = new QLabel(QString("Multiple Objects (%1)").arg(count), header);
    headerLayout->addWidget(headerTitleLabel_);
    headerLayout->addStretch(1);
    return header;
}

bool PropertiesWidget::ShouldShowProperty(
    const std::shared_ptr<Engine::Core::Instance>& instance,
    const Engine::Core::PropertyDefinition& definition
) const {
    for (const auto& tag : definition.CustomTags) {
        const auto parsed = Engine::Core::PropertyTags::ParseVisibleIfTag(tag);
        if (!parsed.has_value()) {
            continue;
        }
        const QString& dependencyProperty = parsed->first;
        const QString& expectedValue = parsed->second;

        QVariant actualValue;
        try {
            actualValue = instance->GetProperty(dependencyProperty);
        } catch (...) {
            return false;
        }

        if (!MatchesConditionValue(actualValue, expectedValue)) {
            return false;
        }
    }
    return true;
}

bool PropertiesWidget::MatchesConditionValue(const QVariant& actualValue, const QString& expectedValue) const {
    const int typeId = actualValue.typeId();
    if (typeId == QMetaType::Bool) {
        return actualValue.toBool() == (expectedValue.compare("true", Qt::CaseInsensitive) == 0);
    }
    if (typeId == QMetaType::Int) {
        bool ok = false;
        const int expectedInt = expectedValue.toInt(&ok);
        return ok && actualValue.toInt() == expectedInt;
    }
    if (typeId == QMetaType::Double) {
        bool ok = false;
        const double expectedDouble = expectedValue.toDouble(&ok);
        return ok && qFuzzyCompare(actualValue.toDouble() + 1.0, expectedDouble + 1.0);
    }

    const QList<ValueUtils::EnumOption> options = ValueUtils::EnumOptionsForType(typeId);
    if (!options.isEmpty()) {
        const int enumAsInt = actualValue.toInt();
        return ValueUtils::EnumNameFromTypeAndInt(typeId, enumAsInt).compare(expectedValue, Qt::CaseInsensitive) == 0;
    }

    return actualValue.toString().compare(expectedValue, Qt::CaseInsensitive) == 0;
}

void PropertiesWidget::OnPropertyEdited(const QString& propertyName, const QVariant& value) {
    if (instances_.empty()) {
        return;
    }

    std::vector<std::shared_ptr<Engine::Utils::SetPropertyCommand>> commands;
    commands.reserve(instances_.size());

    for (const auto& inst : instances_) {
        if (inst == nullptr) {
            continue;
        }

        QVariant oldValue;
        try {
            oldValue = inst->GetProperty(propertyName);
        } catch (...) {
            continue;
        }
        if (oldValue == value) {
            continue;
        }

        commands.push_back(std::make_shared<Engine::Utils::SetPropertyCommand>(inst, propertyName, oldValue, value));
    }

    if (commands.empty()) {
        return;
    }

    if (historyService_ == nullptr) {
        for (const auto& command : commands) {
            command->Do();
        }
        return;
    }

    if (historyService_->IsRecording()) {
        try {
            for (const auto& command : commands) {
                historyService_->Record(command);
            }
        } catch (const std::exception& ex) {
            Engine::Core::RegularError::ShowErrorFromException(ex);
        }
        return;
    }

    historyService_->BeginRecording(QString("Set %1").arg(propertyName));
    try {
        for (const auto& command : commands) {
            historyService_->Record(command);
        }
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
    historyService_->FinishRecording();
}

void PropertiesWidget::OnPropertyChanged(const QString& propertyName, const QVariant& value) {
    if (instances_.size() != 1) {
        return;
    }
    if (propertyName == "Name" && headerTitleLabel_ != nullptr && instance_ != nullptr) {
        headerTitleLabel_->setText(QString("%1 (%2)").arg(value.toString(), instance_->GetClassName()));
    }

    if (visibilityDependencies_.contains(propertyName)) {
        RebuildForCurrentInstance();
        return;
    }

    QWidget* editor = editors_.value(propertyName, nullptr);
    if (editor == nullptr) {
        return;
    }
    const auto definition = editorDefinitions_.value(propertyName);
    SetEditorValue(editor, value, definition);
}

void PropertiesWidget::SetEditorValue(
    QWidget* editor,
    const QVariant& value,
    const Engine::Core::PropertyDefinition& definition
) const {
    EditorUtils::SetEditorValue(editor, value, definition);
}

QWidget* PropertiesWidget::CreateEditor(
    const QString& propertyName,
    const Engine::Core::PropertyDefinition& definition,
    const QVariant& value,
    const bool mixed,
    QWidget* parent
) {
    if (!mixed) {
        return EditorUtils::CreateEditor(
            propertyName,
            definition,
            value,
            parent,
            [this](const QString& editedPropertyName, const QVariant& editedValue) {
                OnPropertyEdited(editedPropertyName, editedValue);
            }
        );
    }

    const int typeId = definition.Type.id();

    if (typeId == QMetaType::Bool) {
        auto* editor = new QCheckBox(parent);
        editor->setTristate(true);
        editor->setCheckState(Qt::PartiallyChecked);
        QObject::connect(editor, &QCheckBox::checkStateChanged, editor, [propertyName, this](const int state) {
            if (state == Qt::PartiallyChecked) {
                return;
            }
            OnPropertyEdited(propertyName, state == Qt::Checked);
        });
        return editor;
    }

    if (typeId == QMetaType::Int) {
        auto* editor = new QLineEdit(parent);
        editor->setPlaceholderText("<multiple>");
        editor->setValidator(new QIntValidator(editor));
        const QPointer<QLineEdit> safeEditor(editor);
        QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, this, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            bool ok = false;
            const int next = safeEditor->text().toInt(&ok);
            if (!ok) {
                return;
            }
            OnPropertyEdited(propertyName, next);
        });
        return editor;
    }

    if (typeId == QMetaType::Double) {
        auto* editor = new QLineEdit(parent);
        editor->setPlaceholderText("<multiple>");
        editor->setValidator(new QDoubleValidator(editor));
        const QPointer<QLineEdit> safeEditor(editor);
        QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, this, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            bool ok = false;
            const double next = safeEditor->text().toDouble(&ok);
            if (!ok) {
                return;
            }
            OnPropertyEdited(propertyName, next);
        });
        return editor;
    }

    const QList<ValueUtils::EnumOption> enumOptions = ValueUtils::EnumOptionsForType(typeId);
    if (!enumOptions.isEmpty()) {
        auto* editor = new QComboBox(parent);
        editor->addItem("<multiple>", QVariant());
        for (const auto& option : enumOptions) {
            editor->addItem(option.Name, option.Value);
        }
        editor->setCurrentIndex(0);
        const QPointer<QComboBox> safeEditor(editor);
        QObject::connect(
            editor,
            qOverload<int>(&QComboBox::currentIndexChanged),
            editor,
            [propertyName, this, safeEditor, typeId](const int index) {
                if (safeEditor.isNull() || index <= 0 || index >= safeEditor->count()) {
                    return;
                }
                OnPropertyEdited(
                    propertyName,
                    ValueUtils::EnumVariantFromTypeAndInt(typeId, safeEditor->itemData(index).toInt())
                );
            }
        );
        return editor;
    }

    if (definition.IsInstanceReference) {
        auto* editor = new QLineEdit(parent);
        editor->setText("<multiple>");
        editor->setReadOnly(true);
        return editor;
    }

    if (typeId == QMetaType::fromType<Lvs::Engine::Math::Vector3>().id()) {
        auto* editor = new QLineEdit(parent);
        editor->setPlaceholderText("<multiple>");
        const QPointer<QLineEdit> safeEditor(editor);
        QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, this, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            Lvs::Engine::Math::Vector3 parsed;
            if (!ValueUtils::TryParseVector3(safeEditor->text(), parsed)) {
                return;
            }
            OnPropertyEdited(propertyName, QVariant::fromValue(parsed));
        });
        return editor;
    }

    if (typeId == QMetaType::fromType<Lvs::Engine::Math::CFrame>().id()) {
        auto* editor = new QLineEdit(parent);
        editor->setPlaceholderText("<multiple>");
        const QPointer<QLineEdit> safeEditor(editor);
        QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, this, safeEditor]() {
            if (safeEditor.isNull()) {
                return;
            }
            Lvs::Engine::Math::CFrame parsed;
            if (!ValueUtils::TryParseCFrame(safeEditor->text(), parsed)) {
                return;
            }
            OnPropertyEdited(propertyName, QVariant::fromValue(parsed));
        });
        return editor;
    }

    auto* editor = new QLineEdit(parent);
    editor->setPlaceholderText("<multiple>");
    const QPointer<QLineEdit> safeEditor(editor);
    QObject::connect(editor, &QLineEdit::editingFinished, editor, [propertyName, this, safeEditor, typeId]() {
        if (safeEditor.isNull()) {
            return;
        }
        if (typeId == QMetaType::QString) {
            OnPropertyEdited(propertyName, safeEditor->text());
            return;
        }
        if (!safeEditor->text().isEmpty()) {
            OnPropertyEdited(propertyName, safeEditor->text());
        }
    });
    return editor;
}

void PropertiesWidget::RebuildForCurrentInstance() {
    auto instances = instances_;
    BindInstances(instances);
}

void PropertiesWidget::ResetContentRoot() {
    if (scroll_ == nullptr) {
        return;
    }

    if (contentRoot_ == nullptr || scroll_->widget() != contentRoot_) {
        if (QWidget* oldContent = scroll_->takeWidget(); oldContent != nullptr && oldContent != contentRoot_) {
            oldContent->deleteLater();
        }

        contentRoot_ = new QWidget(scroll_);
        contentRoot_->setAutoFillBackground(true);
        contentRoot_->setBackgroundRole(QPalette::Base);
        layout_ = new QVBoxLayout(contentRoot_);
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setSpacing(4);
        layout_->addStretch(1);
        scroll_->setWidget(contentRoot_);
        return;
    }

    if (layout_ == nullptr) {
        layout_ = new QVBoxLayout(contentRoot_);
        layout_->setContentsMargins(0, 0, 0, 0);
        layout_->setSpacing(4);
        layout_->addStretch(1);
        return;
    }

    while (layout_->count() > 1) {
        QLayoutItem* item = layout_->takeAt(0);
        if (item == nullptr) {
            break;
        }
        if (QWidget* widget = item->widget(); widget != nullptr) {
            widget->deleteLater();
        }
        delete item;
    }
}

} // namespace Lvs::Studio::Widgets::Properties
