#include "Lvs/Engine/Utils/Command.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"

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
    // Preserve world position during reparent for BaseParts
    Math::Vector3 worldPosition;
    bool isBasePart = false;
    if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance_); 
        part != nullptr) {
        worldPosition = part->GetWorldPosition();
        isBasePart = true;
    }
    
    instance_->SetParent(newParent_);
    
    // Restore world position after reparenting by adjusting local position
    if (isBasePart) {
        if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance_); 
            part != nullptr) {
            const auto newParentPart = std::dynamic_pointer_cast<Objects::BasePart>(newParent_);
            if (newParentPart != nullptr) {
                // Convert world position to new parent's local space
                const auto parentWorldCFrame = newParentPart->GetWorldCFrame();
                const auto worldCFrame = Math::CFrame::New(worldPosition);
                const auto localCFrame = parentWorldCFrame.Inverse() * worldCFrame;
                part->SetProperty("Position", QVariant::fromValue(localCFrame.Position));
            } else if (newParent_ == nullptr) {
                // Reparenting to root (no parent) - position is world space
                part->SetProperty("Position", QVariant::fromValue(worldPosition));
            }
        }
    }
}

void ReparentCommand::Undo() {
    // Preserve world position during undo reparent for BaseParts
    Math::Vector3 worldPosition;
    bool isBasePart = false;
    if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance_); 
        part != nullptr) {
        worldPosition = part->GetWorldPosition();
        isBasePart = true;
    }
    
    instance_->SetParent(oldParent_);
    
    // Restore world position after reparenting by adjusting local position
    if (isBasePart) {
        if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance_); 
            part != nullptr) {
            const auto oldParentPart = std::dynamic_pointer_cast<Objects::BasePart>(oldParent_);
            if (oldParentPart != nullptr) {
                // Convert world position to old parent's local space
                const auto parentWorldCFrame = oldParentPart->GetWorldCFrame();
                const auto worldCFrame = Math::CFrame::New(worldPosition);
                const auto localCFrame = parentWorldCFrame.Inverse() * worldCFrame;
                part->SetProperty("Position", QVariant::fromValue(localCFrame.Position));
            } else if (oldParent_ == nullptr) {
                // Reparenting to root (no parent) - position is world space
                part->SetProperty("Position", QVariant::fromValue(worldPosition));
            }
        }
    }
}

} // namespace Lvs::Engine::Utils
