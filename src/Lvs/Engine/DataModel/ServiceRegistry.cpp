#include "Lvs/Engine/DataModel/ServiceRegistry.hpp"

namespace Lvs::Engine::DataModel::ServiceRegistry {

namespace {
QVector<ServiceFactory>& Registry() {
    static QVector<ServiceFactory> factories;
    return factories;
}
} // namespace

void RegisterService(const ServiceFactory& factory) {
    Registry().push_back(factory);
}

QVector<ServiceFactory> GetServiceClasses() {
    return Registry();
}

} // namespace Lvs::Engine::DataModel::ServiceRegistry
