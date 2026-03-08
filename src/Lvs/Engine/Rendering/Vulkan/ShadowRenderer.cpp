#include "Lvs/Engine/Rendering/Vulkan/ShadowRenderer.hpp"

#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Rendering/Common/Mesh.hpp"
#include "Lvs/Engine/Rendering/Common/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBinding.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanVertexLayout.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanRenderManifest.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

VkFormat FindSupportedDepthFormat(const VkPhysicalDevice physicalDevice) {
    constexpr std::array candidates{
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (const auto format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        const VkFormatFeatureFlags needed = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & needed) == needed) {
            return format;
        }
    }
    throw std::runtime_error("No sampled depth format is available for shadow rendering.");
}

VkCommandBuffer BeginOneTimeCommands(
    const VkDevice device,
    const std::uint32_t queueFamilyIndex,
    VkCommandPool& commandPool
) {
    const VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create transient command pool.");
    }

    const VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to allocate transient command buffer.");
    }

    const VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to begin transient command buffer.");
    }
    return commandBuffer;
}

void EndOneTimeCommands(
    const VkDevice device,
    const VkQueue queue,
    const VkCommandPool commandPool,
    const VkCommandBuffer commandBuffer
) {
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end transient command buffer.");
    }

    const VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    if (vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit transient command buffer.");
    }
    vkQueueWaitIdle(queue);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

} // namespace

ShadowRenderer::~ShadowRenderer() = default;

void ShadowRenderer::Initialize(Common::GraphicsContext& context) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    if (initialized_) {
        return;
    }
    if (pipelineManifest_ == nullptr) {
        pipelineManifest_ = std::make_shared<VulkanPipelineManifestProvider>();
    }

    depthFormat_ = FindSupportedDepthFormat(vkContext.GetPhysicalDevice());
    CreateRenderPass(vkContext);
    CreatePipelineLayout(vkContext);
    CreatePipelines(vkContext);
    EnsureDepthResources(
        vkContext,
        Common::ShadowQualityProfile{}.MapResolution,
        Common::ShadowQualityProfile{}.CascadeResolutionScale
    );
    EnsureJitterTexture(vkContext);
    initialized_ = true;
}

void ShadowRenderer::RecreateSwapchain(Common::GraphicsContext& context) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    if (!initialized_) {
        Initialize(context);
        return;
    }
    DestroySwapchainResources(vkContext);
    CreatePipelines(vkContext);
}

void ShadowRenderer::Shutdown(Common::GraphicsContext& context) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    DestroySwapchainResources(vkContext);
    DestroyDepthResources(vkContext);
    DestroyJitterTexture(vkContext);

    const VkDevice device = vkContext.GetDevice();
    if (shadowSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, shadowSampler_, nullptr);
        shadowSampler_ = VK_NULL_HANDLE;
    }
    pipelineLayout_.reset();
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
    shadowData_ = {};
}

ShadowRenderer::ShadowPassOutput ShadowRenderer::Render(
    Common::GraphicsContext& context,
    Common::CommandBuffer& commandBuffer,
    const ShadowPassInput& input
) {
    if (input.Casters == nullptr || input.Camera == nullptr) {
        shadowData_.HasShadowData = false;
        return ShadowPassOutput{.Data = shadowData_};
    }

    std::vector<std::shared_ptr<Common::RenderPartProxy>> partCasters;
    partCasters.reserve(input.Casters->size());
    for (const auto& proxy : *input.Casters) {
        if (const auto partProxy = std::dynamic_pointer_cast<Common::RenderPartProxy>(proxy); partProxy != nullptr) {
            partCasters.push_back(partProxy);
        }
    }
    RenderPartCasters(
        static_cast<VulkanContext&>(context),
        commandBuffer,
        partCasters,
        *input.Camera,
        input.DirectionalLightDirection,
        input.CameraAspect,
        input.Quality
    );
    return ShadowPassOutput{.Data = shadowData_};
}

void ShadowRenderer::RenderPartCasters(
    VulkanContext& context,
    Common::CommandBuffer& commandBuffer,
    const std::vector<std::shared_ptr<Common::RenderPartProxy>>& shadowCasters,
    const Objects::Camera& camera,
    const Math::Vector3& directionalLightDirection,
    const float cameraAspect,
    const Common::ShadowQualityProfile& settings
) {
    if (!initialized_) {
        Initialize(static_cast<Common::GraphicsContext&>(context));
    }
    if (pipelines_.empty() || renderPass_ == VK_NULL_HANDLE) {
        shadowData_.HasShadowData = false;
        return;
    }

    const auto normalized = Common::NormalizeShadowSettings(settings);
    EnsureDepthResources(context, normalized.MapResolution, normalized.CascadeResolutionScale);
    EnsureJitterTexture(context);

    shadowData_.TapCount = normalized.TapCount;
    shadowData_.BlurAmount = normalized.BlurAmount;
    shadowData_.Bias = normalized.Bias;
    shadowData_.FadeWidth = normalized.FadeWidth;
    shadowData_.CascadeCount = normalized.CascadeCount;
    shadowData_.MaxDistance = normalized.MaxDistance;

    if (!settings.Enabled || directionalLightDirection.MagnitudeSquared() <= 1e-8) {
        shadowData_.HasShadowData = false;
        return;
    }

    Common::ShadowCascadeComputation cascades{};
    if (!Common::ComputeShadowCascades(
            camera,
            directionalLightDirection,
            cameraAspect,
            normalized,
            cascadeResolutions_,
            cascades)) {
        shadowData_.HasShadowData = false;
        return;
    }

    shadowData_.LightViewProjectionMatrices = cascades.Matrices;
    shadowData_.Split0 = cascades.Split0;
    shadowData_.Split1 = cascades.Split1;
    shadowData_.MaxDistance = cascades.MaxDistance;

    for (int cascadeIndex = 0; cascadeIndex < shadowData_.CascadeCount; ++cascadeIndex) {
        const auto cascadeResolution = cascadeResolutions_[static_cast<std::size_t>(cascadeIndex)];
        const VkClearValue clearValue{.depthStencil = {.depth = 1.0F, .stencil = 0}};
        const VulkanDrawPassState passState(
            renderPass_,
            cascadeImages_[cascadeIndex].Framebuffer,
            {.X = 0, .Y = 0, .Width = cascadeResolution, .Height = cascadeResolution},
            &clearValue,
            1
        );
        commandBuffer.BeginDrawPass(passState);
        commandBuffer.SetViewport({
            .X = 0.0F,
            .Y = 0.0F,
            .Width = static_cast<float>(cascadeResolution),
            .Height = static_cast<float>(cascadeResolution),
            .MinDepth = 0.0F,
            .MaxDepth = 1.0F
        });
        commandBuffer.SetScissor({
            .X = 0,
            .Y = 0,
            .Width = cascadeResolution,
            .Height = cascadeResolution
        });
        const Common::PipelineVariantKey pipelineKey{
            .CullMode = Common::PipelineCullMode::None,
            .DepthMode = Common::PipelineDepthMode::ReadWrite,
            .BlendMode = Common::PipelineBlendMode::Opaque
        };
        GetPipeline(pipelineKey).Bind(commandBuffer);

        for (const auto& proxy : shadowCasters) {
            if (proxy == nullptr) {
                continue;
            }
            if (!proxy->GetPolicy().Visible || !proxy->GetPolicy().CastsShadow) {
                continue;
            }

            const auto& uploadedMesh = proxy->GetMesh();
            const auto mesh = std::dynamic_pointer_cast<Common::Mesh>(uploadedMesh);
            if (mesh == nullptr) {
                continue;
            }

            PushConstants push{};
            const auto lightVp = shadowData_.LightViewProjectionMatrices[static_cast<std::size_t>(cascadeIndex)].FlattenColumnMajor();
            const auto model = proxy->GetModelMatrix().FlattenColumnMajor();
            for (int i = 0; i < 16; ++i) {
                push.LightViewProjection[i] = static_cast<float>(lightVp[static_cast<std::size_t>(i)]);
                push.Model[i] = static_cast<float>(model[static_cast<std::size_t>(i)]);
            }
            commandBuffer.PushConstants(
                *pipelineLayout_,
                Common::ShaderStageFlags::Vertex,
                &push,
                sizeof(PushConstants)
            );

            mesh->EnsureUploaded(context);
            mesh->Draw(commandBuffer);
        }

        commandBuffer.EndDrawPass();
    }

    shadowData_.HasShadowData = true;
}

const ShadowRenderer::ShadowData& ShadowRenderer::GetShadowData() const {
    return shadowData_;
}

void ShadowRenderer::WriteSceneBinding(Common::GraphicsContext& context, Common::ResourceBinding& binding) const {
    static_cast<void>(context);
    if (shadowSampler_ == VK_NULL_HANDLE || cascadeImageViews_[0] == VK_NULL_HANDLE || cascadeImageViews_[1] == VK_NULL_HANDLE ||
        cascadeImageViews_[2] == VK_NULL_HANDLE || jitterSampler_ == VK_NULL_HANDLE || jitterImageView_ == VK_NULL_HANDLE) {
        return;
    }

    auto& vkBinding = dynamic_cast<VulkanResourceBinding&>(binding);
    const std::array<VkDescriptorImageInfo, 3> shadowInfos{{
        VkDescriptorImageInfo{
            .sampler = shadowSampler_,
            .imageView = cascadeImageViews_[0],
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        },
        VkDescriptorImageInfo{
            .sampler = shadowSampler_,
            .imageView = cascadeImageViews_[1],
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        },
        VkDescriptorImageInfo{
            .sampler = shadowSampler_,
            .imageView = cascadeImageViews_[2],
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        }
    }};
    vkBinding.UpdateImages(2, shadowInfos.data(), static_cast<std::uint32_t>(shadowInfos.size()));

    const VkDescriptorImageInfo jitterInfo{
        .sampler = jitterSampler_,
        .imageView = jitterImageView_,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    vkBinding.UpdateImage(3, jitterInfo);
}

VkSampler ShadowRenderer::GetShadowSampler() const {
    return shadowSampler_;
}

const std::array<VkImageView, 3>& ShadowRenderer::GetShadowImageViews() const {
    return cascadeImageViews_;
}

VkSampler ShadowRenderer::GetJitterSampler() const {
    return jitterSampler_;
}

VkImageView ShadowRenderer::GetJitterImageView() const {
    return jitterImageView_;
}

void ShadowRenderer::CreateRenderPass(VulkanContext& context) {
    const VkAttachmentDescription depthAttachment{
        .flags = 0,
        .format = depthFormat_,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    const VkAttachmentReference depthRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 0,
        .pColorAttachments = nullptr,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = &depthRef,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };

    const std::array<VkSubpassDependency, 2> dependencies{{
        VkSubpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },
        VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        }
    }};

    const VkRenderPassCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &depthAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = static_cast<std::uint32_t>(dependencies.size()),
        .pDependencies = dependencies.data()
    };
    if (vkCreateRenderPass(context.GetDevice(), &createInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow render pass.");
    }
}

void ShadowRenderer::CreatePipelineLayout(VulkanContext& context) {
    const VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };
    const VkPipelineLayoutCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    pipelineLayout_ = VulkanPipelineLayout::Create(context.GetDevice(), createInfo);
}

void ShadowRenderer::CreatePipelines(VulkanContext& context) {
    pipelines_.clear();
    for (const auto& key : GetPipelineVariants()) {
        pipelines_.emplace(key, CreatePipelineVariant(context, key));
    }
}

std::unique_ptr<VulkanPipelineVariant> ShadowRenderer::CreatePipelineVariant(
    VulkanContext& context,
    const Common::PipelineVariantKey& key
) {
    const VkDevice device = context.GetDevice();
    const auto vertPath = pipelineManifest_->GetShaderPath("shadow", Common::ShaderStage::Vertex);
    const auto fragPath = pipelineManifest_->GetShaderPath("shadow", Common::ShaderStage::Fragment);

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
        .depthClampEnable = VK_TRUE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = key.CullMode == Common::PipelineCullMode::Front ? VK_CULL_MODE_FRONT_BIT
                                                                    : key.CullMode == Common::PipelineCullMode::Back
                                                                          ? VK_CULL_MODE_BACK_BIT
                                                                          : VK_CULL_MODE_NONE,
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
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F
    };
    const std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    const VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

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
        .pColorBlendState = nullptr,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout_->GetHandle(),
        .renderPass = renderPass_,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    return VulkanPipelineVariant::CreateGraphicsPipeline(device, pipelineInfo, *pipelineLayout_);
}

void ShadowRenderer::EnsureDepthResources(
    VulkanContext& context,
    const std::uint32_t resolution,
    const float cascadeResolutionScale
) {
    const auto targetResolutions = Common::ComputeCascadeResolutions(resolution, cascadeResolutionScale);

    if (resolution == mapResolution_ && targetResolutions == cascadeResolutions_ && shadowSampler_ != VK_NULL_HANDLE &&
        cascadeImages_[0].Image != VK_NULL_HANDLE) {
        return;
    }

    if (cascadeImages_[0].Image != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context.GetDevice());
    }
    DestroyDepthResources(context);
    mapResolution_ = resolution;
    cascadeResolutions_ = targetResolutions;

    const VkDevice device = context.GetDevice();
    const VkPhysicalDevice physicalDevice = context.GetPhysicalDevice();

    for (int i = 0; i < MAX_CASCADES; ++i) {
        const auto cascadeResolution = cascadeResolutions_[static_cast<std::size_t>(i)];
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depthFormat_,
            .extent = {.width = cascadeResolution, .height = cascadeResolution, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(device, &imageInfo, nullptr, &cascadeImages_[i].Image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow depth image.");
        }

        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(device, cascadeImages_[i].Image, &memRequirements);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                physicalDevice,
                memRequirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (vkAllocateMemory(device, &allocInfo, nullptr, &cascadeImages_[i].Memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate shadow depth memory.");
        }
        vkBindImageMemory(device, cascadeImages_[i].Image, cascadeImages_[i].Memory, 0);

        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = cascadeImages_[i].Image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = depthFormat_,
            .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                           .a = VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                                 .baseMipLevel = 0,
                                 .levelCount = 1,
                                 .baseArrayLayer = 0,
                                 .layerCount = 1}
        };
        if (vkCreateImageView(device, &viewInfo, nullptr, &cascadeImages_[i].View) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow depth image view.");
        }
        cascadeImageViews_[i] = cascadeImages_[i].View;

        const VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = renderPass_,
            .attachmentCount = 1,
            .pAttachments = &cascadeImages_[i].View,
            .width = cascadeResolution,
            .height = cascadeResolution,
            .layers = 1
        };
        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &cascadeImages_[i].Framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow framebuffer.");
        }
        cascadeImages_[i].Resolution = cascadeResolution;
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_TRUE,
        .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(device, &samplerInfo, nullptr, &shadowSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow sampler.");
    }
}

void ShadowRenderer::EnsureJitterTexture(VulkanContext& context) {
    if (jitterImage_ != VK_NULL_HANDLE && jitterImageView_ != VK_NULL_HANDLE && jitterSampler_ != VK_NULL_HANDLE) {
        return;
    }

    const std::uint32_t clampedSize = std::max(Common::kShadowMinJitterSizeXY, jitterSizeXY_);
    const std::uint32_t clampedDepth = std::max(Common::kShadowMinJitterDepth, jitterDepth_);
    const std::vector<std::uint8_t> data = Common::GenerateShadowJitterTextureData(clampedSize, clampedDepth);

    const VkDevice device = context.GetDevice();
    const VkPhysicalDevice physicalDevice = context.GetPhysicalDevice();
    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(data.size());
    auto staging = BufferUtils::CreateBuffer(
        physicalDevice,
        device,
        uploadSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void* mapped = nullptr;
    vkMapMemory(device, staging.Memory, 0, uploadSize, 0, &mapped);
    std::memcpy(mapped, data.data(), data.size());
    vkUnmapMemory(device, staging.Memory);

    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_3D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {.width = clampedSize, .height = clampedSize, .depth = clampedDepth},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &imageInfo, nullptr, &jitterImage_) != VK_SUCCESS) {
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create shadow jitter image.");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, jitterImage_, &memRequirements);
    const VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = BufferUtils::FindMemoryType(
            physicalDevice,
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        )
    };
    if (vkAllocateMemory(device, &allocInfo, nullptr, &jitterMemory_) != VK_SUCCESS) {
        vkDestroyImage(device, jitterImage_, nullptr);
        jitterImage_ = VK_NULL_HANDLE;
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to allocate shadow jitter image memory.");
    }
    vkBindImageMemory(device, jitterImage_, jitterMemory_, 0);

    VkCommandPool commandPool = VK_NULL_HANDLE;
    const VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, context.GetGraphicsQueueFamily(), commandPool);

    const VkImageMemoryBarrier toTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = jitterImage_,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &toTransfer
    );

    const VkBufferImageCopy copyRegion{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {clampedSize, clampedSize, clampedDepth}
    };
    vkCmdCopyBufferToImage(commandBuffer, staging.Buffer, jitterImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    const VkImageMemoryBarrier toShaderRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = jitterImage_,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}
    };
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &toShaderRead
    );
    EndOneTimeCommands(device, context.GetGraphicsQueue(), commandPool, commandBuffer);
    BufferUtils::DestroyBuffer(device, staging);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = jitterImage_,
        .viewType = VK_IMAGE_VIEW_TYPE_3D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}
    };
    if (vkCreateImageView(device, &viewInfo, nullptr, &jitterImageView_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow jitter image view.");
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0F,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0F,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0F,
        .maxLod = 0.0F,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(device, &samplerInfo, nullptr, &jitterSampler_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow jitter sampler.");
    }

    shadowData_.JitterScaleX = 1.0F / static_cast<float>(std::max(1U, clampedSize));
    shadowData_.JitterScaleY = 1.0F / static_cast<float>(std::max(1U, clampedSize));
}

void ShadowRenderer::DestroyDepthResources(VulkanContext& context) {
    const VkDevice device = context.GetDevice();

    for (int i = 0; i < MAX_CASCADES; ++i) {
        if (cascadeImages_[i].Framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, cascadeImages_[i].Framebuffer, nullptr);
            cascadeImages_[i].Framebuffer = VK_NULL_HANDLE;
        }
        if (cascadeImages_[i].View != VK_NULL_HANDLE) {
            vkDestroyImageView(device, cascadeImages_[i].View, nullptr);
            cascadeImages_[i].View = VK_NULL_HANDLE;
        }
        if (cascadeImages_[i].Image != VK_NULL_HANDLE) {
            vkDestroyImage(device, cascadeImages_[i].Image, nullptr);
            cascadeImages_[i].Image = VK_NULL_HANDLE;
        }
        if (cascadeImages_[i].Memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, cascadeImages_[i].Memory, nullptr);
            cascadeImages_[i].Memory = VK_NULL_HANDLE;
        }
        cascadeImages_[i].Resolution = 0;
        cascadeImageViews_[i] = VK_NULL_HANDLE;
    }

    if (shadowSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, shadowSampler_, nullptr);
        shadowSampler_ = VK_NULL_HANDLE;
    }

    mapResolution_ = 0;
}

void ShadowRenderer::DestroyJitterTexture(VulkanContext& context) {
    const VkDevice device = context.GetDevice();
    if (jitterSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, jitterSampler_, nullptr);
        jitterSampler_ = VK_NULL_HANDLE;
    }
    if (jitterImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, jitterImageView_, nullptr);
        jitterImageView_ = VK_NULL_HANDLE;
    }
    if (jitterImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, jitterImage_, nullptr);
        jitterImage_ = VK_NULL_HANDLE;
    }
    if (jitterMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, jitterMemory_, nullptr);
        jitterMemory_ = VK_NULL_HANDLE;
    }
}

void ShadowRenderer::DestroySwapchainResources(VulkanContext& context) {
    static_cast<void>(context);
    pipelines_.clear();
}

const VulkanPipelineVariant& ShadowRenderer::GetPipeline(const Common::PipelineVariantKey& key) const {
    const auto it = pipelines_.find(key);
    if (it == pipelines_.end() || it->second == nullptr) {
        throw std::runtime_error("Shadow pipeline variant is not available.");
    }
    return *it->second;
}

std::vector<Common::PipelineVariantKey> ShadowRenderer::GetPipelineVariants() const {
    return {{
        .CullMode = Common::PipelineCullMode::None,
        .DepthMode = Common::PipelineDepthMode::ReadWrite,
        .BlendMode = Common::PipelineBlendMode::Opaque
    }};
}

} // namespace Lvs::Engine::Rendering::Vulkan
