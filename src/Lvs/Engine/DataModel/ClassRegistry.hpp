#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/Types.hpp"

#include <functional>
#include <memory>

namespace Lvs::Engine::DataModel::ClassRegistry {

struct ClassInfo {
    Core::String Name;
    Core::String Category;
    Core::String BaseClass;
    std::function<std::shared_ptr<Core::Instance>()> Factory;
};

void RegisterClass(const ClassInfo& classInfo);
std::shared_ptr<Core::Instance> CreateInstance(const Core::String& name);
Core::Vector<ClassInfo> GetClasses();
Core::OrderedMap<Core::String, Core::Vector<ClassInfo>> GetClassesByCategory();
Core::Vector<ClassInfo> GetDerivedClasses(const Core::String& baseName);

template <typename T>
void RegisterClass(
    Core::String name,
    Core::String category = "General",
    Core::String baseClass = {}
) {
    RegisterClass(ClassInfo{
        .Name = std::move(name),
        .Category = std::move(category),
        .BaseClass = std::move(baseClass),
        .Factory = []() { return std::make_shared<T>(); }
    });
}

} // namespace Lvs::Engine::DataModel::ClassRegistry
