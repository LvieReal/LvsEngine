#include "Lvs/Engine/DataModel/Services/Selection.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Selection::Descriptor() {
    static Core::ClassDescriptor descriptor("Selection", &Service::Descriptor());
    static const bool initialized = []() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Selection>("Selection", "Services", "Service");
        ServiceRegistry::RegisterService<Selection>();
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

Selection::Selection()
    : Service(Descriptor()) {
    SetServiceFlags(true, true);
    SetInsertable(false);
}

void Selection::Set(const std::vector<std::shared_ptr<Core::Instance>>& objects) {
    std::vector<std::shared_ptr<Core::Instance>> current = Get();
    if (current == objects) {
        return;
    }

    selected_.clear();
    for (const auto& object : objects) {
        selected_.push_back(object);
    }
    SelectionChanged.Fire(Get());
}

void Selection::Set(const std::shared_ptr<Core::Instance>& object) {
    if (object == nullptr) {
        Set(std::vector<std::shared_ptr<Core::Instance>>{});
        return;
    }
    Set(std::vector<std::shared_ptr<Core::Instance>>{object});
}

void Selection::Clear() {
    Set(std::vector<std::shared_ptr<Core::Instance>>{});
}

std::vector<std::shared_ptr<Core::Instance>> Selection::Get() const {
    std::vector<std::shared_ptr<Core::Instance>> result;
    result.reserve(selected_.size());
    for (const auto& weak : selected_) {
        if (const auto shared = weak.lock(); shared != nullptr) {
            result.push_back(shared);
        }
    }
    return result;
}

std::shared_ptr<Core::Instance> Selection::GetPrimary() const {
    for (const auto& weak : selected_) {
        if (const auto shared = weak.lock(); shared != nullptr) {
            return shared;
        }
    }
    return nullptr;
}

bool Selection::HasSelection() const {
    return GetPrimary() != nullptr;
}

} // namespace Lvs::Engine::DataModel
