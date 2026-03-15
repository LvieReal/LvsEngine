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

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPixmap>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <algorithm>
#include <memory>
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
        Core::Settings::Changed("ExplorerIconPack", [this](const QVariant&) {
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
    queuedInstance_.reset();
    ClearInternal(false);
}

void PropertiesWidget::Clear() {
    if (isBinding_) {
        clearQueued_ = true;
        bindQueued_ = false;
        queuedInstance_.reset();
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
        auto next = queuedInstance_;
        bindQueued_ = false;
        queuedInstance_.reset();
        BindInstance(next);
    }
}

void PropertiesWidget::BindInstance(const std::shared_ptr<Engine::Core::Instance>& instance) {
    if (isBinding_) {
        bindQueued_ = true;
        clearQueued_ = false;
        queuedInstance_ = instance;
        return;
    }

    isBinding_ = true;
    ClearInternal();
    if (instance == nullptr) {
        isBinding_ = false;
        ProcessQueuedBinding();
        return;
    }
    instance_ = instance;

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
        orderedProperties.push_back({
            it.key(),
            it->Category,
            it->RegistrationOrder
        });
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
        if (!ShouldShowProperty(instance_, definition)) {
            continue;
        }

        for (const auto& tag : definition.CustomTags) {
            const auto parsed = Engine::Core::PropertyTags::ParseVisibleIfTag(tag);
            if (parsed.has_value()) {
                visibilityDependencies_.insert(parsed->first);
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
            contentLayout->addWidget(CreateInstanceHeader(instance_, contentWidget));
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
            QWidget* editor = CreateEditor(propertyName, definition, property.Get(), contentWidget);
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
    if (instance_ == nullptr) {
        return;
    }

    QVariant oldValue;
    try {
        oldValue = instance_->GetProperty(propertyName);
    } catch (...) {
        return;
    }
    if (oldValue == value) {
        return;
    }

    auto command = std::make_shared<Engine::Utils::SetPropertyCommand>(instance_, propertyName, oldValue, value);
    if (historyService_ == nullptr) {
        command->Do();
        return;
    }

    historyService_->BeginRecording(QString("Set %1").arg(propertyName));
    try {
        historyService_->Record(command);
    } catch (const std::exception& ex) {
        Engine::Core::RegularError::ShowErrorFromException(ex);
    }
    historyService_->FinishRecording();
}

void PropertiesWidget::OnPropertyChanged(const QString& propertyName, const QVariant& value) {
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
    QWidget* parent
) {
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

void PropertiesWidget::RebuildForCurrentInstance() {
    auto instance = instance_;
    BindInstance(instance);
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
