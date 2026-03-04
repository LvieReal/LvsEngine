#pragma once

#include "Lvs/Engine/Core/ObjectBase.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <QMetaType>
#include <QVariant>

#include <memory>
#include <vector>

namespace Lvs::Engine::DataModel {
class DataModel;
}

namespace Lvs::Engine::Core {

class Instance : public ObjectBase, public std::enable_shared_from_this<Instance> {
public:
    Instance();
    ~Instance() override = default;

    static ClassDescriptor& Descriptor();

    [[nodiscard]] const QString& GetId() const;
    [[nodiscard]] QString GetClassName() const;
    [[nodiscard]] QString GetFullPath() const;

    [[nodiscard]] virtual bool IsService() const;
    [[nodiscard]] virtual bool IsHiddenService() const;
    [[nodiscard]] virtual bool IsInsertable() const;

    virtual void SetProperty(const QString& name, const QVariant& value);

    virtual bool CanParentTo(const std::shared_ptr<Instance>& parent) const;
    virtual bool CanAcceptChild(const std::shared_ptr<Instance>& child) const;

    [[nodiscard]] std::shared_ptr<Instance> GetParent() const;
    void SetParent(const std::shared_ptr<Instance>& newParent);

    [[nodiscard]] std::vector<std::shared_ptr<Instance>> GetChildren() const;
    [[nodiscard]] std::vector<std::shared_ptr<Instance>> GetDescendants() const;

    std::shared_ptr<DataModel::DataModel> GetDataModel();
    std::shared_ptr<const DataModel::DataModel> GetDataModel() const;

    virtual void Destroy();

    Utils::Signal<const QString&, const QVariant&> PropertyChanged;
    Utils::Signal<const std::shared_ptr<Instance>&> ChildAdded;
    Utils::Signal<const std::shared_ptr<Instance>&> ChildRemoved;
    Utils::Signal<const std::shared_ptr<Instance>&> AncestryChanged;
    Utils::Signal<> Destroying;

protected:
    explicit Instance(const ClassDescriptor& descriptor);
    void SetServiceFlags(bool isService, bool isHiddenService);
    void SetInsertable(bool isInsertable);

private:
    void PropagateAncestryChanged();
    void UpdateRegistryRecursive(
        const std::shared_ptr<DataModel::DataModel>& oldDataModel,
        const std::shared_ptr<DataModel::DataModel>& newDataModel
    );

    QString id_;
    std::weak_ptr<Instance> parent_;
    std::vector<std::shared_ptr<Instance>> children_;
    bool isService_{false};
    bool isHiddenService_{false};
    bool isInsertable_{false};
};

} // namespace Lvs::Engine::Core

Q_DECLARE_METATYPE(std::shared_ptr<Lvs::Engine::Core::Instance>)
