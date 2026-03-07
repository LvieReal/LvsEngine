#pragma once

namespace Lvs::Engine::Rendering::Common {

class ShaderModule {
public:
    virtual ~ShaderModule() = default;

    [[nodiscard]] virtual bool IsValid() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
