#pragma once

#include "Lvs/Engine/DataModel/Service.hpp"

#include <QVector>

#include <functional>
#include <memory>

namespace Lvs::Engine::DataModel::ServiceRegistry {

using ServiceFactory = std::function<std::shared_ptr<Service>()>;

void RegisterService(const ServiceFactory& factory);
QVector<ServiceFactory> GetServiceClasses();

template <typename T>
void RegisterService() {
    RegisterService([]() { return std::make_shared<T>(); });
}

} // namespace Lvs::Engine::DataModel::ServiceRegistry
