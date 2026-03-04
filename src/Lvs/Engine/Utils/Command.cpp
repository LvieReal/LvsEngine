#include "Lvs/Engine/Utils/Command.hpp"

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::Utils {

SetPropertyCommand::SetPropertyCommand(
    std::shared_ptr<Core::Instance> instance,
    QString propertyName,
    QVariant oldValue,
    QVariant newValue
)
    : instance_(std::move(instance)),
      propertyName_(std::move(propertyName)),
      oldValue_(std::move(oldValue)),
      newValue_(std::move(newValue)) {
}

void SetPropertyCommand::Do() {
    instance_->SetProperty(propertyName_, newValue_);
}

void SetPropertyCommand::Undo() {
    instance_->SetProperty(propertyName_, oldValue_);
}

ReparentCommand::ReparentCommand(
    std::shared_ptr<Core::Instance> instance,
    std::shared_ptr<Core::Instance> newParent
)
    : instance_(std::move(instance)),
      oldParent_(instance_->GetParent()),
      newParent_(std::move(newParent)) {
}

void ReparentCommand::Do() {
    instance_->SetParent(newParent_);
}

void ReparentCommand::Undo() {
    instance_->SetParent(oldParent_);
}

} // namespace Lvs::Engine::Utils
