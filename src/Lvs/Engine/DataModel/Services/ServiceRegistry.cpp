#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"

namespace Lvs::Engine::DataModel::ServiceRegistry {

namespace {
Core::Vector<ServiceFactory>& Registry() {
    static Core::Vector<ServiceFactory> factories;
    return factories;
}
} // namespace

void RegisterService(const ServiceFactory& factory) {
    Registry().push_back(factory);
}

Core::Vector<ServiceFactory> GetServiceClasses() {
    return Registry();
}

} // namespace Lvs::Engine::DataModel::ServiceRegistry
