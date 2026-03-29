#include "Lvs/Engine/DataModel/Services/Selection.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"

#include <mutex>
#include <unordered_set>

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& Selection::Descriptor() {
    static Core::ClassDescriptor descriptor("Selection", &Service::Descriptor());
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<Selection>("Selection", "Services", "Service");
        ServiceRegistry::RegisterService<Selection>();
    });
    return descriptor;
}

Selection::Selection()
    : Service(Descriptor()) {
    SetServiceFlags(true, true);
    SetInsertable(false);
}

void Selection::Set(const std::vector<std::shared_ptr<Core::Instance>>& objects) {
    std::vector<std::shared_ptr<Core::Instance>> normalized;
    normalized.reserve(objects.size());

    std::unordered_set<const Core::Instance*> seen;
    for (const auto& object : objects) {
        if (object == nullptr) {
            continue;
        }
        if (!seen.insert(object.get()).second) {
            continue;
        }
        normalized.push_back(object);
    }

    if (Get() == normalized) {
        return;
    }

    selected_.clear();
    for (const auto& object : normalized) {
        selected_.push_back(object);
    }
    SelectionChanged.Fire(normalized);
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

void Selection::PruneInvalid() {
    std::vector<std::shared_ptr<Core::Instance>> live;
    live.reserve(selected_.size());
    std::vector<std::weak_ptr<Core::Instance>> nextSelected;
    nextSelected.reserve(selected_.size());

    std::unordered_set<const Core::Instance*> seen;
    bool changed = false;

    for (const auto& weak : selected_) {
        const auto shared = weak.lock();
        if (shared == nullptr) {
            changed = true;
            continue;
        }
        if (!seen.insert(shared.get()).second) {
            changed = true;
            continue;
        }
        live.push_back(shared);
        nextSelected.push_back(shared);
    }

    if (!changed) {
        return;
    }

    selected_ = std::move(nextSelected);
    SelectionChanged.Fire(live);
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
