#include "Lvs/Engine/Rendering/Vulkan/ShadowRenderer.hpp"

#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Mesh.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Vertex.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

struct Vec4 {
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double w{0.0};
};

double Clamp(const double value, const double minValue, const double maxValue) {
    return std::max(minValue, std::min(maxValue, value));
}

Vec4 Multiply(const Math::Matrix4& matrix, const Vec4& vector) {
    const auto& m = matrix.Rows();
    return {
        m[0][0] * vector.x + m[0][1] * vector.y + m[0][2] * vector.z + m[0][3] * vector.w,
        m[1][0] * vector.x + m[1][1] * vector.y + m[1][2] * vector.z + m[1][3] * vector.w,
        m[2][0] * vector.x + m[2][1] * vector.y + m[2][2] * vector.z + m[2][3] * vector.w,
        m[3][0] * vector.x + m[3][1] * vector.y + m[3][2] * vector.z + m[3][3] * vector.w
    };
}

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

void ShadowRenderer::Initialize(VulkanContext& context) {
    if (initialized_) {
        return;
    }

    depthFormat_ = FindSupportedDepthFormat(context.GetPhysicalDevice());
    CreateRenderPass(context);
    CreatePipelineLayout(context);
    CreatePipeline(context);
    EnsureDepthResources(context, 4096, 0.7F);
    EnsureJitterTexture(context);
    initialized_ = true;
}

void ShadowRenderer::RecreateSwapchain(VulkanContext& context) {
    if (!initialized_) {
        Initialize(context);
        return;
    }
    DestroySwapchainResources(context);
    CreatePipeline(context);
}

void ShadowRenderer::Shutdown(VulkanContext& context) {
    DestroySwapchainResources(context);
    DestroyDepthResources(context);
    DestroyJitterTexture(context);

    const VkDevice device = context.GetDevice();
    if (shadowSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device, shadowSampler_, nullptr);
        shadowSampler_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
    shadowData_ = {};
}

void ShadowRenderer::Render(
    VulkanContext& context,
    const VkCommandBuffer commandBuffer,
    const std::vector<std::shared_ptr<RenderPartProxy>>& shadowCasters,
    const Objects::Camera& camera,
    const Math::Vector3& directionalLightDirection,
    const float cameraAspect,
    const ShadowSettings& settings
) {
    if (!initialized_) {
        Initialize(context);
    }
    if (pipeline_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        shadowData_.HasShadowData = false;
        return;
    }

    const std::uint32_t requestedResolution =
        std::max(128U, std::min(8192U, settings.MapResolution == 0 ? 4096U : settings.MapResolution));
    const float cascadeResolutionScale = std::max(0.25F, std::min(1.0F, settings.CascadeResolutionScale));
    const float cascadeSplitLambda = std::max(0.0F, std::min(1.0F, settings.CascadeSplitLambda));
    EnsureDepthResources(context, requestedResolution, cascadeResolutionScale);
    EnsureJitterTexture(context);

    shadowData_.TapCount = std::max(1, std::min(64, settings.TapCount));
    shadowData_.BlurAmount = std::max(0.0F, std::min(12.0F, settings.BlurAmount));
    shadowData_.Bias = 0.25F;
    shadowData_.FadeWidth = 0.25F;
    shadowData_.CascadeCount = std::max(1, std::min(MAX_CASCADES, settings.CascadeCount));
    shadowData_.MaxDistance = std::max(1.0F, std::min(1024.0F, settings.MaxDistance));

    if (!settings.Enabled || directionalLightDirection.MagnitudeSquared() <= 1e-8) {
        shadowData_.HasShadowData = false;
        return;
    }

    CascadeComputation cascades{};
    if (!ComputeCascades(
            camera,
            directionalLightDirection,
            cameraAspect,
            shadowData_.CascadeCount,
            shadowData_.MaxDistance,
            cascadeSplitLambda,
            cascades
        )) {
        shadowData_.HasShadowData = false;
        return;
    }

    shadowData_.LightViewProjectionMatrices = cascades.Matrices;
    shadowData_.Split0 = cascades.Split0;
    shadowData_.Split1 = cascades.Split1;
    shadowData_.MaxDistance = cascades.MaxDistance;

    for (int cascadeIndex = 0; cascadeIndex < shadowData_.CascadeCount; ++cascadeIndex) {
        const auto cascadeResolution = cascadeResolutions_[static_cast<std::size_t>(cascadeIndex)];
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(cascadeResolution),
            .height = static_cast<float>(cascadeResolution),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{
            .offset = {0, 0},
            .extent = {.width = cascadeResolution, .height = cascadeResolution}
        };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        const VkClearValue clearValue{.depthStencil = {.depth = 1.0F, .stencil = 0}};
        const VkRenderPassBeginInfo passInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderPass_,
            .framebuffer = cascadeImages_[cascadeIndex].Framebuffer,
            .renderArea = {.offset = {0, 0}, .extent = {.width = cascadeResolution, .height = cascadeResolution}},
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };

        vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        for (const auto& proxy : shadowCasters) {
            if (proxy == nullptr) {
                continue;
            }
            const auto instance = proxy->GetInstance();
            if (instance == nullptr || !instance->GetProperty("Renders").toBool()) {
                continue;
            }
            if (instance->GetProperty("Transparency").toDouble() >= 1.0) {
                continue;
            }

            const auto& mesh = proxy->GetMesh();
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
            vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &push);

            mesh->EnsureUploaded(context.GetPhysicalDevice(), context.GetDevice());
            mesh->Draw(commandBuffer);
        }

        vkCmdEndRenderPass(commandBuffer);
    }

    shadowData_.HasShadowData = true;
}

const ShadowRenderer::ShadowData& ShadowRenderer::GetShadowData() const {
    return shadowData_;
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
    if (vkCreatePipelineLayout(context.GetDevice(), &createInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shadow pipeline layout.");
    }
}

void ShadowRenderer::CreatePipeline(VulkanContext& context) {
    const VkDevice device = context.GetDevice();
    const QString vertPath = Utils::SourcePath::GetResourcePath("Shaders/Vulkan/Shadow.vert.spv");
    const QString fragPath = Utils::SourcePath::GetResourcePath("Shaders/Vulkan/Shadow.frag.spv");

    const auto vertCode = ShaderUtils::ReadBinaryFile(vertPath);
    const auto fragCode = ShaderUtils::ReadBinaryFile(fragPath);
    const VkShaderModule vertModule = ShaderUtils::CreateShaderModule(device, vertCode);
    const VkShaderModule fragModule = ShaderUtils::CreateShaderModule(device, fragCode);

    const VkPipelineShaderStageCreateInfo vertStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    const VkPipelineShaderStageCreateInfo fragStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragModule,
        .pName = "main",
        .pSpecializationInfo = nullptr
    };
    const VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    const auto binding = Vertex::BindingDescription();
    const auto attributes = Vertex::AttributeDescriptions();
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
        .cullMode = VK_CULL_MODE_NONE,
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
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
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
        .layout = pipelineLayout_,
        .renderPass = renderPass_,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        throw std::runtime_error("Failed to create shadow graphics pipeline.");
    }

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);
}

void ShadowRenderer::EnsureDepthResources(
    VulkanContext& context,
    const std::uint32_t resolution,
    const float cascadeResolutionScale
) {
    const double clampedScale = Clamp(static_cast<double>(cascadeResolutionScale), 0.25, 1.0);
    std::array<std::uint32_t, MAX_CASCADES> targetResolutions{};
    for (int i = 0; i < MAX_CASCADES; ++i) {
        const double scaledResolution = static_cast<double>(resolution) * std::pow(clampedScale, static_cast<double>(i));
        targetResolutions[static_cast<std::size_t>(i)] =
            std::max(128U, static_cast<std::uint32_t>(std::lround(std::max(128.0, scaledResolution))));
    }

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

    const auto sizeXY = static_cast<int>(std::max(2U, jitterSizeXY_));
    const auto depth = static_cast<int>(std::max(2U, jitterDepth_));
    const int pairCount = depth * 2;
    int samplesPerSide = static_cast<int>(std::sqrt(static_cast<double>(pairCount)));
    if (samplesPerSide * samplesPerSide != pairCount) {
        samplesPerSide = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(pairCount))));
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(sizeXY) * sizeXY * depth * 4U, 0);
    constexpr double Pi = 3.14159265358979323846;

    struct Offset {
        double x{0.0};
        double y{0.0};
    };

    for (int y = 0; y < sizeXY; ++y) {
        for (int x = 0; x < sizeXY; ++x) {
            const std::uint32_t seed =
                static_cast<std::uint32_t>(((x + 1) * 73856093) ^ ((y + 1) * 19349663));
            std::mt19937 rng(seed);
            std::uniform_real_distribution<double> dist(0.0, 1.0);

            std::vector<Offset> offsets;
            offsets.reserve(static_cast<std::size_t>(pairCount));
            for (int i = 0; i < pairCount; ++i) {
                const int sx = i % samplesPerSide;
                const int sy = i / samplesPerSide;
                const double u = (static_cast<double>(sx) + dist(rng)) / static_cast<double>(samplesPerSide);
                const double v = (static_cast<double>(sy) + dist(rng)) / static_cast<double>(samplesPerSide);
                const double r = std::sqrt(std::clamp(v, 0.0, 1.0));
                const double theta = 2.0 * Pi * u;
                offsets.push_back({std::cos(theta) * r, std::sin(theta) * r});
            }
            std::sort(offsets.begin(), offsets.end(), [](const auto& a, const auto& b) {
                const double ra = (a.x * a.x) + (a.y * a.y);
                const double rb = (b.x * b.x) + (b.y * b.y);
                return ra > rb;
            });

            auto encode = [](const double value) -> std::uint8_t {
                const double normalized = std::clamp((value * 0.5) + 0.5, 0.0, 1.0);
                return static_cast<std::uint8_t>(std::round(normalized * 255.0));
            };

            for (int z = 0; z < depth; ++z) {
                const auto& a = offsets[static_cast<std::size_t>((z * 2) + 0)];
                const auto& b = offsets[static_cast<std::size_t>((z * 2) + 1)];
                const std::size_t index =
                    ((static_cast<std::size_t>(z) * static_cast<std::size_t>(sizeXY) * static_cast<std::size_t>(sizeXY)) +
                     (static_cast<std::size_t>(y) * static_cast<std::size_t>(sizeXY)) + static_cast<std::size_t>(x)) *
                    4U;
                data[index + 0] = encode(a.x);
                data[index + 1] = encode(a.y);
                data[index + 2] = encode(b.x);
                data[index + 3] = encode(b.y);
            }
        }
    }

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
        .extent = {.width = jitterSizeXY_, .height = jitterSizeXY_, .depth = jitterDepth_},
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
        .imageExtent = {jitterSizeXY_, jitterSizeXY_, jitterDepth_}
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

    shadowData_.JitterScaleX = 1.0F / static_cast<float>(std::max(1U, jitterSizeXY_));
    shadowData_.JitterScaleY = 1.0F / static_cast<float>(std::max(1U, jitterSizeXY_));
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
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.GetDevice(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
}

bool ShadowRenderer::ComputeCascades(
    const Objects::Camera& camera,
    const Math::Vector3& directionalLightDirection,
    const float cameraAspect,
    const int cascadeCount,
    const float maxDistance,
    const float cascadeSplitLambda,
    CascadeComputation& out
) const {
    if (directionalLightDirection.MagnitudeSquared() <= 1e-8) {
        return false;
    }

    const double nearPlane = std::max(0.01, camera.GetProperty("NearPlane").toDouble());
    const double farPlane = std::max(nearPlane + 1.0, static_cast<double>(maxDistance));
    const auto splits = ComputeCascadeSplits(nearPlane, farPlane, cascadeCount, static_cast<double>(cascadeSplitLambda));

    double rangeNear = nearPlane;
    for (int i = 0; i < cascadeCount; ++i) {
        bool success = true;
        const Math::Matrix4 matrix = ComputeCascadeLightViewProjection(
            camera,
            directionalLightDirection,
            cameraAspect,
            rangeNear,
            splits[static_cast<std::size_t>(i)],
            cascadeResolutions_[static_cast<std::size_t>(i)],
            success
        );
        if (!success) {
            return false;
        }
        out.Matrices[static_cast<std::size_t>(i)] = matrix;
        rangeNear = splits[static_cast<std::size_t>(i)];
    }

    out.Split0 = static_cast<float>(cascadeCount >= 2 ? splits[0] : farPlane);
    out.Split1 = static_cast<float>(cascadeCount >= 3 ? splits[1] : farPlane);
    out.MaxDistance = static_cast<float>(farPlane);
    return true;
}

Math::Matrix4 ShadowRenderer::ComputeCascadeLightViewProjection(
    const Objects::Camera& camera,
    const Math::Vector3& direction,
    const float cameraAspect,
    const double rangeNear,
    const double rangeFar,
    const std::uint32_t cascadeResolution,
    bool& success
) const {
    success = false;
    if (rangeFar <= rangeNear + 1e-6) {
        return Math::Matrix4::Identity();
    }

    const auto cameraCFrame = camera.GetProperty("CFrame").value<Math::CFrame>();
    const Math::Vector3 camPos = cameraCFrame.Position;
    const Math::Vector3 camForward = cameraCFrame.LookVector().Unit();
    const Math::Vector3 camRight = cameraCFrame.RightVector().Unit();
    const Math::Vector3 camUp = cameraCFrame.UpVector().Unit();

    const double aspect = std::max(1e-6, static_cast<double>(cameraAspect));
    constexpr double DegToRad = 3.14159265358979323846 / 180.0;
    const double fovRadians = camera.GetProperty("FieldOfView").toDouble() * DegToRad;
    const double tanHalfFov = std::tan(fovRadians * 0.5);

    const double nearHeight = tanHalfFov * rangeNear;
    const double nearWidth = nearHeight * aspect;
    const double farHeight = tanHalfFov * rangeFar;
    const double farWidth = farHeight * aspect;

    const Math::Vector3 nearCenter = camPos + (camForward * rangeNear);
    const Math::Vector3 farCenter = camPos + (camForward * rangeFar);

    const std::array corners{
        nearCenter + (camUp * nearHeight) - (camRight * nearWidth),
        nearCenter + (camUp * nearHeight) + (camRight * nearWidth),
        nearCenter - (camUp * nearHeight) - (camRight * nearWidth),
        nearCenter - (camUp * nearHeight) + (camRight * nearWidth),
        farCenter + (camUp * farHeight) - (camRight * farWidth),
        farCenter + (camUp * farHeight) + (camRight * farWidth),
        farCenter - (camUp * farHeight) - (camRight * farWidth),
        farCenter - (camUp * farHeight) + (camRight * farWidth)
    };

    Math::Vector3 frustumCenter{};
    for (const auto& corner : corners) {
        frustumCenter = frustumCenter + corner;
    }
    frustumCenter = frustumCenter * (1.0 / 8.0);

    double radius = 0.0;
    for (const auto& corner : corners) {
        radius = std::max(radius, (corner - frustumCenter).Magnitude());
    }
    constexpr double radiusQuantization = 16.0;
    radius = std::ceil(radius * radiusQuantization) / radiusQuantization;
    radius = std::max(radius, 1.0);

    const Math::Vector3 lightDir = direction.Unit();
    const Math::Vector3 eye = frustumCenter - (lightDir * radius);

    Math::Vector3 up{0.0, 1.0, 0.0};
    if (std::abs(lightDir.Dot(up)) > 0.98) {
        up = {0.0, 0.0, 1.0};
    }

    const auto lightFrame = Math::CFrame::LookAt(eye, frustumCenter, up);
    const auto lightView = lightFrame.Inverse().ToMatrix4();

    double minZ = std::numeric_limits<double>::infinity();
    double maxZ = -std::numeric_limits<double>::infinity();
    for (const auto& corner : corners) {
        const auto transformed = Multiply(lightView, Vec4{corner.x, corner.y, corner.z, 1.0});
        minZ = std::min(minZ, transformed.z);
        maxZ = std::max(maxZ, transformed.z);
    }

    const double left = -radius;
    const double right = radius;
    const double bottom = -radius;
    const double top = radius;

    constexpr double cascadeDepthMultiplier = 10.0;
    const double depthRadius = radius * cascadeDepthMultiplier;
    const double nearPlane = std::max(0.1, (-maxZ) - depthRadius);
    const double farPlane = std::max(nearPlane + 1.0, (-minZ) + depthRadius);

    auto projection = BuildOrthographicZeroToOne(left, right, bottom, top, nearPlane, farPlane);
    projection = StabilizeProjection(projection, lightView, static_cast<double>(std::max(1U, cascadeResolution)));
    success = true;
    return projection * lightView;
}

std::array<double, ShadowRenderer::MAX_CASCADES> ShadowRenderer::ComputeCascadeSplits(
    const double nearPlane,
    const double farPlane,
    const int cascadeCount,
    const double lambda
) const {
    std::array<double, MAX_CASCADES> splits{farPlane, farPlane, farPlane};
    const double ratio = farPlane / std::max(nearPlane, 1e-4);
    const double clampedLambda = Clamp(lambda, 0.0, 1.0);
    for (int i = 1; i <= cascadeCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(cascadeCount);
        const double uniformSplit = nearPlane + ((farPlane - nearPlane) * t);
        const double logarithmicSplit = nearPlane * std::pow(ratio, t);
        const double split = (logarithmicSplit * clampedLambda) + (uniformSplit * (1.0 - clampedLambda));
        splits[static_cast<std::size_t>(i - 1)] = Clamp(split, nearPlane, farPlane);
    }
    splits[static_cast<std::size_t>(cascadeCount - 1)] = farPlane;
    return splits;
}

Math::Matrix4 ShadowRenderer::BuildOrthographicZeroToOne(
    const double left,
    const double right,
    const double bottom,
    const double top,
    const double nearPlane,
    const double farPlane
) const {
    const double width = std::max(1e-6, right - left);
    const double height = std::max(1e-6, top - bottom);
    const double depth = std::max(1e-6, farPlane - nearPlane);

    return Math::Matrix4({{
        {2.0 / width, 0.0, 0.0, -(right + left) / width},
        {0.0, 2.0 / height, 0.0, -(top + bottom) / height},
        {0.0, 0.0, -1.0 / depth, -nearPlane / depth},
        {0.0, 0.0, 0.0, 1.0}
    }});
}

Math::Matrix4 ShadowRenderer::StabilizeProjection(
    const Math::Matrix4& projection,
    const Math::Matrix4& lightView,
    const double resolution
) const {
    const auto shadowMatrix = projection * lightView;
    Vec4 shadowOrigin = Multiply(shadowMatrix, Vec4{0.0, 0.0, 0.0, 1.0});
    shadowOrigin.x *= (resolution * 0.5);
    shadowOrigin.y *= (resolution * 0.5);
    shadowOrigin.z *= (resolution * 0.5);

    const Vec4 rounded{
        std::round(shadowOrigin.x),
        std::round(shadowOrigin.y),
        std::round(shadowOrigin.z),
        std::round(shadowOrigin.w)
    };
    const Vec4 roundOffset{
        (rounded.x - shadowOrigin.x) * (2.0 / resolution),
        (rounded.y - shadowOrigin.y) * (2.0 / resolution),
        0.0,
        0.0
    };

    auto rows = projection.Rows();
    rows[0][3] += roundOffset.x;
    rows[1][3] += roundOffset.y;
    return Math::Matrix4(rows);
}

} // namespace Lvs::Engine::Rendering::Vulkan
