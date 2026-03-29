#pragma once

#include "Lvs/Engine/Objects/Light.hpp"

namespace Lvs::Engine::Objects {

class DirectionalLight : public Light {
public:
    DirectionalLight();
    ~DirectionalLight() override = default;

    static Core::ClassDescriptor& Descriptor();
    [[nodiscard]] int GetLightType() const override;
};

} // namespace Lvs::Engine::Objects
