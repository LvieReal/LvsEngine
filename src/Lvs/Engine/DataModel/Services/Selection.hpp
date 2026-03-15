#pragma once

#include "Lvs/Engine/DataModel/Services/Service.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <memory>
#include <vector>

namespace Lvs::Engine::DataModel {

class Selection : public Service {
public:
    Selection();
    ~Selection() override = default;

    static Core::ClassDescriptor& Descriptor();

    void Set(const std::vector<std::shared_ptr<Core::Instance>>& objects);
    void Set(const std::shared_ptr<Core::Instance>& object);
    void Clear();

    [[nodiscard]] std::vector<std::shared_ptr<Core::Instance>> Get() const;
    [[nodiscard]] std::shared_ptr<Core::Instance> GetPrimary() const;
    [[nodiscard]] bool HasSelection() const;

    Utils::Signal<const std::vector<std::shared_ptr<Core::Instance>>&> SelectionChanged;

private:
    std::vector<std::weak_ptr<Core::Instance>> selected_;
};

} // namespace Lvs::Engine::DataModel
