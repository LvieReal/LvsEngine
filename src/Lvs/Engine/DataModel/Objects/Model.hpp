#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::DataModel::Objects {

class Model : public Core::Instance {
public:
    Model();
    ~Model() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::DataModel::Objects
