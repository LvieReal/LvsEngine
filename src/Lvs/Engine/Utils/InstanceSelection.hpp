#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/AABB.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Utils/Raycast.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace Lvs::Engine::Utils {

inline bool HasAncestorInSet(
    const std::shared_ptr<Core::Instance>& instance,
    const std::unordered_set<const Core::Instance*>& set
) {
    if (instance == nullptr) {
        return false;
    }
    auto parent = instance->GetParent();
    while (parent != nullptr) {
        if (set.contains(parent.get())) {
            return true;
        }
        parent = parent->GetParent();
    }
    return false;
}

inline std::vector<std::shared_ptr<Core::Instance>> FilterTopLevelInstances(
    const std::vector<std::shared_ptr<Core::Instance>>& instances
) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("InstanceSelection::FilterTopLevelInstances");
    }
    std::vector<std::shared_ptr<Core::Instance>> out;
    out.reserve(instances.size());

    std::unordered_set<const Core::Instance*> selected;
    selected.reserve(instances.size());
    for (const auto& inst : instances) {
        if (inst != nullptr) {
            selected.insert(inst.get());
        }
    }

    for (const auto& inst : instances) {
        if (inst == nullptr) {
            continue;
        }
        if (HasAncestorInSet(inst, selected)) {
            continue;
        }
        out.push_back(inst);
    }
    return out;
}

inline std::vector<std::shared_ptr<Objects::BasePart>> CollectDescendantBaseParts(
    const std::shared_ptr<Core::Instance>& instance
) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("InstanceSelection::CollectDescendantBaseParts");
    }
    std::vector<std::shared_ptr<Objects::BasePart>> parts;
    if (instance == nullptr) {
        return parts;
    }

    if (const auto part = std::dynamic_pointer_cast<Objects::BasePart>(instance); part != nullptr) {
        parts.push_back(part);
        return parts;
    }

    const auto descendants = instance->GetDescendants();
    parts.reserve(descendants.size());
    for (const auto& desc : descendants) {
        const auto part = std::dynamic_pointer_cast<Objects::BasePart>(desc);
        if (part == nullptr) {
            continue;
        }
        parts.push_back(part);
    }
    return parts;
}

inline std::vector<std::shared_ptr<Objects::BasePart>> CollectBasePartsFromInstances(
    const std::vector<std::shared_ptr<Core::Instance>>& instances
) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("InstanceSelection::CollectBasePartsFromInstances");
    }
    std::vector<std::shared_ptr<Objects::BasePart>> out;
    std::unordered_set<const Objects::BasePart*> seen;

    for (const auto& inst : instances) {
        const auto parts = CollectDescendantBaseParts(inst);
        for (const auto& part : parts) {
            if (part == nullptr) {
                continue;
            }
            if (!seen.insert(part.get()).second) {
                continue;
            }
            out.push_back(part);
        }
    }

    return out;
}

inline std::optional<Math::AABB> ComputeCombinedWorldAABB(
    const std::vector<std::shared_ptr<Objects::BasePart>>& parts
) {
    if (Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("InstanceSelection::ComputeCombinedWorldAABB");
    }
    Math::AABB combined{};
    bool hasBounds = false;

    for (const auto& part : parts) {
        if (part == nullptr || part->GetParent() == nullptr) {
            continue;
        }

        const auto aabb = BuildPartWorldAABB(part);
        if (!hasBounds) {
            combined = aabb;
            hasBounds = true;
        } else {
            combined.Min.x = std::min(combined.Min.x, aabb.Min.x);
            combined.Min.y = std::min(combined.Min.y, aabb.Min.y);
            combined.Min.z = std::min(combined.Min.z, aabb.Min.z);
            combined.Max.x = std::max(combined.Max.x, aabb.Max.x);
            combined.Max.y = std::max(combined.Max.y, aabb.Max.y);
            combined.Max.z = std::max(combined.Max.z, aabb.Max.z);
        }
    }

    if (!hasBounds) {
        return std::nullopt;
    }
    return combined;
}

inline std::shared_ptr<Objects::BasePart> ResolveLocalSpaceTargetPart(
    const std::shared_ptr<Core::Instance>& selectionPrimary,
    const std::vector<std::shared_ptr<Objects::BasePart>>& selectionParts
) {
    if (selectionPrimary == nullptr) {
        return !selectionParts.empty() ? selectionParts.front() : nullptr;
    }

    if (const auto primaryPart = std::dynamic_pointer_cast<Objects::BasePart>(selectionPrimary);
        primaryPart != nullptr) {
        return primaryPart;
    }

    if (selectionPrimary->GetClassName() == "Model") {
        const auto primaryVar = selectionPrimary->GetProperty("PrimaryPart");
        if (primaryVar.Is<Core::Variant::InstanceRef>()) {
            if (const auto locked = primaryVar.Get<Core::Variant::InstanceRef>().lock()) {
                if (const auto asPart = std::dynamic_pointer_cast<Objects::BasePart>(locked);
                    asPart != nullptr && asPart->GetParent() != nullptr) {
                    return asPart;
                }
            }
        }
    }

    const auto parts = CollectDescendantBaseParts(selectionPrimary);
    if (!parts.empty()) {
        return parts.front();
    }

    return !selectionParts.empty() ? selectionParts.front() : nullptr;
}

} // namespace Lvs::Engine::Utils
