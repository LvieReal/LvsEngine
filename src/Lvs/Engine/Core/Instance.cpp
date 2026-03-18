#include "Lvs/Engine/Core/Instance.hpp"

#include "Lvs/Engine/DataModel/DataModel.hpp"

#include <QUuid>

#include <algorithm>
#include <stdexcept>

namespace Lvs::Engine::Core {

namespace {
ClassDescriptor& BaseDescriptor() {
    static ClassDescriptor descriptor("Instance", nullptr);
    static const bool initialized = []() {
        descriptor.RegisterProperty(
            ObjectBase::MakePropertyDefinition<QString>("Name", QString{})
        );
        descriptor.RegisterProperty(
            ObjectBase::MakePropertyDefinition<QString>(
                "ClassName",
                QString{},
                true,
                "Data",
                {},
                true
            )
        );
        ClassDescriptor::RegisterClassDescriptor(&descriptor);
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}
} // namespace

Instance::Instance()
    : Instance(Descriptor()) {
}

Instance::Instance(const ClassDescriptor& descriptor)
    : ObjectBase(descriptor),
      id_(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    const QString className = GetClassName();
    SetProperty("Name", className);
    SetProperty("ClassName", className);
}

ClassDescriptor& Instance::Descriptor() {
    return BaseDescriptor();
}

const QString& Instance::GetId() const {
    return id_;
}

QString Instance::GetClassName() const {
    return GetClassDescriptor().ClassName();
}

QString Instance::GetFullPath() const {
    const auto parent = GetParent();
    if (parent == nullptr) {
        return GetClassName();
    }
    return QString("%1.%2").arg(parent->GetFullPath(), GetClassName());
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

void Instance::SetProperty(const QString& name, const QVariant& value) {
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
