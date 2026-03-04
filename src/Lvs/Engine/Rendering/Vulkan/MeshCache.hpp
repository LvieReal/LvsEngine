#pragma once

#include "Lvs/Engine/Enums/PartShape.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <unordered_map>

class QString;

namespace Lvs::Engine::Objects {
class BasePart;
class MeshPart;
}

namespace Lvs::Engine::Rendering::Vulkan {

class Mesh;

class MeshCache final {
public:
    MeshCache() = default;
    ~MeshCache() = default;

    void Initialize();
    void Destroy(VkDevice device);

    std::shared_ptr<Mesh> Get(const std::shared_ptr<Objects::BasePart>& part);
    std::shared_ptr<Mesh> GetPrimitive(Enums::PartShape shape);

private:
    std::shared_ptr<Mesh> GetByShape(Enums::PartShape shape);
    std::shared_ptr<Mesh> GetMeshPart(const std::shared_ptr<Objects::MeshPart>& meshPart);

    bool initialized_{false};
    std::unordered_map<int, std::shared_ptr<Mesh>> primitiveMeshes_;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> meshPartMeshes_;
};

} // namespace Lvs::Engine::Rendering::Vulkan
