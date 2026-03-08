#pragma once

#include <string>

namespace Lvs::Engine::Rendering::Common {

class RenderResourceRegistry {
public:
    virtual ~RenderResourceRegistry() = default;
    [[nodiscard]] virtual std::string GetTexturePath(const std::string& resourceId) const = 0;
};

} // namespace Lvs::Engine::Rendering::Common

