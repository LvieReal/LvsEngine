#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::Objects {

class PostEffects : public Core::Instance {
public:
    PostEffects();
    ~PostEffects() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::Objects

