#pragma once

#include "Lvs/Engine/Core/Instance.hpp"

#include <QHash>

namespace Lvs::Engine::DataModel {

class Place;

class DataModel : public Core::Instance {
public:
    DataModel();
    ~DataModel() override = default;

    static Core::ClassDescriptor& Descriptor();

    void RegisterInstance(const std::shared_ptr<Core::Instance>& instance);
    void UnregisterInstance(const std::shared_ptr<Core::Instance>& instance);
    std::shared_ptr<Core::Instance> FindInstanceById(const QString& instanceId) const;

    void SetOwnerPlace(Place* ownerPlace);
    [[nodiscard]] Place* GetOwnerPlace() const;

private:
    QHash<QString, std::weak_ptr<Core::Instance>> instanceRegistry_;
    Place* ownerPlace_{nullptr};
};

} // namespace Lvs::Engine::DataModel
