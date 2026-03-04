#pragma once

#include "Lvs/Engine/Objects/BasePart.hpp"

namespace Lvs::Engine::Objects {

class MeshPart : public BasePart {
public:
    MeshPart();
    ~MeshPart() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::Objects
