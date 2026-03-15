#pragma once

#include "Lvs/Engine/DataModel/Services/Service.hpp"

namespace Lvs::Engine::DataModel {

class QualitySettings : public Service {
public:
    QualitySettings();
    ~QualitySettings() override = default;

    static Core::ClassDescriptor& Descriptor();
};

} // namespace Lvs::Engine::DataModel
