#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::Objects {

class Skybox : public Core::Instance {
public:
    Skybox();
    ~Skybox() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::Objects
