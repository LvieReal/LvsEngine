#pragma once

#include "Lvs/Engine/DataModel/Objects/BasePart.hpp"

namespace Lvs::Engine::DataModel::Objects {

class Part : public BasePart {
public:
    Part();
    ~Part() override = default;

    static Core::ClassDescriptor& Descriptor();

protected:
    explicit Part(const Core::ClassDescriptor& descriptor);
};

} // namespace Lvs::Engine::DataModel::Objects
