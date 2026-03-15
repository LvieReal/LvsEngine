#pragma once

#include "Lvs/Engine/DataModel/Services/Service.hpp"

namespace Lvs::Engine::DataModel {

class Workspace : public Service {
public:
    Workspace();
    ~Workspace() override = default;

    static Core::ClassDescriptor& Descriptor();
    void InitializeDefaultObjects();
};

} // namespace Lvs::Engine::DataModel
