#include "Lvs/Engine/Utils/CommandGroup.hpp"

namespace Lvs::Engine::Utils {

CommandGroup::CommandGroup(QString name)
    : name_(std::move(name)) {
}

void CommandGroup::Add(std::shared_ptr<Command> command) {
    commands_.push_back(std::move(command));
}

void CommandGroup::Do() const {
    for (const auto& command : commands_) {
        command->Do();
    }
}

void CommandGroup::Undo() const {
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        (*it)->Undo();
    }
}

const QString& CommandGroup::Name() const {
    return name_;
}

const std::vector<std::shared_ptr<Command>>& CommandGroup::Commands() const {
    return commands_;
}

} // namespace Lvs::Engine::Utils
