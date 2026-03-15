#pragma once

#include "Lvs/Engine/DataModel/Services/Service.hpp"

namespace Lvs::Engine::DataModel {

class Lighting : public Service {
public:
    Lighting();
    ~Lighting() override = default;

    static Core::ClassDescriptor& Descriptor();
    void InitializeDefaultObjects();
};

} // namespace Lvs::Engine::DataModel
