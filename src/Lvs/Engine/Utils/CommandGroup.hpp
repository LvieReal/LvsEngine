#pragma once

#include "Lvs/Engine/Utils/Command.hpp"

#include <QString>

#include <memory>
#include <vector>

namespace Lvs::Engine::Utils {

class CommandGroup final {
public:
    explicit CommandGroup(QString name = {});

    void Add(std::shared_ptr<Command> command);
    void Do() const;
    void Undo() const;

    [[nodiscard]] const QString& Name() const;
    [[nodiscard]] const std::vector<std::shared_ptr<Command>>& Commands() const;

private:
    QString name_;
    std::vector<std::shared_ptr<Command>> commands_;
};

} // namespace Lvs::Engine::Utils
