#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::DataModel {

class Service : public Core::Instance {
public:
    Service();
    ~Service() override = default;

    static Core::ClassDescriptor& Descriptor();

protected:
    explicit Service(const Core::ClassDescriptor& descriptor);
};

} // namespace Lvs::Engine::DataModel
