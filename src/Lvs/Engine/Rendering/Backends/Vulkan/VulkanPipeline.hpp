#pragma once

#include "Lvs/Engine/Rendering/RHI/IPipeline.hpp"

#include <functional>

namespace Lvs::Engine::Rendering::Backends::Vulkan {

class VulkanPipeline final : public RHI::IPipeline {
public:
    explicit VulkanPipeline(
        RHI::PipelineDesc desc,
        void* handle = nullptr,
        std::function<void(void*)> destroy = nullptr
    );
    ~VulkanPipeline() override;
    [[nodiscard]] void* GetNativeHandle() const override;
    [[nodiscard]] const RHI::PipelineDesc& GetDesc() const;

private:
    RHI::PipelineDesc desc_{};
    void* handle_{nullptr};
    std::function<void(void*)> destroy_{};
};

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
