#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::DataModel::Objects {

class Skybox : public Core::Instance {
public:
    Skybox();
    ~Skybox() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::DataModel::Objects
