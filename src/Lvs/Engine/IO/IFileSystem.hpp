#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <optional>

namespace Lvs::Engine::IO {

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual bool Exists(const Core::String& path) const = 0;
    [[nodiscard]] virtual std::optional<Core::String> ReadAllText(const Core::String& path) const = 0;
    [[nodiscard]] virtual bool WriteAllText(const Core::String& path, const Core::String& data) const = 0;
    [[nodiscard]] virtual bool MkdirP(const Core::String& path) const = 0;
};

} // namespace Lvs::Engine::IO

