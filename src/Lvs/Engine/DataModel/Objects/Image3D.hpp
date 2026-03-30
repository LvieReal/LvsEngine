#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::DataModel::Objects {

class Image3D : public Core::Instance {
public:
    Image3D();
    ~Image3D() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::DataModel::Objects

