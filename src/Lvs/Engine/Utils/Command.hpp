#pragma once

#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Core/Variant.hpp"

#include <memory>

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::Utils {

class Command {
public:
    virtual ~Command() = default;
    virtual void Do() = 0;
    virtual void Undo() = 0;
};

class SetPropertyCommand final : public Command {
public:
    SetPropertyCommand(
        std::shared_ptr<Core::Instance> instance,
        Core::String propertyName,
        Core::Variant oldValue,
        Core::Variant newValue
    );

    void Do() override;
    void Undo() override;

private:
    std::shared_ptr<Core::Instance> instance_;
    Core::String propertyName_;
    Core::Variant oldValue_;
    Core::Variant newValue_;
};

class ReparentCommand final : public Command {
public:
    ReparentCommand(
        std::shared_ptr<Core::Instance> instance,
        std::shared_ptr<Core::Instance> newParent
    );

    void Do() override;
    void Undo() override;

private:
    std::shared_ptr<Core::Instance> instance_;
    std::shared_ptr<Core::Instance> oldParent_;
    std::shared_ptr<Core::Instance> newParent_;
};

} // namespace Lvs::Engine::Utils
