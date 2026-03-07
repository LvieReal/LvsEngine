#pragma once

#include <memory>

namespace Lvs::Engine::Rendering::Common {

class ResourceBinding;

class BindingLayout {
public:
    virtual ~BindingLayout() = default;

    [[nodiscard]] virtual std::unique_ptr<ResourceBinding> AllocateBinding() const = 0;
    [[nodiscard]] virtual void* GetNativeHandle() const = 0;
    [[nodiscard]] virtual bool IsValid() const = 0;
};

} // namespace Lvs::Engine::Rendering::Common
