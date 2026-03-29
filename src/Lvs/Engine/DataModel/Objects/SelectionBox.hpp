#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

namespace Lvs::Engine::Objects {

class SelectionBox : public Core::Instance {
public:
    SelectionBox();
    ~SelectionBox() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::Objects
