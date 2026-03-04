#pragma once

#include "Lvs/Engine/Core/PropertyDefinition.hpp"

#include <QVariant>
#include <QWidget>

#include <functional>

namespace Lvs::Studio::Widgets::Properties::EditorUtils {

QWidget* CreateEditor(
    const QString& propertyName,
    const Engine::Core::PropertyDefinition& definition,
    const QVariant& value,
    QWidget* parent,
    const std::function<void(const QString&, const QVariant&)>& onEdited
);

void SetEditorValue(
    QWidget* editor,
    const QVariant& value,
    const Engine::Core::PropertyDefinition& definition
);

} // namespace Lvs::Studio::Widgets::Properties::EditorUtils

