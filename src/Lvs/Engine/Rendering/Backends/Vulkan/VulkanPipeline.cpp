#include "Lvs/Engine/Rendering/Backends/Vulkan/VulkanPipeline.hpp"

namespace Lvs::Engine::Rendering::Backends::Vulkan {

VulkanPipeline::VulkanPipeline(
    RHI::PipelineDesc desc,
    void* handle,
    std::function<void(void*)> destroy
)
    : desc_(desc),
      handle_(handle),
      destroy_(destroy) {}

VulkanPipeline::~VulkanPipeline() {
    if (destroy_ != nullptr && handle_ != nullptr) {
        destroy_(handle_);
        handle_ = nullptr;
    }
}

void* VulkanPipeline::GetNativeHandle() const {
    return handle_;
}

const RHI::PipelineDesc& VulkanPipeline::GetDesc() const {
    return desc_;
}

} // namespace Lvs::Engine::Rendering::Backends::Vulkan
