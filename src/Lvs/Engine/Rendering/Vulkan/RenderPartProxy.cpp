#include "Lvs/Engine/Rendering/Vulkan/RenderPartProxy.hpp"

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Mesh.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Renderer.hpp"

#include <utility>

namespace Lvs::Engine::Rendering::Vulkan {

RenderPartProxy::RenderPartProxy(std::shared_ptr<Objects::BasePart> part)
    : instance_(std::move(part)) {
    if (instance_ != nullptr) {
        propertyChangedConnection_ = instance_->PropertyChanged.Connect(
            [this](const QString& propertyName, const QVariant&) { MarkDirty(propertyName); }
        );
        ancestryChangedConnection_ = instance_->AncestryChanged.Connect(
            [this](const std::shared_ptr<Core::Instance>&) { dirty_ = true; }
        );
    }
}

RenderPartProxy::~RenderPartProxy() {
    if (propertyChangedConnection_.has_value()) {
        propertyChangedConnection_->Disconnect();
        propertyChangedConnection_.reset();
    }
    if (ancestryChangedConnection_.has_value()) {
        ancestryChangedConnection_->Disconnect();
        ancestryChangedConnection_.reset();
    }
}

void RenderPartProxy::SyncFromInstance(Renderer& renderer) {
    if (instance_ == nullptr) {
        mesh_.reset();
        return;
    }

    // Keep model matrix in sync even when only an ancestor moved.
    const auto world = instance_->GetWorldCFrame().ToMatrix4();
    const auto size = Math::Matrix4::Scale(instance_->GetProperty("Size").value<Math::Vector3>());
    modelMatrix_ = world * size;

    if (!dirty_) {
        return;
    }

    mesh_ = renderer.GetMeshCache().Get(instance_);
    color_ = instance_->GetProperty("Color").value<Math::Color3>();
    alpha_ = static_cast<float>(1.0 - instance_->GetProperty("Transparency").toDouble());
    cullMode_ = instance_->GetProperty("CullMode").value<Enums::MeshCullMode>();
    metalness_ = static_cast<float>(instance_->GetProperty("Metalness").toDouble());
    roughness_ = static_cast<float>(instance_->GetProperty("Roughness").toDouble());
    emissive_ = static_cast<float>(instance_->GetProperty("Emissive").toDouble());

    surfaceEnabled_ = false;
    topSurfaceType_ = 0;
    bottomSurfaceType_ = 0;
    frontSurfaceType_ = 0;
    backSurfaceType_ = 0;
    leftSurfaceType_ = 0;
    rightSurfaceType_ = 0;

    const bool isMeshPart = std::dynamic_pointer_cast<Objects::MeshPart>(instance_) != nullptr;
    if (!isMeshPart) {
        if (const auto part = std::dynamic_pointer_cast<Objects::Part>(instance_); part != nullptr) {
            const auto shape = part->GetProperty("Shape").value<Enums::PartShape>();
            surfaceEnabled_ = shape == Enums::PartShape::Cube;
            if (surfaceEnabled_) {
                topSurfaceType_ = static_cast<int>(part->GetProperty("TopSurface").value<Enums::PartSurfaceType>());
                bottomSurfaceType_ = static_cast<int>(part->GetProperty("BottomSurface").value<Enums::PartSurfaceType>());
                frontSurfaceType_ = static_cast<int>(part->GetProperty("FrontSurface").value<Enums::PartSurfaceType>());
                backSurfaceType_ = static_cast<int>(part->GetProperty("BackSurface").value<Enums::PartSurfaceType>());
                leftSurfaceType_ = static_cast<int>(part->GetProperty("LeftSurface").value<Enums::PartSurfaceType>());
                rightSurfaceType_ = static_cast<int>(part->GetProperty("RightSurface").value<Enums::PartSurfaceType>());
            }
        }
    }

    dirty_ = false;
}

void RenderPartProxy::Draw(const VkCommandBuffer commandBuffer, Renderer& renderer) {
    Draw(commandBuffer, renderer, false);
}

void RenderPartProxy::Draw(const VkCommandBuffer commandBuffer, Renderer& renderer, const bool transparent) {
    if (instance_ == nullptr || mesh_ == nullptr) {
        return;
    }
    if (!instance_->GetProperty("Renders").toBool()) {
        return;
    }
    renderer.DrawPart(commandBuffer, *this, transparent);
}

const std::shared_ptr<Mesh>& RenderPartProxy::GetMesh() const {
    return mesh_;
}

Math::Vector3 RenderPartProxy::GetWorldPosition() const {
    if (instance_ == nullptr) {
        return {};
    }
    return instance_->GetWorldPosition();
}

const Math::Matrix4& RenderPartProxy::GetModelMatrix() const {
    return modelMatrix_;
}

const Math::Color3& RenderPartProxy::GetColor() const {
    return color_;
}

float RenderPartProxy::GetAlpha() const {
    return alpha_;
}

Enums::MeshCullMode RenderPartProxy::GetCullMode() const {
    return cullMode_;
}

float RenderPartProxy::GetMetalness() const {
    return metalness_;
}

float RenderPartProxy::GetRoughness() const {
    return roughness_;
}

float RenderPartProxy::GetEmissive() const {
    return emissive_;
}

bool RenderPartProxy::GetSurfaceEnabled() const {
    return surfaceEnabled_;
}

int RenderPartProxy::GetTopSurfaceType() const {
    return topSurfaceType_;
}

int RenderPartProxy::GetBottomSurfaceType() const {
    return bottomSurfaceType_;
}

int RenderPartProxy::GetFrontSurfaceType() const {
    return frontSurfaceType_;
}

int RenderPartProxy::GetBackSurfaceType() const {
    return backSurfaceType_;
}

int RenderPartProxy::GetLeftSurfaceType() const {
    return leftSurfaceType_;
}

int RenderPartProxy::GetRightSurfaceType() const {
    return rightSurfaceType_;
}

const std::shared_ptr<Objects::BasePart>& RenderPartProxy::GetInstance() const {
    return instance_;
}

void RenderPartProxy::MarkDirty(const QString& propertyName) {
    static_cast<void>(propertyName);
    dirty_ = true;
}

} // namespace Lvs::Engine::Rendering::Vulkan
