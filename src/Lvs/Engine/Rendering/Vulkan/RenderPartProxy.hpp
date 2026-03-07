#pragma once

#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/CommandBuffer.hpp"
#include "Lvs/Engine/Rendering/Common/SceneRenderer.hpp"
#include "Lvs/Engine/Rendering/Common/MeshUploader.hpp"
#include "Lvs/Engine/Rendering/Common/RenderProxy.hpp"

#include <memory>
#include <optional>

namespace Lvs::Engine::Objects {
class BasePart;
}

namespace Lvs::Engine::Core {
class Instance;
}

namespace Lvs::Engine::Rendering::Vulkan {

class Mesh;
class Renderer;

class RenderPartProxy final : public Common::RenderProxy {
public:
    explicit RenderPartProxy(std::shared_ptr<Objects::BasePart> part);
    ~RenderPartProxy() override;

    void SyncFromRenderer(Common::SceneRenderer& renderer) override;
    void Draw(Common::CommandBuffer& commandBuffer, Common::SceneRenderer& renderer) override;
    void Draw(Common::CommandBuffer& commandBuffer, Common::SceneRenderer& renderer, bool transparent);

    [[nodiscard]] const std::shared_ptr<Common::UploadedMesh>& GetMesh() const;
    [[nodiscard]] Math::Vector3 GetWorldPosition() const;
    [[nodiscard]] const Math::Matrix4& GetModelMatrix() const;
    [[nodiscard]] const Math::Color3& GetColor() const;
    [[nodiscard]] float GetAlpha() const;
    [[nodiscard]] Enums::MeshCullMode GetCullMode() const;
    [[nodiscard]] float GetMetalness() const;
    [[nodiscard]] float GetRoughness() const;
    [[nodiscard]] float GetEmissive() const;
    [[nodiscard]] bool GetSurfaceEnabled() const;
    [[nodiscard]] int GetTopSurfaceType() const;
    [[nodiscard]] int GetBottomSurfaceType() const;
    [[nodiscard]] int GetFrontSurfaceType() const;
    [[nodiscard]] int GetBackSurfaceType() const;
    [[nodiscard]] int GetLeftSurfaceType() const;
    [[nodiscard]] int GetRightSurfaceType() const;
    [[nodiscard]] const std::shared_ptr<Objects::BasePart>& GetInstance() const;

private:
    void MarkDirty();

    std::shared_ptr<Objects::BasePart> instance_;
    std::shared_ptr<Common::UploadedMesh> mesh_;
    Math::Matrix4 modelMatrix_{Math::Matrix4::Identity()};
    Math::Color3 color_{};
    float alpha_{1.0F};
    Enums::MeshCullMode cullMode_{Enums::MeshCullMode::Back};
    float metalness_{0.0F};
    float roughness_{0.5F};
    float emissive_{0.0F};
    bool surfaceEnabled_{false};
    int topSurfaceType_{0};
    int bottomSurfaceType_{0};
    int frontSurfaceType_{0};
    int backSurfaceType_{0};
    int leftSurfaceType_{0};
    int rightSurfaceType_{0};
    bool dirty_{true};
    std::optional<Engine::Core::Instance::PropertyInvalidatedConnection> propertyChangedConnection_;
    std::optional<Engine::Core::Instance::InstanceConnection> ancestryChangedConnection_;
};

} // namespace Lvs::Engine::Rendering::Vulkan
