#include "Lvs/Engine/DataModel/ClassRegistry.hpp"

#include <stdexcept>

namespace Lvs::Engine::DataModel::ClassRegistry {

namespace {
Core::HashMap<Core::String, ClassInfo>& Registry() {
    static Core::HashMap<Core::String, ClassInfo> registry;
    return registry;
}

Core::OrderedMap<Core::String, Core::Vector<ClassInfo>>& CategoryMap() {
    static Core::OrderedMap<Core::String, Core::Vector<ClassInfo>> categoryMap;
    return categoryMap;
}
} // namespace

void RegisterClass(const ClassInfo& classInfo) {
    Registry().insert_or_assign(classInfo.Name, classInfo);
    CategoryMap()[classInfo.Category].push_back(classInfo);
}

std::shared_ptr<Core::Instance> CreateInstance(const Core::String& name) {
    const auto it = Registry().find(name);
    if (it == Registry().end()) {
        throw std::runtime_error("Class not registered: " + name);
    }
    return it->second.Factory();
}

Core::Vector<ClassInfo> GetClasses() {
    Core::Vector<ClassInfo> classes;
    classes.reserve(Registry().size());
    for (const auto& [name, info] : Registry()) {
        (void)name;
        classes.push_back(info);
    }
    return classes;
}

Core::OrderedMap<Core::String, Core::Vector<ClassInfo>> GetClassesByCategory() {
    return CategoryMap();
}

Core::Vector<ClassInfo> GetDerivedClasses(const Core::String& baseName) {
    Core::Vector<ClassInfo> derived;
    for (const auto& [name, info] : Registry()) {
        (void)name;
        if (info.BaseClass == baseName) {
            derived.push_back(info);
        }
    }
    return derived;
}

} // namespace Lvs::Engine::DataModel::ClassRegistry
