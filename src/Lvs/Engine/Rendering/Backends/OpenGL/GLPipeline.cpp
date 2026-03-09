#include "Lvs/Engine/Rendering/Backends/OpenGL/GLPipeline.hpp"

#include <cstdint>
#include <utility>

namespace Lvs::Engine::Rendering::Backends::OpenGL {

GLPipeline::GLPipeline(
    const RHI::PipelineDesc desc,
    const unsigned int programHandle,
    std::function<void(unsigned int)> destroy
)
    : desc_(desc),
      programHandle_(programHandle),
      destroy_(std::move(destroy)) {}

GLPipeline::~GLPipeline() {
    if (destroy_ != nullptr && programHandle_ != 0U) {
        destroy_(programHandle_);
        programHandle_ = 0U;
    }
}

void* GLPipeline::GetNativeHandle() const {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(programHandle_));
}

const RHI::PipelineDesc& GLPipeline::GetDesc() const {
    return desc_;
}

} // namespace Lvs::Engine::Rendering::Backends::OpenGL
