#pragma once

#include "Lvs/Engine/Core/PropertyDefinition.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"
#include "Lvs/Studio/Core/Settings.hpp"

#include <QHash>
#include <QPointer>
#include <QSet>
#include <QWidget>

#include <memory>
#include <optional>

class QLabel;
class QScrollArea;
class QVBoxLayout;
class QWidget;

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::DataModel {
class ChangeHistoryService;
class Place;
}

namespace Lvs::Studio::Widgets::Properties {

class PropertiesWidget final : public QWidget {
public:
    explicit PropertiesWidget(const std::shared_ptr<Engine::DataModel::Place>& place, QWidget* parent = nullptr);
    ~PropertiesWidget() override;

    void Clear();
    void BindInstance(const std::shared_ptr<Engine::Core::Instance>& instance);

private:
    void ClearInternal(bool resetContentRoot = true);
    void ProcessQueuedBinding();
    void ResetContentRoot();
    void AddInstanceHeader(const std::shared_ptr<Engine::Core::Instance>& instance);
    bool ShouldShowProperty(
        const std::shared_ptr<Engine::Core::Instance>& instance,
        const Engine::Core::PropertyDefinition& definition
    ) const;
    bool MatchesConditionValue(const QVariant& actualValue, const QString& expectedValue) const;
    void OnPropertyEdited(const QString& propertyName, const QVariant& value);
    void OnPropertyChanged(const QString& propertyName, const QVariant& value);
    void SetEditorValue(QWidget* editor, const QVariant& value, const Engine::Core::PropertyDefinition& definition) const;
    QWidget* CreateEditor(
        const QString& propertyName,
        const Engine::Core::PropertyDefinition& definition,
        const QVariant& value,
        QWidget* parent
    );
    void RebuildForCurrentInstance();

    QPointer<QVBoxLayout> layout_;
    std::shared_ptr<Engine::DataModel::ChangeHistoryService> historyService_;
    std::shared_ptr<Engine::Core::Instance> instance_;
    QHash<QString, QWidget*> editors_;
    QHash<QString, Engine::Core::PropertyDefinition> editorDefinitions_;
    QSet<QString> visibilityDependencies_;
    QPointer<QLabel> headerIconLabel_;
    QPointer<QLabel> headerTitleLabel_;
    std::shared_ptr<Core::Settings::Connection> iconPackConnection_;
    std::optional<Engine::Utils::Signal<const QString&, const QVariant&>::Connection> propertyChangedConnection_;
    QPointer<QScrollArea> scroll_;
    QPointer<QWidget> contentRoot_;
    bool isBinding_{false};
    bool clearQueued_{false};
    bool bindQueued_{false};
    std::shared_ptr<Engine::Core::Instance> queuedInstance_;
};

} // namespace Lvs::Studio::Widgets::Properties
