#pragma once

#include "Lvs/Engine/Utils/Command.hpp"
#include "Lvs/Engine/Core/Types.hpp"

#include <memory>
#include <vector>

namespace Lvs::Engine::Utils {

class CommandGroup final {
public:
    explicit CommandGroup(Core::String name = {});

    void Add(std::shared_ptr<Command> command);
    void Do() const;
    void Undo() const;

    [[nodiscard]] const Core::String& Name() const;
    [[nodiscard]] const std::vector<std::shared_ptr<Command>>& Commands() const;

private:
    Core::String name_;
    std::vector<std::shared_ptr<Command>> commands_;
};

} // namespace Lvs::Engine::Utils
