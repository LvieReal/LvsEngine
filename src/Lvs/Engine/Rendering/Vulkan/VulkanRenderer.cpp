#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderer.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Rendering/Common/Mesh.hpp"
#include "Lvs/Engine/Rendering/Common/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBinding.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanMeshUploader.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Rendering/Vulkan/PostProcessRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/ShadowRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/SkyboxRenderer.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanVertexLayout.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderManifest.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

void Normalize3(float values[3]) {
    const float length = std::sqrt(values[0] * values[0] + values[1] * values[1] + values[2] * values[2]);
    if (length <= 1e-6F) {
        values[0] = 0.0F;
        values[1] = -1.0F;
        values[2] = 0.0F;
        return;
    }
    values[0] /= length;
    values[1] /= length;
    values[2] /= length;
}

Common::PipelineCullMode ToPipelineCullMode(const Enums::MeshCullMode cullMode) {
    switch (cullMode) {
        case Enums::MeshCullMode::NoCull:
            return Common::PipelineCullMode::None;
        case Enums::MeshCullMode::Front:
            return Common::PipelineCullMode::Front;
        case Enums::MeshCullMode::Back:
        default:
            return Common::PipelineCullMode::Back;
    }
}

VkCullModeFlags ToVkCullMode(const Common::PipelineCullMode cullMode) {
    switch (cullMode) {
        case Common::PipelineCullMode::None:
            return VK_CULL_MODE_NONE;
        case Common::PipelineCullMode::Front:
            return VK_CULL_MODE_FRONT_BIT;
        case Common::PipelineCullMode::Back:
        default:
            return VK_CULL_MODE_BACK_BIT;
    }
}

} // namespace

VulkanRenderer::VulkanRenderer(std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory)
    : Common::Renderer(std::move(factory), std::make_unique<VulkanMeshUploader>()),
      pipelineManifest_(std::make_shared<VulkanPipelineManifestProvider>()),
      resourceRegistry_(std::make_shared<VulkanRenderResourceRegistry>()) {}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::OnInitialize(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    auto& vkContext = static_cast<VulkanContext&>(context);

    bindingLayout_ = CreateBindingLayout(vkContext);
    CreatePipelineLayout(vkContext);
    uniformBuffers_ = AllocateUniformBuffers(vkContext);
    bindings_ = CreateResourceBindings(vkContext);
    CreateGraphicsPipelines(vkContext);
    InitializeSubRenderers(vkContext);
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->Initialize(context, surface);
    }
    CreateSurfaceTextures(vkContext);
}

void VulkanRenderer::OnRecreateSwapchain(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    OnDestroySwapchainResources(context, surface);
    CreateGraphicsPipelines(vkContext);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->RecreateSwapchain(context);
    }
    if (shadowRenderer_ != nullptr) {
        shadowRenderer_->RecreateSwapchain(context);
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->RecreateSwapchain(context, surface);
    }
}

void VulkanRenderer::OnDestroySwapchainResources(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    static_cast<void>(surface);
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->DestroySwapchainResources(context);
    }
    pipelines_.clear();
}

void VulkanRenderer::OnShutdown(Common::GraphicsContext& context) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    const auto* surface = GetCurrentSurface();
    if (surface != nullptr) {
        OnDestroySwapchainResources(context, *surface);
    } else {
        pipelines_.clear();
    }

    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->Shutdown(context);
    }
    if (shadowRenderer_ != nullptr) {
        shadowRenderer_->Shutdown(context);
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->Shutdown(context);
    }
    DestroySurfaceTextures(vkContext);

    uniformBuffers_.clear();
    bindings_.clear();
    boundSkyImageView_ = VK_NULL_HANDLE;
    boundSurfaceAtlasView_ = VK_NULL_HANDLE;
    boundSurfaceNormalAtlasView_ = VK_NULL_HANDLE;

    pipelineLayout_.reset();
    bindingLayout_.reset();
}

void VulkanRenderer::OnBindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    scene_.Build(place);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->BindToPlace(place);
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->BindToPlace(place);
    }
}

void VulkanRenderer::OnUnbind() {
    scene_.Build(nullptr);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->Unbind();
    }
    if (postProcessRenderer_ != nullptr) {
        postProcessRenderer_->Unbind();
    }
}

void VulkanRenderer::OnOverlayPrimitivesChanged(const std::vector<Common::OverlayPrimitive>& primitives) {
    static_cast<void>(primitives);
}

void VulkanRenderer::OnRecordShadowCommands(
    Common::GraphicsContext& context,
    const Common::RenderSurface& surface,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex
) {
    static_cast<void>(surface);
    static_cast<void>(frameIndex);
    auto& vkContext = static_cast<VulkanContext&>(context);
    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }
    camera->Resize(GetAspect(vkContext));

    settingsSnapshot_ = settingsResolver_.Resolve(GetPlace());
    const auto cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
    scene_.SyncProxies(*this);
    frameRenderData_ = policyResolver_.Resolve(scene_.GetRenderProxies(), cameraPosition);
    if (!settingsSnapshot_.DirectionalLightEnabled) {
        settingsSnapshot_.Shadow.Enabled = false;
    }

    if (shadowRenderer_ != nullptr) {
        shadowRenderer_->Render(context, commandBuffer, Common::ShadowRenderer::ShadowPassInput{
                                                        .Casters = &frameRenderData_.ShadowCasters,
                                                        .Camera = camera.get(),
                                                        .DirectionalLightDirection = settingsSnapshot_.DirectionalLightDirection,
                                                        .CameraAspect = GetAspect(vkContext),
                                                        .Quality = settingsSnapshot_.Shadow
                                                    });
    }
}

void VulkanRenderer::OnRecordDrawCommands(
    Common::GraphicsContext& context,
    const Common::RenderSurface& surface,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex
) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    const Common::FrameContext frameContext{
        .Graphics = context,
        .Surface = surface,
        .Commands = commandBuffer,
        .FrameIndex = frameIndex
    };
    if (pipelines_.empty()) {
        return;
    }

    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }
    camera->Resize(GetAspect(vkContext));

    const auto extentInfo = frameContext.Surface.GetExtent();
    const VkExtent2D extent{extentInfo.Width, extentInfo.Height};
    frameContext.Commands.SetViewport({
        .X = 0.0F,
        .Y = 0.0F,
        .Width = static_cast<float>(extent.width),
        .Height = static_cast<float>(extent.height),
        .MinDepth = 0.0F,
        .MaxDepth = 1.0F
    });
    frameContext.Commands.SetScissor({
        .X = 0,
        .Y = 0,
        .Width = extent.width,
        .Height = extent.height
    });

    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->UpdateResources(context);
    }
    UpdateMainSkyDescriptorSets(vkContext);
    UpdateShadowDescriptorSets(vkContext);
    UpdateSurfaceDescriptorSets(vkContext);
    UpdateCameraUniformAndLighting(vkContext, frameContext);

    GetResourceBinding(frameContext.FrameIndex).Bind(frameContext.Commands, *pipelineLayout_, 0);

    for (const auto& proxy : frameRenderData_.OpaqueDraws) {
        proxy->Draw(frameContext.Commands, *this);
    }
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->Draw(context, frameContext.Commands, frameContext.FrameIndex, *camera);
    }
    for (const auto& proxy : frameRenderData_.TransparentDraws) {
        if (proxy != nullptr) {
            DrawRenderProxy(frameContext.Commands, *proxy, true);
        }
    }
    for (const auto& primitive : GetOverlayPrimitives()) {
        DrawOverlayPrimitive(frameContext.Commands, primitive);
    }
}

void VulkanRenderer::DrawRenderProxy(Common::CommandBuffer& commandBuffer, const Common::RenderProxy& proxy, const bool transparent) {
    const auto* partProxy = dynamic_cast<const Common::RenderPartProxy*>(&proxy);
    if (partProxy == nullptr) {
        throw std::runtime_error("VulkanRenderer received an unsupported render proxy type.");
    }
    DrawPart(commandBuffer, *partProxy, transparent);
}

void VulkanRenderer::RecordFrameCommands(
    Common::GraphicsContext& context,
    const Common::RenderSurface& surface,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t imageIndex,
    const std::uint32_t frameIndex,
    const std::array<float, 4>& clearColor
) {
    auto& vkCommandBuffer = dynamic_cast<VulkanRenderCommandBuffer&>(commandBuffer);
    const VkCommandBuffer handle = vkCommandBuffer.GetHandle();

    OnRecordShadowCommands(context, surface, commandBuffer, frameIndex);

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}};
    clearValues[1].color = {{0.0F, 0.0F, 0.0F, 0.0F}};
    clearValues[2].depthStencil = {.depth = 0.0F, .stencil = 0};
    const auto extent = surface.GetExtent();
    const VkRenderPassBeginInfo scenePassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = reinterpret_cast<VkRenderPass>(surface.GetSceneRenderPass().GetNativeHandle()),
        .framebuffer = reinterpret_cast<VkFramebuffer>(surface.GetSceneFramebuffer(imageIndex).GetNativeHandle()),
        .renderArea = {.offset = {0, 0}, .extent = {extent.Width, extent.Height}},
        .clearValueCount = static_cast<std::uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()
    };
    vkCmdBeginRenderPass(handle, &scenePassInfo, VK_SUBPASS_CONTENTS_INLINE);
    OnRecordDrawCommands(context, surface, commandBuffer, frameIndex);
    vkCmdEndRenderPass(handle);

    const VkImageMemoryBarrier sceneToSampleBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = reinterpret_cast<VkImage>(surface.GetOffscreenColorImage(imageIndex).Image),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    const VkImageMemoryBarrier glowToSampleBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = reinterpret_cast<VkImage>(surface.GetOffscreenGlowImage(imageIndex).Image),
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    const std::array barriers{sceneToSampleBarrier, glowToSampleBarrier};
    vkCmdPipelineBarrier(
        handle,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        static_cast<std::uint32_t>(barriers.size()),
        barriers.data()
    );

    if (postProcessRenderer_ == nullptr) {
        return;
    }

    postProcessRenderer_->RecordBlurCommands(context, commandBuffer, imageIndex);

    const VkClearValue postClearValue{.color = {{clearColor[0], clearColor[1], clearColor[2], clearColor[3]}}};
    const VkRenderPassBeginInfo postPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = reinterpret_cast<VkRenderPass>(surface.GetPostProcessRenderPass().GetNativeHandle()),
        .framebuffer = reinterpret_cast<VkFramebuffer>(surface.GetSwapchainFramebuffer(imageIndex).GetNativeHandle()),
        .renderArea = {.offset = {0, 0}, .extent = {extent.Width, extent.Height}},
        .clearValueCount = 1,
        .pClearValues = &postClearValue
    };
    vkCmdBeginRenderPass(handle, &postPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    commandBuffer.SetViewport({
        .X = 0.0F,
        .Y = 0.0F,
        .Width = static_cast<float>(extent.Width),
        .Height = static_cast<float>(extent.Height),
        .MinDepth = 0.0F,
        .MaxDepth = 1.0F
    });
    commandBuffer.SetScissor({
        .X = 0,
        .Y = 0,
        .Width = extent.Width,
        .Height = extent.Height
    });
    postProcessRenderer_->DrawComposite(context, commandBuffer, imageIndex, frameIndex);
    vkCmdEndRenderPass(handle);
}

std::unique_ptr<Common::BindingLayout> VulkanRenderer::CreateBindingLayout(VulkanContext& context) const {
    const VkDescriptorSetLayoutBinding cameraBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const VkDescriptorSetLayoutBinding skyBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const VkDescriptorSetLayoutBinding shadowBinding{
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 3,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const VkDescriptorSetLayoutBinding shadowJitterBinding{
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const VkDescriptorSetLayoutBinding surfaceAtlasBinding{
        .binding = 4,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const VkDescriptorSetLayoutBinding surfaceNormalAtlasBinding{
        .binding = 5,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const std::array bindings{
        cameraBinding,
        skyBinding,
        shadowBinding,
        shadowJitterBinding,
        surfaceAtlasBinding,
        surfaceNormalAtlasBinding
    };

    const VkDescriptorSetLayoutCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    const std::array<VkDescriptorPoolSize, 2> poolSizes{{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = context.GetFramesInFlight()},
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = context.GetFramesInFlight() * 7
        }
    }};
    return VulkanBindingLayout::Create(
        context.GetDevice(),
        createInfo,
        std::vector<VkDescriptorPoolSize>(poolSizes.begin(), poolSizes.end()),
        context.GetFramesInFlight()
    );
}

void VulkanRenderer::CreatePipelineLayout(VulkanContext& context) {
    const auto& bindingLayout = dynamic_cast<const VulkanBindingLayout&>(*bindingLayout_);
    const VkDescriptorSetLayout descriptorSetLayout = bindingLayout.GetLayoutHandle();
    const VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    const VkPipelineLayoutCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    pipelineLayout_ = VulkanPipelineLayout::Create(context.GetDevice(), createInfo);
}

void VulkanRenderer::CreateGraphicsPipelines(VulkanContext& context) {
    pipelines_.clear();
    for (const auto& key : GetPipelineVariants()) {
        pipelines_.emplace(key, CreateGraphicsPipelineVariant(context, key));
    }
}

std::unique_ptr<Common::Pipeline> VulkanRenderer::CreateGraphicsPipelineVariant(
    VulkanContext& context,
    const Common::PipelineVariantKey& key
) {
    const VkDevice device = context.GetDevice();
    const auto vertPath = pipelineManifest_->GetShaderPath("main", Common::ShaderStage::Vertex);
    const auto fragPath = pipelineManifest_->GetShaderPath("main", Common::ShaderStage::Fragment);

    const auto vertCode = ShaderUtils::ReadBinaryFile(vertPath);
    const auto fragCode = ShaderUtils::ReadBinaryFile(fragPath);
    const auto vertModule = VulkanShaderModule::Create(device, vertCode);
    const auto fragModule = VulkanShaderModule::Create(device, fragCode);

    const VkPipelineShaderStageCreateInfo vertStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertModule->GetHandle(),
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    const VkPipelineShaderStageCreateInfo fragStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragModule->GetHandle(),
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    const VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    const auto binding = VulkanVertexLayout::BindingDescription();
    const auto attributes = VulkanVertexLayout::AttributeDescriptions();
    const VkPipelineVertexInputStateCreateInfo vertexInput{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size()),
        .pVertexAttributeDescriptions = attributes.data()
    };
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    const VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };
    const VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = ToVkCullMode(key.CullMode),
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0F,
        .depthBiasClamp = 0.0F,
        .depthBiasSlopeFactor = 0.0F,
        .lineWidth = 1.0F
    };
    const VkPipelineMultisampleStateCreateInfo multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0F,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };
    const VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = key.DepthMode == Common::PipelineDepthMode::Disabled ? VK_FALSE : VK_TRUE,
        .depthWriteEnable = key.DepthMode == Common::PipelineDepthMode::ReadWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp = key.DepthMode == Common::PipelineDepthMode::Disabled ? VK_COMPARE_OP_ALWAYS
                                                                                : VK_COMPARE_OP_GREATER_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F
    };
    const VkPipelineColorBlendAttachmentState sceneBlendAttachment{
        .blendEnable = key.BlendMode == Common::PipelineBlendMode::AlphaBlend ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT
    };
    const VkPipelineColorBlendAttachmentState glowBlendAttachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                          VK_COLOR_COMPONENT_A_BIT
    };
    const std::array blendAttachments{sceneBlendAttachment, glowBlendAttachment};
    const VkPipelineColorBlendStateCreateInfo colorBlend{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = static_cast<std::uint32_t>(blendAttachments.size()),
        .pAttachments = blendAttachments.data(),
        .blendConstants = {0.0F, 0.0F, 0.0F, 0.0F}
    };
    const std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    const auto* surface = GetCurrentSurface();
    const VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamicState,
        .layout = reinterpret_cast<VkPipelineLayout>(pipelineLayout_->GetNativeHandle()),
        .renderPass = surface != nullptr ? reinterpret_cast<VkRenderPass>(surface->GetSceneRenderPass().GetNativeHandle())
                                          : VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    return VulkanPipelineVariant::CreateGraphicsPipeline(device, pipelineInfo, *pipelineLayout_);
}

std::vector<std::unique_ptr<Common::BufferResource>> VulkanRenderer::AllocateUniformBuffers(VulkanContext& context) const {
    std::vector<std::unique_ptr<Common::BufferResource>> uniformBuffers;
    uniformBuffers.reserve(context.GetFramesInFlight());
    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        uniformBuffers.push_back(context.CreateBuffer(Common::BufferDesc{
            .Size = sizeof(CameraUniform),
            .Usage = Common::BufferUsage::Uniform,
            .Memory = Common::MemoryUsage::CpuVisible
        }));
    }
    return uniformBuffers;
}

std::vector<std::unique_ptr<Common::ResourceBinding>> VulkanRenderer::CreateResourceBindings(VulkanContext& context) const {
    const auto& bindingLayout = dynamic_cast<const VulkanBindingLayout&>(*bindingLayout_);
    std::vector<std::unique_ptr<Common::ResourceBinding>> bindings;
    bindings.reserve(context.GetFramesInFlight());

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        bindings.push_back(bindingLayout.AllocateBinding());
        bindings.back()->UpdateBuffer(0, *uniformBuffers_[i], 0, sizeof(CameraUniform));
    }
    return bindings;
}

std::vector<Common::PipelineVariantKey> VulkanRenderer::GetPipelineVariants() const {
    return {
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::Back,
                                   .DepthMode = Common::PipelineDepthMode::ReadWrite,
                                   .BlendMode = Common::PipelineBlendMode::Opaque},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::Front,
                                   .DepthMode = Common::PipelineDepthMode::ReadWrite,
                                   .BlendMode = Common::PipelineBlendMode::Opaque},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::None,
                                   .DepthMode = Common::PipelineDepthMode::ReadWrite,
                                   .BlendMode = Common::PipelineBlendMode::Opaque},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::Back,
                                   .DepthMode = Common::PipelineDepthMode::ReadOnly,
                                   .BlendMode = Common::PipelineBlendMode::AlphaBlend},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::Front,
                                   .DepthMode = Common::PipelineDepthMode::ReadOnly,
                                   .BlendMode = Common::PipelineBlendMode::AlphaBlend},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::None,
                                   .DepthMode = Common::PipelineDepthMode::ReadOnly,
                                   .BlendMode = Common::PipelineBlendMode::AlphaBlend},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::Back,
                                   .DepthMode = Common::PipelineDepthMode::Disabled,
                                   .BlendMode = Common::PipelineBlendMode::AlphaBlend},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::Front,
                                   .DepthMode = Common::PipelineDepthMode::Disabled,
                                   .BlendMode = Common::PipelineBlendMode::AlphaBlend},
        Common::PipelineVariantKey{.CullMode = Common::PipelineCullMode::None,
                                   .DepthMode = Common::PipelineDepthMode::Disabled,
                                   .BlendMode = Common::PipelineBlendMode::AlphaBlend}
    };
}

void VulkanRenderer::InitializeSubRenderers(VulkanContext& context) {
    const auto& factory = GetFactory();
    if (skyboxRenderer_ == nullptr) {
        skyboxRenderer_ = factory != nullptr ? factory->CreateSkyboxRenderer() : std::make_unique<SkyboxRenderer>();
    }
    if (shadowRenderer_ == nullptr) {
        shadowRenderer_ = factory != nullptr ? factory->CreateShadowRenderer() : std::make_unique<ShadowRenderer>();
    }
    if (postProcessRenderer_ == nullptr) {
        postProcessRenderer_ = factory != nullptr ? factory->CreatePostProcessRenderer() : std::make_unique<PostProcessRenderer>();
    }
    skyboxRenderer_->Initialize(context);
    shadowRenderer_->Initialize(context);
}

void VulkanRenderer::UpdateMainSkyDescriptorSets(VulkanContext& context) {
    if (skyboxRenderer_ == nullptr) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        skyboxRenderer_->WriteSceneBinding(context, *bindings_[i]);
    }
    boundSkyImageView_ = VK_NULL_HANDLE;
}

void VulkanRenderer::UpdateShadowDescriptorSets(VulkanContext& context) {
    if (shadowRenderer_ == nullptr) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        shadowRenderer_->WriteSceneBinding(context, *bindings_[i]);
    }
}

void VulkanRenderer::CreateSurfaceTextures(VulkanContext& context) {
    DestroySurfaceTextures(context);

    const auto device = context.GetDevice();
    const auto physicalDevice = context.GetPhysicalDevice();
    const auto queue = context.GetGraphicsQueue();
    const auto queueFamily = context.GetGraphicsQueueFamily();

    const auto atlasPath = resourceRegistry_->GetTexturePath("surface_atlas");
    surfaceAtlas_ = TextureUtils::CreateTexture2DFromPath(
        physicalDevice,
        device,
        queue,
        queueFamily,
        atlasPath,
        true,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    );

    const auto normalAtlasPath = resourceRegistry_->GetTexturePath("surface_normal_atlas");
    hasSurfaceNormalAtlas_ = Utils::FileIO::Exists(normalAtlasPath);
    if (hasSurfaceNormalAtlas_) {
        surfaceNormalAtlas_ = TextureUtils::CreateTexture2DFromPath(
            physicalDevice,
            device,
            queue,
            queueFamily,
            normalAtlasPath,
            true,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        );
    }

    boundSurfaceAtlasView_ = VK_NULL_HANDLE;
    boundSurfaceNormalAtlasView_ = VK_NULL_HANDLE;
}

void VulkanRenderer::DestroySurfaceTextures(VulkanContext& context) {
    TextureUtils::DestroyTexture2D(context.GetDevice(), surfaceAtlas_);
    TextureUtils::DestroyTexture2D(context.GetDevice(), surfaceNormalAtlas_);
    hasSurfaceNormalAtlas_ = false;
    boundSurfaceAtlasView_ = VK_NULL_HANDLE;
    boundSurfaceNormalAtlasView_ = VK_NULL_HANDLE;
}

void VulkanRenderer::UpdateSurfaceDescriptorSets(VulkanContext& context) {
    if (surfaceAtlas_.View == VK_NULL_HANDLE || surfaceAtlas_.Sampler == VK_NULL_HANDLE) {
        return;
    }

    const auto normalView = hasSurfaceNormalAtlas_ ? surfaceNormalAtlas_.View : surfaceAtlas_.View;
    const auto normalSampler = hasSurfaceNormalAtlas_ ? surfaceNormalAtlas_.Sampler : surfaceAtlas_.Sampler;
    if (normalView == VK_NULL_HANDLE || normalSampler == VK_NULL_HANDLE) {
        return;
    }

    if (surfaceAtlas_.View == boundSurfaceAtlasView_ && normalView == boundSurfaceNormalAtlasView_) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        const VkDescriptorImageInfo atlasInfo{
            .sampler = surfaceAtlas_.Sampler,
            .imageView = surfaceAtlas_.View,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkDescriptorImageInfo normalInfo{
            .sampler = normalSampler,
            .imageView = normalView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        auto& binding = dynamic_cast<VulkanResourceBinding&>(*bindings_[i]);
        binding.UpdateImage(4, atlasInfo);
        binding.UpdateImage(5, normalInfo);
    }

    boundSurfaceAtlasView_ = surfaceAtlas_.View;
    boundSurfaceNormalAtlasView_ = normalView;
}

void VulkanRenderer::UpdateCameraUniformAndLighting(VulkanContext& context, const Common::FrameContext& frameContext) {
    static_cast<void>(context);
    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }

    CameraUniform uniform{};
    const Common::ShadowRenderer::ShadowData defaultShadowData{};
    const auto view = camera->GetViewMatrix().FlattenColumnMajor();
    const auto projection = camera->GetProjectionMatrix().FlattenColumnMajor();
    for (int i = 0; i < 16; ++i) {
        uniform.View[i] = static_cast<float>(view[static_cast<std::size_t>(i)]);
        uniform.Projection[i] = static_cast<float>(projection[static_cast<std::size_t>(i)]);
    }

    const auto cameraCFrame = camera->GetProperty("CFrame").value<Math::CFrame>();
    uniform.CameraPosition[0] = static_cast<float>(cameraCFrame.Position.x);
    uniform.CameraPosition[1] = static_cast<float>(cameraCFrame.Position.y);
    uniform.CameraPosition[2] = static_cast<float>(cameraCFrame.Position.z);
    uniform.CameraPosition[3] = 1.0F;
    const auto cameraForward = cameraCFrame.LookVector().Unit();
    uniform.CameraForward[0] = static_cast<float>(cameraForward.x);
    uniform.CameraForward[1] = static_cast<float>(cameraForward.y);
    uniform.CameraForward[2] = static_cast<float>(cameraForward.z);
    uniform.CameraForward[3] = 0.0F;

    uniform.LightDirection[0] = 0.0F;
    uniform.LightDirection[1] = -1.0F;
    uniform.LightDirection[2] = 0.0F;
    uniform.LightDirection[3] = 0.0F;
    uniform.LightColorIntensity[0] = 0.0F;
    uniform.LightColorIntensity[1] = 0.0F;
    uniform.LightColorIntensity[2] = 0.0F;
    uniform.LightColorIntensity[3] = 0.0F;
    uniform.LightSpecular[0] = 0.0F;
    uniform.LightSpecular[1] = settingsSnapshot_.DirectionalLightShininess;
    uniform.LightSpecular[2] = 0.0F;
    uniform.LightSpecular[3] = 0.0F;
    uniform.Ambient[0] = static_cast<float>(settingsSnapshot_.AmbientColor.r);
    uniform.Ambient[1] = static_cast<float>(settingsSnapshot_.AmbientColor.g);
    uniform.Ambient[2] = static_cast<float>(settingsSnapshot_.AmbientColor.b);
    uniform.Ambient[3] = 1.0F;

    const auto skyTint = skyboxRenderer_ != nullptr ? skyboxRenderer_->GetSkyTint() : Math::Color3{1.0, 1.0, 1.0};
    uniform.SkyTint[0] = static_cast<float>(skyTint.r);
    uniform.SkyTint[1] = static_cast<float>(skyTint.g);
    uniform.SkyTint[2] = static_cast<float>(skyTint.b);
    uniform.SkyTint[3] = 1.0F;
    uniform.RenderSettings[0] = 0.0F;
    uniform.RenderSettings[1] = static_cast<float>(Enums::LightingComputationMode::PerPixel);
    uniform.RenderSettings[2] = 0.0F;
    uniform.RenderSettings[3] = 0.0F;
    for (int cascade = 0; cascade < 3; ++cascade) {
        const auto identity = Math::Matrix4::Identity().FlattenColumnMajor();
        for (int i = 0; i < 16; ++i) {
            uniform.ShadowMatrices[cascade][i] = static_cast<float>(identity[static_cast<std::size_t>(i)]);
        }
    }
    uniform.ShadowCascadeSplits[0] = defaultShadowData.Split0;
    uniform.ShadowCascadeSplits[1] = defaultShadowData.Split1;
    uniform.ShadowCascadeSplits[2] = defaultShadowData.MaxDistance;
    uniform.ShadowCascadeSplits[3] = 1.0F;
    uniform.ShadowParams[0] = defaultShadowData.Bias;
    uniform.ShadowParams[1] = defaultShadowData.BlurAmount;
    uniform.ShadowParams[2] = static_cast<float>(defaultShadowData.TapCount);
    uniform.ShadowParams[3] = defaultShadowData.FadeWidth;
    uniform.ShadowState[0] = 0.0F;
    uniform.ShadowState[1] = defaultShadowData.JitterScaleX;
    uniform.ShadowState[2] = defaultShadowData.JitterScaleY;
    uniform.ShadowState[3] = 0.0F;

    uniform.Ambient[0] = static_cast<float>(settingsSnapshot_.AmbientColor.r) * settingsSnapshot_.AmbientStrength;
    uniform.Ambient[1] = static_cast<float>(settingsSnapshot_.AmbientColor.g) * settingsSnapshot_.AmbientStrength;
    uniform.Ambient[2] = static_cast<float>(settingsSnapshot_.AmbientColor.b) * settingsSnapshot_.AmbientStrength;
    uniform.RenderSettings[1] = static_cast<float>(settingsSnapshot_.ShadingMode);
    uniform.RenderSettings[2] = settingsSnapshot_.GammaCorrection ? 1.0F : 0.0F;
    uniform.RenderSettings[3] = settingsSnapshot_.InaccurateNeon ? 1.0F : 0.0F;

    if (settingsSnapshot_.DirectionalLightEnabled) {
        float dir[3] = {
            static_cast<float>(settingsSnapshot_.DirectionalLightDirection.x),
            static_cast<float>(settingsSnapshot_.DirectionalLightDirection.y),
            static_cast<float>(settingsSnapshot_.DirectionalLightDirection.z)
        };
        Normalize3(dir);
        uniform.LightDirection[0] = dir[0];
        uniform.LightDirection[1] = dir[1];
        uniform.LightDirection[2] = dir[2];

        uniform.LightColorIntensity[0] = static_cast<float>(settingsSnapshot_.DirectionalLightColor.r);
        uniform.LightColorIntensity[1] = static_cast<float>(settingsSnapshot_.DirectionalLightColor.g);
        uniform.LightColorIntensity[2] = static_cast<float>(settingsSnapshot_.DirectionalLightColor.b);
        uniform.LightColorIntensity[3] = settingsSnapshot_.DirectionalLightIntensity;
        uniform.LightSpecular[0] = settingsSnapshot_.DirectionalLightSpecularStrength;
        uniform.LightSpecular[1] = settingsSnapshot_.DirectionalLightShininess;
        uniform.RenderSettings[0] = 1.0F;
    }

    const auto* shadowData = shadowRenderer_ != nullptr ? &shadowRenderer_->GetShadowData() : nullptr;
    if (shadowData != nullptr && shadowData->HasShadowData) {
        for (int cascade = 0; cascade < 3; ++cascade) {
            const auto flattened = shadowData->LightViewProjectionMatrices[static_cast<std::size_t>(cascade)].FlattenColumnMajor();
            for (int i = 0; i < 16; ++i) {
                uniform.ShadowMatrices[cascade][i] = static_cast<float>(flattened[static_cast<std::size_t>(i)]);
            }
        }
        uniform.ShadowCascadeSplits[0] = shadowData->Split0;
        uniform.ShadowCascadeSplits[1] = shadowData->Split1;
        uniform.ShadowCascadeSplits[2] = shadowData->MaxDistance;
        uniform.ShadowCascadeSplits[3] = static_cast<float>(shadowData->CascadeCount);
        uniform.ShadowParams[0] = shadowData->Bias;
        uniform.ShadowParams[1] = shadowData->BlurAmount;
        uniform.ShadowParams[2] = static_cast<float>(shadowData->TapCount);
        uniform.ShadowParams[3] = shadowData->FadeWidth;
        uniform.ShadowState[0] = settingsSnapshot_.Shadow.Enabled ? 1.0F : 0.0F;
        uniform.ShadowState[1] = shadowData->JitterScaleX;
        uniform.ShadowState[2] = shadowData->JitterScaleY;
    } else {
        uniform.ShadowState[0] = 0.0F;
    }

    uniform.ShadowParams[1] = settingsSnapshot_.Shadow.BlurAmount;
    uniform.ShadowParams[2] = static_cast<float>(settingsSnapshot_.Shadow.TapCount);

    uniformBuffers_[frameContext.FrameIndex]->Upload(&uniform, sizeof(CameraUniform));
}

float VulkanRenderer::GetAspect(const VulkanContext& context) const {
    const auto extent = context.GetSwapchainExtent();
    if (extent.Height == 0) {
        return 1.0F;
    }
    return static_cast<float>(extent.Width) / static_cast<float>(extent.Height);
}

void VulkanRenderer::DrawPart(
    Common::CommandBuffer& commandBuffer,
    const Common::RenderPartProxy& proxy,
    const bool transparent
) {
    const auto mesh = std::dynamic_pointer_cast<Common::Mesh>(proxy.GetMesh());
    if (mesh == nullptr) {
        return;
    }

    const Common::PipelineVariantKey pipelineKey{
        .CullMode = ToPipelineCullMode(proxy.GetCullMode()),
        .DepthMode = transparent ? Common::PipelineDepthMode::ReadOnly : Common::PipelineDepthMode::ReadWrite,
        .BlendMode = transparent ? Common::PipelineBlendMode::AlphaBlend : Common::PipelineBlendMode::Opaque
    };
    GetPipeline(pipelineKey).Bind(commandBuffer);

    PushConstants push{};
    const auto model = proxy.GetModelMatrix().FlattenColumnMajor();
    for (int i = 0; i < 16; ++i) {
        push.Model[i] = static_cast<float>(model[static_cast<std::size_t>(i)]);
    }
    const auto color = proxy.GetColor();
    push.BaseColor[0] = static_cast<float>(color.r);
    push.BaseColor[1] = static_cast<float>(color.g);
    push.BaseColor[2] = static_cast<float>(color.b);
    push.BaseColor[3] = proxy.GetAlpha();
    push.Material[0] = proxy.GetMetalness();
    push.Material[1] = proxy.GetRoughness();
    push.Material[2] = proxy.GetEmissive();
    push.Material[3] = 0.0F;
    push.SurfaceData0[0] = static_cast<float>(proxy.GetTopSurfaceType());
    push.SurfaceData0[1] = static_cast<float>(proxy.GetBottomSurfaceType());
    push.SurfaceData0[2] = static_cast<float>(proxy.GetFrontSurfaceType());
    push.SurfaceData0[3] = static_cast<float>(proxy.GetBackSurfaceType());
    push.SurfaceData1[0] = static_cast<float>(proxy.GetLeftSurfaceType());
    push.SurfaceData1[1] = static_cast<float>(proxy.GetRightSurfaceType());
    push.SurfaceData1[2] = proxy.GetSurfaceEnabled() ? 1.0F : 0.0F;
    push.SurfaceData1[3] = hasSurfaceNormalAtlas_ ? 1.0F : 0.0F;

    commandBuffer.PushConstants(
        *pipelineLayout_,
        Common::ShaderStageFlags::Vertex | Common::ShaderStageFlags::Fragment,
        &push,
        sizeof(PushConstants)
    );

    auto* activeContext = GetCurrentContext();
    if (activeContext == nullptr) {
        return;
    }
    mesh->EnsureUploaded(*activeContext);
    mesh->Draw(commandBuffer);
}

void VulkanRenderer::DrawOverlayPrimitive(
    Common::CommandBuffer& commandBuffer,
    const Rendering::Common::OverlayPrimitive& primitive
) {
    const auto mesh = std::dynamic_pointer_cast<Common::Mesh>(GetMeshCache().GetPrimitive(primitive.Shape));
    auto* activeContext = GetCurrentContext();
    if (mesh == nullptr || activeContext == nullptr) {
        return;
    }

    const Common::PipelineVariantKey pipelineKey{
        .CullMode = Common::PipelineCullMode::None,
        .DepthMode = primitive.AlwaysOnTop ? Common::PipelineDepthMode::Disabled : Common::PipelineDepthMode::ReadOnly,
        .BlendMode = Common::PipelineBlendMode::AlphaBlend
    };
    GetPipeline(pipelineKey).Bind(commandBuffer);

    PushConstants push{};
    const auto model = primitive.Model.FlattenColumnMajor();
    for (int i = 0; i < 16; ++i) {
        push.Model[i] = static_cast<float>(model[static_cast<std::size_t>(i)]);
    }
    push.BaseColor[0] = static_cast<float>(primitive.Color.r);
    push.BaseColor[1] = static_cast<float>(primitive.Color.g);
    push.BaseColor[2] = static_cast<float>(primitive.Color.b);
    push.BaseColor[3] = primitive.Alpha;
    push.Material[0] = primitive.Metalness;
    push.Material[1] = primitive.Roughness;
    push.Material[2] = primitive.Emissive;
    push.Material[3] = primitive.IgnoreLighting ? 1.0F : 0.0F;
    push.SurfaceData0[0] = 0.0F;
    push.SurfaceData0[1] = 0.0F;
    push.SurfaceData0[2] = 0.0F;
    push.SurfaceData0[3] = 0.0F;
    push.SurfaceData1[0] = 0.0F;
    push.SurfaceData1[1] = 0.0F;
    push.SurfaceData1[2] = 0.0F;
    push.SurfaceData1[3] = 0.0F;

    commandBuffer.PushConstants(
        *pipelineLayout_,
        Common::ShaderStageFlags::Vertex | Common::ShaderStageFlags::Fragment,
        &push,
        sizeof(PushConstants)
    );

    mesh->EnsureUploaded(*activeContext);
    mesh->Draw(commandBuffer);
}

const Common::Pipeline& VulkanRenderer::GetPipeline(const Common::PipelineVariantKey& key) const {
    const auto it = pipelines_.find(key);
    if (it == pipelines_.end() || it->second == nullptr) {
        throw std::runtime_error("Requested graphics pipeline variant is not available.");
    }
    return *it->second;
}

const Common::ResourceBinding& VulkanRenderer::GetResourceBinding(const std::uint32_t frameIndex) const {
    if (frameIndex >= bindings_.size() || bindings_[frameIndex] == nullptr) {
        throw std::runtime_error("Requested frame resource binding is not available.");
    }
    return *bindings_[frameIndex];
}

} // namespace Lvs::Engine::Rendering::Vulkan

