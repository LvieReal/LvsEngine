#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"

namespace Lvs::Engine::Objects {

class BasePart : public Core::Instance {
public:
    BasePart();
    ~BasePart() override = default;

    static Core::ClassDescriptor& Descriptor();

    [[nodiscard]] Math::CFrame GetWorldCFrame();
    [[nodiscard]] Math::Vector3 GetWorldPosition();
    [[nodiscard]] Math::Vector3 GetWorldRotation();

    void SetProperty(const QString& name, const QVariant& value) override;

private:
    void MarkTransformDirty();
    void RecomputeWorldCFrame();
    void SetCFrame(const Math::CFrame& cframe);
    void SetPosition(const Math::Vector3& position);
    void SetRotation(const Math::Vector3& rotation);

    Math::CFrame worldCFrame_{Math::CFrame::Identity()};
    bool transformDirty_{true};

protected:
    explicit BasePart(const Core::ClassDescriptor& descriptor);
};

} // namespace Lvs::Engine::Objects
