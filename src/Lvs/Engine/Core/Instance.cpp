#include "Lvs/Engine/Core/Instance.hpp"

#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>

namespace Lvs::Engine::Core {

namespace {
String GenerateUuidString() {
    std::array<uint8_t, 16> bytes{};
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    for (size_t i = 0; i < bytes.size(); i += 4) {
        const uint32_t v = dist(gen);
        bytes[i + 0] = static_cast<uint8_t>((v >> 0) & 0xFFu);
        bytes[i + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        bytes[i + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        bytes[i + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
    }

    // RFC 4122-ish version/variant bits (best-effort, only for formatting consistency).
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0Fu) | 0x40u);
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3Fu) | 0x80u);

    std::ostringstream oss;
    oss.setf(std::ios::hex, std::ios::basefield);
    oss.fill('0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss.width(2);
        oss << static_cast<unsigned int>(bytes[i]);
    }
    return oss.str();
}

ClassDescriptor& BaseDescriptor() {
    static ClassDescriptor descriptor("Instance", nullptr);
    static std::once_flag registered;
    std::call_once(registered, [&]() {
        descriptor.RegisterProperty(
            ObjectBase::MakePropertyDefinition<String>("Name", String{})
        );
        descriptor.RegisterProperty(
            ObjectBase::MakePropertyDefinition<String>(
                "ClassName",
                String{},
                true,
                "Data",
                {},
                true
            )
        );
        ClassDescriptor::RegisterClassDescriptor(&descriptor);
    });
    return descriptor;
}
} // namespace

Instance::Instance()
    : Instance(Descriptor()) {
}

Instance::Instance(const ClassDescriptor& descriptor)
    : ObjectBase(descriptor),
      id_(GenerateUuidString()) {
    const String className = GetClassName();
    SetProperty("Name", Variant::From(className));
    SetProperty("ClassName", Variant::From(className));
}

ClassDescriptor& Instance::Descriptor() {
    return BaseDescriptor();
}

const String& Instance::GetId() const {
    return id_;
}

String Instance::GetClassName() const {
    return GetClassDescriptor().ClassName();
}

String Instance::GetFullPath() const {
    const auto parent = GetParent();
    if (parent == nullptr) {
        return GetClassName();
    }
    return parent->GetFullPath() + "." + GetClassName();
}

bool Instance::IsService() const {
    return isService_;
}

bool Instance::IsHiddenService() const {
    return isService_ && isHiddenService_;
}

bool Instance::IsInsertable() const {
    return isInsertable_;
}

void Instance::SetProperty(const String& name, const Variant& value) {
    ObjectBase::SetProperty(name, value);
    PropertyChanged.Fire(name, GetProperty(name));
    PropertyInvalidated.Fire();
}

bool Instance::CanParentTo(const std::shared_ptr<Instance>& parent) const {
    static_cast<void>(parent);
    return true;
}

bool Instance::CanAcceptChild(const std::shared_ptr<Instance>& child) const {
    static_cast<void>(child);
    return true;
}

std::shared_ptr<Instance> Instance::GetParent() const {
    return parent_.lock();
}

void Instance::SetParent(const std::shared_ptr<Instance>& newParent) {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Instance::SetParent");
    }
    const auto currentParent = GetParent();
    if (currentParent == newParent) {
        return;
    }

    if (newParent != nullptr && !CanParentTo(newParent)) {
        throw std::runtime_error("Cannot parent to target parent.");
    }
    if (newParent != nullptr && !newParent->CanAcceptChild(shared_from_this())) {
        throw std::runtime_error("New parent cannot accept this child.");
    }

    const auto oldDataModel = GetDataModel();

    if (currentParent != nullptr) {
        auto& siblings = currentParent->children_;
        siblings.erase(
            std::remove_if(
                siblings.begin(),
                siblings.end(),
                [this](const std::shared_ptr<Instance>& child) { return child.get() == this; }
            ),
            siblings.end()
        );
        currentParent->ChildRemoved.Fire(shared_from_this());
    }

    parent_ = newParent;

    if (newParent != nullptr) {
        newParent->children_.push_back(shared_from_this());
        newParent->ChildAdded.Fire(shared_from_this());
    }

    PropagateAncestryChanged();

    const auto newDataModel = GetDataModel();
    if (oldDataModel != newDataModel) {
        UpdateRegistryRecursive(oldDataModel, newDataModel);
    }
}

std::vector<std::shared_ptr<Instance>> Instance::GetChildren() const {
    return children_;
}

std::vector<std::shared_ptr<Instance>> Instance::GetDescendants() const {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Instance::GetDescendants");
    }
    std::vector<std::shared_ptr<Instance>> result;

    struct Frame {
        const Instance* InstancePtr{nullptr};
        std::size_t NextChild{0};
    };
    std::vector<Frame> stack;
    stack.reserve(32);
    stack.push_back(Frame{.InstancePtr = this, .NextChild = 0});

    while (!stack.empty()) {
        auto& frame = stack.back();
        if (frame.InstancePtr == nullptr) {
            stack.pop_back();
            continue;
        }

        const auto& children = frame.InstancePtr->children_;
        if (frame.NextChild >= children.size()) {
            stack.pop_back();
            continue;
        }

        std::shared_ptr<Instance> child = children[frame.NextChild++];
        if (child == nullptr) {
            continue;
        }

        result.push_back(child);

        if (!child->children_.empty()) {
            stack.push_back(Frame{.InstancePtr = child.get(), .NextChild = 0});
        }
    }
    return result;
}

void Instance::ForEachDescendant(const std::function<void(const std::shared_ptr<Instance>&)>& visitor) const {
    if (Utils::Benchmark::Enabled()) {
        LVS_BENCH_SCOPE("Instance::ForEachDescendant");
    }
    if (!visitor) {
        return;
    }

    struct Frame {
        const Instance* InstancePtr{nullptr};
        std::size_t NextChild{0};
    };
    std::vector<Frame> stack;
    stack.reserve(32);
    stack.push_back(Frame{.InstancePtr = this, .NextChild = 0});

    while (!stack.empty()) {
        auto& frame = stack.back();
        if (frame.InstancePtr == nullptr) {
            stack.pop_back();
            continue;
        }

        const auto& children = frame.InstancePtr->children_;
        if (frame.NextChild >= children.size()) {
            stack.pop_back();
            continue;
        }

        std::shared_ptr<Instance> child = children[frame.NextChild++];
        if (child == nullptr) {
            continue;
        }

        visitor(child);

        if (!child->children_.empty()) {
            stack.push_back(Frame{.InstancePtr = child.get(), .NextChild = 0});
        }
    }
}

std::shared_ptr<DataModel::DataModel> Instance::GetDataModel() {
    auto current = shared_from_this();
    while (current != nullptr) {
        if (const auto dataModel = std::dynamic_pointer_cast<DataModel::DataModel>(current); dataModel != nullptr) {
            return dataModel;
        }
        current = current->GetParent();
    }
    return nullptr;
}

std::shared_ptr<const DataModel::DataModel> Instance::GetDataModel() const {
    auto current = std::const_pointer_cast<Instance>(
        std::static_pointer_cast<const Instance>(shared_from_this())
    );
    while (current != nullptr) {
        if (const auto dataModel = std::dynamic_pointer_cast<DataModel::DataModel>(current); dataModel != nullptr) {
            return dataModel;
        }
        current = current->GetParent();
    }
    return nullptr;
}

void Instance::Destroy() {
    Destroying.Fire();

    const auto childrenCopy = children_;
    for (const auto& child : childrenCopy) {
        child->Destroy();
    }

    SetParent(nullptr);
    children_.clear();
}

void Instance::SetServiceFlags(const bool isService, const bool isHiddenService) {
    isService_ = isService;
    isHiddenService_ = isHiddenService;
}

void Instance::SetInsertable(const bool isInsertable) {
    isInsertable_ = isInsertable;
}

void Instance::PropagateAncestryChanged() {
    AncestryChanged.Fire(GetParent());
    for (const auto& child : children_) {
        child->PropagateAncestryChanged();
    }
}

void Instance::UpdateRegistryRecursive(
    const std::shared_ptr<DataModel::DataModel>& oldDataModel,
    const std::shared_ptr<DataModel::DataModel>& newDataModel
) {
    if (oldDataModel != nullptr) {
        oldDataModel->UnregisterInstance(shared_from_this());
    }
    if (newDataModel != nullptr) {
        newDataModel->RegisterInstance(shared_from_this());
    }

    for (const auto& child : children_) {
        child->UpdateRegistryRecursive(oldDataModel, newDataModel);
    }
}

} // namespace Lvs::Engine::Core
