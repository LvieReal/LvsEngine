#pragma once

#include "Lvs/Engine/DataModel/Objects/BasePart.hpp"

namespace Lvs::Engine::DataModel::Objects {

class MeshPart : public BasePart {
public:
    MeshPart();
    ~MeshPart() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::DataModel::Objects
