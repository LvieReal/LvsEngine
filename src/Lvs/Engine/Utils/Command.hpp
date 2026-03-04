#pragma once

#include <QVariant>

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
        QString propertyName,
        QVariant oldValue,
        QVariant newValue
    );

    void Do() override;
    void Undo() override;

private:
    std::shared_ptr<Core::Instance> instance_;
    QString propertyName_;
    QVariant oldValue_;
    QVariant newValue_;
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
