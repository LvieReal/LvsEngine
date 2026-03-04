#include "Lvs/Engine/Rendering/Vulkan/Renderer.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Mesh.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Vulkan/Vertex.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <QFileInfo>

#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <stdexcept>
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

} // namespace

void Renderer::Initialize(VulkanContext& context) {
    context_ = &context;
    if (initialized_) {
        return;
    }

    meshCache_.Initialize();
    CreateDescriptorSetLayout(context);
    CreatePipelineLayout(context);
    CreateUniformBuffers(context);
    CreateDescriptorPool(context);
    CreateDescriptorSets(context);
    CreateGraphicsPipeline(context);

    skyboxRenderer_.Initialize(context);
    shadowRenderer_.Initialize(context);
    CreateSurfaceTextures(context);

    initialized_ = true;
}

void Renderer::RecreateSwapchain(VulkanContext& context) {
    context_ = &context;
    if (!initialized_) {
        Initialize(context);
        return;
    }

    DestroySwapchainResources(context);
    CreateGraphicsPipeline(context);
    skyboxRenderer_.RecreateSwapchain(context);
    shadowRenderer_.RecreateSwapchain(context);
}

void Renderer::DestroySwapchainResources(VulkanContext& context) {
    const VkDevice device = context.GetDevice();
    if (graphicsPipelineBackCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipelineBackCull_, nullptr);
        graphicsPipelineBackCull_ = VK_NULL_HANDLE;
    }
    if (graphicsPipelineFrontCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipelineFrontCull_, nullptr);
        graphicsPipelineFrontCull_ = VK_NULL_HANDLE;
    }
    if (graphicsPipelineNoCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipelineNoCull_, nullptr);
        graphicsPipelineNoCull_ = VK_NULL_HANDLE;
    }
    if (transparentPipelineBackCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, transparentPipelineBackCull_, nullptr);
        transparentPipelineBackCull_ = VK_NULL_HANDLE;
    }
    if (transparentPipelineFrontCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, transparentPipelineFrontCull_, nullptr);
        transparentPipelineFrontCull_ = VK_NULL_HANDLE;
    }
    if (transparentPipelineNoCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, transparentPipelineNoCull_, nullptr);
        transparentPipelineNoCull_ = VK_NULL_HANDLE;
    }
    if (alwaysOnTopPipelineBackCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, alwaysOnTopPipelineBackCull_, nullptr);
        alwaysOnTopPipelineBackCull_ = VK_NULL_HANDLE;
    }
    if (alwaysOnTopPipelineNoCull_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, alwaysOnTopPipelineNoCull_, nullptr);
        alwaysOnTopPipelineNoCull_ = VK_NULL_HANDLE;
    }
}

void Renderer::Shutdown(VulkanContext& context) {
    context_ = &context;
    const VkDevice device = context.GetDevice();

    DestroySwapchainResources(context);

    skyboxRenderer_.Shutdown(context);
    shadowRenderer_.Shutdown(context);
    DestroySurfaceTextures(context);

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }

    for (auto& uniform : uniformBuffers_) {
        BufferUtils::DestroyBuffer(device, uniform);
    }
    uniformBuffers_.clear();
    descriptorSets_.clear();
    boundSkyImageView_ = VK_NULL_HANDLE;
    boundSurfaceAtlasView_ = VK_NULL_HANDLE;
    boundSurfaceNormalAtlasView_ = VK_NULL_HANDLE;

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }

    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    meshCache_.Destroy(device);
    initialized_ = false;
}

void Renderer::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
    workspace_ = place_ != nullptr ? std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace")) : nullptr;
    scene_.Build(place_);
    skyboxRenderer_.BindToPlace(place_);
}

void Renderer::Unbind() {
    place_.reset();
    workspace_.reset();
    scene_.Build(nullptr);
    skyboxRenderer_.Unbind();
    overlayPrimitives_.clear();
}

void Renderer::SetOverlayPrimitives(std::vector<OverlayPrimitive> primitives) {
    overlayPrimitives_ = std::move(primitives);
}

void Renderer::RecordShadowCommands(
    VulkanContext& context,
    const VkCommandBuffer commandBuffer,
    const std::uint32_t frameIndex
) {
    static_cast<void>(frameIndex);
    context_ = &context;
    if (!initialized_ || place_ == nullptr || workspace_ == nullptr) {
        return;
    }

    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }
    camera->Resize(GetAspect(context));

    const auto cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
    scene_.BuildDrawLists(*this, cameraPosition);

    ShadowRenderer::ShadowSettings shadowSettings{};
    const auto lighting = GetLighting();
    auto directionalLight = lighting != nullptr ? GetDirectionalLight(lighting) : nullptr;
    Math::Vector3 lightDirection{};

    if (lighting != nullptr) {
        shadowSettings.Enabled = lighting->GetProperty("DefaultShadowsEnabled").toBool();
        shadowSettings.BlurAmount = static_cast<float>(lighting->GetProperty("DefaultShadowBlur").toDouble());
        shadowSettings.TapCount = lighting->GetProperty("DefaultShadowTapCount").toInt();
        shadowSettings.CascadeCount = lighting->GetProperty("DefaultShadowCascadeCount").toInt();
        shadowSettings.MaxDistance = static_cast<float>(lighting->GetProperty("DefaultShadowMaxDistance").toDouble());
        shadowSettings.MapResolution = static_cast<std::uint32_t>(lighting->GetProperty("DefaultShadowMapResolution").toInt());
    }

    if (directionalLight != nullptr && directionalLight->GetProperty("Enabled").toBool()) {
        lightDirection = directionalLight->GetProperty("Direction").value<Math::Vector3>();
    } else {
        shadowSettings.Enabled = false;
    }

    shadowRenderer_.Render(
        context,
        commandBuffer,
        scene_.GetOpaqueProxies(),
        *camera,
        lightDirection,
        GetAspect(context),
        shadowSettings
    );
}

void Renderer::RecordDrawCommands(
    VulkanContext& context,
    const VkCommandBuffer commandBuffer,
    const std::uint32_t frameIndex
) {
    context_ = &context;
    if (!initialized_ || place_ == nullptr || workspace_ == nullptr || graphicsPipelineBackCull_ == VK_NULL_HANDLE) {
        return;
    }

    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }
    camera->Resize(GetAspect(context));

    const VkExtent2D extent = context.GetSwapchainExtent();
    const VkViewport viewport{
        .x = 0.0F,
        .y = 0.0F,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F
    };
    const VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    skyboxRenderer_.UpdateResources(context);
    UpdateMainSkyDescriptorSets(context);
    UpdateShadowDescriptorSets(context);
    UpdateSurfaceDescriptorSets(context);
    UpdateCameraUniformAndLighting(context, frameIndex);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[frameIndex],
        0,
        nullptr
    );

    const auto cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
    scene_.BuildDrawLists(*this, cameraPosition);
    scene_.DrawOpaque(commandBuffer, *this);
    skyboxRenderer_.Draw(context, commandBuffer, frameIndex, *camera);
    scene_.DrawTransparent(commandBuffer, *this);
    for (const auto& primitive : overlayPrimitives_) {
        if (!primitive.AlwaysOnTop) {
            continue;
        }
        DrawOverlayPrimitive(commandBuffer, primitive);
    }
}

MeshCache& Renderer::GetMeshCache() {
    return meshCache_;
}

void Renderer::CreateDescriptorSetLayout(VulkanContext& context) {
    const VkDevice device = context.GetDevice();

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
    if (vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
}

void Renderer::CreatePipelineLayout(VulkanContext& context) {
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
        .pSetLayouts = &descriptorSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    if (vkCreatePipelineLayout(context.GetDevice(), &createInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }
}

void Renderer::CreateGraphicsPipeline(VulkanContext& context) {
    graphicsPipelineBackCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_BACK_BIT, true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    graphicsPipelineFrontCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_FRONT_BIT, true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    graphicsPipelineNoCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_NONE, true, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    transparentPipelineBackCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_BACK_BIT, true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    transparentPipelineFrontCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_FRONT_BIT, true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    transparentPipelineNoCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_NONE, true, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    alwaysOnTopPipelineBackCull_ =
        CreateGraphicsPipelineVariant(context, VK_CULL_MODE_BACK_BIT, false, false, VK_COMPARE_OP_ALWAYS);
    alwaysOnTopPipelineNoCull_ = CreateGraphicsPipelineVariant(context, VK_CULL_MODE_NONE, false, false, VK_COMPARE_OP_ALWAYS);
}

VkPipeline Renderer::CreateGraphicsPipelineVariant(
    VulkanContext& context,
    const VkCullModeFlags cullMode,
    const bool depthTest,
    const bool depthWrite,
    const VkCompareOp depthCompare
) {
    const VkDevice device = context.GetDevice();
    const QString vertPath = Utils::SourcePath::GetResourcePath("Shaders/Vulkan/Main.vert.spv");
    const QString fragPath = Utils::SourcePath::GetResourcePath("Shaders/Vulkan/Main.frag.spv");

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
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = cullMode,
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
        .depthTestEnable = depthTest ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp = depthCompare,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F
    };
    const VkPipelineColorBlendAttachmentState sceneBlendAttachment{
        .blendEnable = VK_TRUE,
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
        .layout = pipelineLayout_,
        .renderPass = context.GetRenderPass(),
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, fragModule, nullptr);
        vkDestroyShaderModule(device, vertModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline variant.");
    }

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);
    return pipeline;
}

void Renderer::CreateUniformBuffers(VulkanContext& context) {
    const VkDeviceSize size = sizeof(CameraUniform);
    uniformBuffers_.resize(context.GetFramesInFlight());
    for (auto& buffer : uniformBuffers_) {
        buffer = BufferUtils::CreateBuffer(
            context.GetPhysicalDevice(),
            context.GetDevice(),
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
    }
}

void Renderer::CreateDescriptorPool(VulkanContext& context) {
    const std::array<VkDescriptorPoolSize, 2> poolSizes{{
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = context.GetFramesInFlight()},
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = context.GetFramesInFlight() * 7
        }
    }};
    const VkDescriptorPoolCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = context.GetFramesInFlight(),
        .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };
    if (vkCreateDescriptorPool(context.GetDevice(), &createInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void Renderer::CreateDescriptorSets(VulkanContext& context) {
    const std::vector<VkDescriptorSetLayout> layouts(context.GetFramesInFlight(), descriptorSetLayout_);
    const VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = descriptorPool_,
        .descriptorSetCount = context.GetFramesInFlight(),
        .pSetLayouts = layouts.data()
    };
    descriptorSets_.resize(context.GetFramesInFlight());
    if (vkAllocateDescriptorSets(context.GetDevice(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets.");
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        const VkDescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffers_[i].Buffer,
            .offset = 0,
            .range = sizeof(CameraUniform)
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr
        };
        vkUpdateDescriptorSets(context.GetDevice(), 1, &write, 0, nullptr);
    }
}

void Renderer::UpdateMainSkyDescriptorSets(VulkanContext& context) {
    const auto& cubemap = skyboxRenderer_.GetCubemap();
    if (cubemap.View == VK_NULL_HANDLE || cubemap.Sampler == VK_NULL_HANDLE || cubemap.View == boundSkyImageView_) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        const VkDescriptorImageInfo skyInfo{
            .sampler = cubemap.Sampler,
            .imageView = cubemap.View,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &skyInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        vkUpdateDescriptorSets(context.GetDevice(), 1, &write, 0, nullptr);
    }
    boundSkyImageView_ = cubemap.View;
}

void Renderer::UpdateShadowDescriptorSets(VulkanContext& context) {
    const auto sampler = shadowRenderer_.GetShadowSampler();
    const auto& views = shadowRenderer_.GetShadowImageViews();
    const auto jitterSampler = shadowRenderer_.GetJitterSampler();
    const auto jitterView = shadowRenderer_.GetJitterImageView();
    if (sampler == VK_NULL_HANDLE || views[0] == VK_NULL_HANDLE || views[1] == VK_NULL_HANDLE || views[2] == VK_NULL_HANDLE ||
        jitterSampler == VK_NULL_HANDLE || jitterView == VK_NULL_HANDLE) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        const std::array<VkDescriptorImageInfo, 3> shadowInfos{{
            VkDescriptorImageInfo{
                .sampler = sampler,
                .imageView = views[0],
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            },
            VkDescriptorImageInfo{
                .sampler = sampler,
                .imageView = views[1],
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            },
            VkDescriptorImageInfo{
                .sampler = sampler,
                .imageView = views[2],
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            }
        }};
        const VkWriteDescriptorSet shadowWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<std::uint32_t>(shadowInfos.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = shadowInfos.data(),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        const VkDescriptorImageInfo jitterInfo{
            .sampler = jitterSampler,
            .imageView = jitterView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet jitterWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &jitterInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        const std::array writes{shadowWrite, jitterWrite};
        vkUpdateDescriptorSets(context.GetDevice(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void Renderer::CreateSurfaceTextures(VulkanContext& context) {
    DestroySurfaceTextures(context);

    const auto device = context.GetDevice();
    const auto physicalDevice = context.GetPhysicalDevice();
    const auto queue = context.GetGraphicsQueue();
    const auto queueFamily = context.GetGraphicsQueueFamily();

    const QString atlasPath = Utils::SourcePath::GetResourcePath("Surfaces/Surfaces2.png");
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

    const QString normalAtlasPath = Utils::SourcePath::GetResourcePath("Surfaces/Surfaces2_normals.png");
    hasSurfaceNormalAtlas_ = QFileInfo::exists(normalAtlasPath);
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

void Renderer::DestroySurfaceTextures(VulkanContext& context) {
    TextureUtils::DestroyTexture2D(context.GetDevice(), surfaceAtlas_);
    TextureUtils::DestroyTexture2D(context.GetDevice(), surfaceNormalAtlas_);
    hasSurfaceNormalAtlas_ = false;
    boundSurfaceAtlasView_ = VK_NULL_HANDLE;
    boundSurfaceNormalAtlasView_ = VK_NULL_HANDLE;
}

void Renderer::UpdateSurfaceDescriptorSets(VulkanContext& context) {
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
        const VkWriteDescriptorSet atlasWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 4,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &atlasInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        const VkDescriptorImageInfo normalInfo{
            .sampler = normalSampler,
            .imageView = normalView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet normalWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 5,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &normalInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        const std::array writes{atlasWrite, normalWrite};
        vkUpdateDescriptorSets(context.GetDevice(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    boundSurfaceAtlasView_ = surfaceAtlas_.View;
    boundSurfaceNormalAtlasView_ = normalView;
}

void Renderer::UpdateCameraUniformAndLighting(VulkanContext& context, const std::uint32_t frameIndex) {
    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }

    CameraUniform uniform{};
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
    uniform.LightSpecular[1] = 32.0F;
    uniform.LightSpecular[2] = 0.0F;
    uniform.LightSpecular[3] = 0.0F;
    uniform.Ambient[0] = 0.1F;
    uniform.Ambient[1] = 0.1F;
    uniform.Ambient[2] = 0.1F;
    uniform.Ambient[3] = 1.0F;

    const auto skyTint = skyboxRenderer_.GetSkyTint();
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
    uniform.ShadowCascadeSplits[0] = 220.0F;
    uniform.ShadowCascadeSplits[1] = 220.0F;
    uniform.ShadowCascadeSplits[2] = 220.0F;
    uniform.ShadowCascadeSplits[3] = 1.0F;
    uniform.ShadowParams[0] = 0.25F;
    uniform.ShadowParams[1] = 0.0F;
    uniform.ShadowParams[2] = 16.0F;
    uniform.ShadowParams[3] = 0.25F;
    uniform.ShadowState[0] = 0.0F;
    uniform.ShadowState[1] = 1.0F / 16.0F;
    uniform.ShadowState[2] = 1.0F / 16.0F;
    uniform.ShadowState[3] = 0.0F;

    const auto lighting = GetLighting();
    if (lighting != nullptr) {
        const auto ambient = lighting->GetProperty("Ambient").value<Math::Color3>();
        const float ambientStrength = static_cast<float>(lighting->GetProperty("AmbientStrength").toDouble());
        uniform.Ambient[0] = static_cast<float>(ambient.r) * ambientStrength;
        uniform.Ambient[1] = static_cast<float>(ambient.g) * ambientStrength;
        uniform.Ambient[2] = static_cast<float>(ambient.b) * ambientStrength;
        uniform.RenderSettings[1] = static_cast<float>(lighting->GetProperty("Shading").value<Enums::LightingComputationMode>());
        uniform.RenderSettings[2] = lighting->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F;
        uniform.RenderSettings[3] = lighting->GetProperty("AllowBlackNeon").toBool() ? 1.0F : 0.0F;

        if (const auto light = GetDirectionalLight(lighting); light != nullptr && light->GetProperty("Enabled").toBool()) {
            auto direction = light->GetProperty("Direction").value<Math::Vector3>();
            float dir[3] = {static_cast<float>(direction.x), static_cast<float>(direction.y), static_cast<float>(direction.z)};
            Normalize3(dir);
            uniform.LightDirection[0] = dir[0];
            uniform.LightDirection[1] = dir[1];
            uniform.LightDirection[2] = dir[2];

            const auto lightColor = light->GetProperty("Color").value<Math::Color3>();
            const float intensity = static_cast<float>(light->GetProperty("Intensity").toDouble());
            uniform.LightColorIntensity[0] = static_cast<float>(lightColor.r);
            uniform.LightColorIntensity[1] = static_cast<float>(lightColor.g);
            uniform.LightColorIntensity[2] = static_cast<float>(lightColor.b);
            uniform.LightColorIntensity[3] = intensity;
            uniform.LightSpecular[0] = static_cast<float>(light->GetProperty("SpecularStrength").toDouble());
            uniform.LightSpecular[1] = static_cast<float>(light->GetProperty("Shininess").toDouble());
            uniform.RenderSettings[0] = 1.0F;
        }

        const auto& shadowData = shadowRenderer_.GetShadowData();
        if (shadowData.HasShadowData) {
            for (int cascade = 0; cascade < 3; ++cascade) {
                const auto flattened =
                    shadowData.LightViewProjectionMatrices[static_cast<std::size_t>(cascade)].FlattenColumnMajor();
                for (int i = 0; i < 16; ++i) {
                    uniform.ShadowMatrices[cascade][i] = static_cast<float>(flattened[static_cast<std::size_t>(i)]);
                }
            }
            uniform.ShadowCascadeSplits[0] = shadowData.Split0;
            uniform.ShadowCascadeSplits[1] = shadowData.Split1;
            uniform.ShadowCascadeSplits[2] = shadowData.MaxDistance;
            uniform.ShadowCascadeSplits[3] = static_cast<float>(shadowData.CascadeCount);
            uniform.ShadowParams[0] = shadowData.Bias;
            uniform.ShadowParams[1] = shadowData.BlurAmount;
            uniform.ShadowParams[2] = static_cast<float>(shadowData.TapCount);
            uniform.ShadowParams[3] = shadowData.FadeWidth;
            uniform.ShadowState[0] = 1.0F;
            uniform.ShadowState[1] = shadowData.JitterScaleX;
            uniform.ShadowState[2] = shadowData.JitterScaleY;
        }

        if (!lighting->GetProperty("DefaultShadowsEnabled").toBool()) {
            uniform.ShadowState[0] = 0.0F;
        }
        uniform.ShadowParams[1] = static_cast<float>(lighting->GetProperty("DefaultShadowBlur").toDouble());
        uniform.ShadowParams[2] = static_cast<float>(lighting->GetProperty("DefaultShadowTapCount").toInt());
    }

    void* mapped = nullptr;
    vkMapMemory(context.GetDevice(), uniformBuffers_[frameIndex].Memory, 0, sizeof(CameraUniform), 0, &mapped);
    std::memcpy(mapped, &uniform, sizeof(CameraUniform));
    vkUnmapMemory(context.GetDevice(), uniformBuffers_[frameIndex].Memory);
}

std::shared_ptr<Objects::Camera> Renderer::GetCamera() const {
    if (workspace_ == nullptr) {
        return nullptr;
    }
    return workspace_->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
}

std::shared_ptr<DataModel::Lighting> Renderer::GetLighting() const {
    if (place_ == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
}

std::shared_ptr<Objects::DirectionalLight> Renderer::GetDirectionalLight(
    const std::shared_ptr<DataModel::Lighting>& lighting
) const {
    if (lighting == nullptr) {
        return nullptr;
    }
    for (const auto& child : lighting->GetChildren()) {
        if (const auto light = std::dynamic_pointer_cast<Objects::DirectionalLight>(child); light != nullptr) {
            return light;
        }
    }
    return nullptr;
}

float Renderer::GetAspect(const VulkanContext& context) const {
    const auto extent = context.GetSwapchainExtent();
    if (extent.height == 0) {
        return 1.0F;
    }
    return static_cast<float>(extent.width) / static_cast<float>(extent.height);
}

void Renderer::DrawPart(const VkCommandBuffer commandBuffer, const RenderPartProxy& proxy, const bool transparent) {
    auto mesh = proxy.GetMesh();
    if (mesh == nullptr) {
        return;
    }

    VkPipeline selectedPipeline = graphicsPipelineBackCull_;
    if (!transparent) {
        switch (proxy.GetCullMode()) {
            case Enums::MeshCullMode::NoCull:
                selectedPipeline = graphicsPipelineNoCull_;
                break;
            case Enums::MeshCullMode::Front:
                selectedPipeline = graphicsPipelineFrontCull_;
                break;
            case Enums::MeshCullMode::Back:
            default:
                selectedPipeline = graphicsPipelineBackCull_;
                break;
        }
    } else {
        switch (proxy.GetCullMode()) {
            case Enums::MeshCullMode::NoCull:
                selectedPipeline = transparentPipelineNoCull_;
                break;
            case Enums::MeshCullMode::Front:
                selectedPipeline = transparentPipelineFrontCull_;
                break;
            case Enums::MeshCullMode::Back:
            default:
                selectedPipeline = transparentPipelineBackCull_;
                break;
        }
    }
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, selectedPipeline);

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

    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &push
    );

    if (context_ == nullptr) {
        return;
    }
    mesh->EnsureUploaded(context_->GetPhysicalDevice(), context_->GetDevice());
    mesh->Draw(commandBuffer);
}

void Renderer::DrawOverlayPrimitive(const VkCommandBuffer commandBuffer, const OverlayPrimitive& primitive) {
    const auto mesh = meshCache_.GetPrimitive(primitive.Shape);
    if (mesh == nullptr || context_ == nullptr) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alwaysOnTopPipelineBackCull_);

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

    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants),
        &push
    );

    mesh->EnsureUploaded(context_->GetPhysicalDevice(), context_->GetDevice());
    mesh->Draw(commandBuffer);
}

} // namespace Lvs::Engine::Rendering::Vulkan
