#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::Objects {

class Folder : public Core::Instance {
public:
    Folder();
    ~Folder() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::Objects

