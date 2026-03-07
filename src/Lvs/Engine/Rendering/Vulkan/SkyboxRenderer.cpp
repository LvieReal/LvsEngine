#include "Lvs/Engine/Rendering/Vulkan/SkyboxRenderer.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"
#include "Lvs/Engine/Rendering/Common/Primitives.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanVertexLayout.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

void SkyboxRenderer::Initialize(VulkanContext& context) {
    if (initialized_) {
        return;
    }

    skyboxMesh_ = std::make_shared<Common::Mesh>(Common::Primitives::GenerateCube());
    CreateDescriptorSetLayout(context);
    CreatePipelineLayout(context);
    CreateDescriptorPool(context);
    CreateDescriptorSets(context);
    UpdateResources(context);
    CreateGraphicsPipeline(context);
    initialized_ = true;
}

void SkyboxRenderer::RecreateSwapchain(VulkanContext& context) {
    if (!initialized_) {
        Initialize(context);
        return;
    }
    DestroySwapchainResources(context);
    CreateGraphicsPipeline(context);
}

void SkyboxRenderer::Shutdown(VulkanContext& context) {
    DestroySwapchainResources(context);

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.GetDevice(), descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    descriptorSets_.clear();

    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.GetDevice(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.GetDevice(), descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }

    CubemapUtils::DestroyCubemap(context.GetDevice(), cubemap_);
    skyboxMesh_.reset();

    if (skyboxPropertyConnection_.has_value()) {
        skyboxPropertyConnection_->Disconnect();
        skyboxPropertyConnection_.reset();
    }

    initialized_ = false;
}

void SkyboxRenderer::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
    skyboxDirty_ = true;
}

void SkyboxRenderer::Unbind() {
    place_.reset();
    skyboxDirty_ = true;
}

void SkyboxRenderer::UpdateResources(VulkanContext& context) {
    if (!initialized_ || context.GetDevice() == VK_NULL_HANDLE) {
        return;
    }

    UpdateSkyFromPlace();
    if (!skyboxDirty_) {
        return;
    }

    std::shared_ptr<Objects::Skybox> sourceSky = activeSkybox_;
    if (sourceSky == nullptr) {
        sourceSky = std::make_shared<Objects::Skybox>();
    }

    const std::array<std::filesystem::path, 6> faces{
        sourceSky->GetProperty("RightTexture").toString().trimmed().toStdString(),
        sourceSky->GetProperty("LeftTexture").toString().trimmed().toStdString(),
        sourceSky->GetProperty("UpTexture").toString().trimmed().toStdString(),
        sourceSky->GetProperty("DownTexture").toString().trimmed().toStdString(),
        sourceSky->GetProperty("FrontTexture").toString().trimmed().toStdString(),
        sourceSky->GetProperty("BackTexture").toString().trimmed().toStdString(),
    };
    skyTint_ = sourceSky->GetProperty("Tint").value<Math::Color3>();
    const auto filtering = sourceSky->GetProperty("Filtering").value<Enums::TextureFiltering>();
    const bool linearFiltering = filtering == Enums::TextureFiltering::Linear;
    const auto textureLayout = sourceSky->GetProperty("TextureLayout").value<Enums::SkyboxTextureLayout>();
    const std::filesystem::path crossTexture = sourceSky->GetProperty("CrossTexture").toString().trimmed().toStdString();
    const int resolutionCap = sourceSky->GetProperty("ResolutionCap").toInt();
    const bool compression = sourceSky->GetProperty("Compression").toBool();

    try {
        CubemapUtils::CubemapHandle newCubemap;
        if (textureLayout == Enums::SkyboxTextureLayout::Cross && !crossTexture.empty()) {
            newCubemap = CubemapUtils::CreateCubemapFromCrossPath(
                context.GetPhysicalDevice(),
                context.GetDevice(),
                context.GetGraphicsQueue(),
                context.GetGraphicsQueueFamily(),
                crossTexture,
                linearFiltering,
                resolutionCap,
                compression
            );
        } else {
            newCubemap = CubemapUtils::CreateCubemapFromPaths(
                context.GetPhysicalDevice(),
                context.GetDevice(),
                context.GetGraphicsQueue(),
                context.GetGraphicsQueueFamily(),
                faces,
                linearFiltering,
                resolutionCap,
                compression
            );
        }
        CubemapUtils::DestroyCubemap(context.GetDevice(), cubemap_);
        cubemap_ = newCubemap;
        UpdateDescriptorSets(context);
    } catch (const std::exception&) {
    }

    skyboxDirty_ = false;
}

void SkyboxRenderer::Draw(
    VulkanContext& context,
    const VkCommandBuffer commandBuffer,
    const std::uint32_t frameIndex,
    const Objects::Camera& camera
) {
    if (!initialized_ || pipelineVariants_.empty() || skyboxMesh_ == nullptr || descriptorSets_.empty()) {
        return;
    }

    const Common::PipelineVariantKey pipelineKey{
        .CullMode = Common::PipelineCullMode::Front,
        .DepthMode = Common::PipelineDepthMode::ReadOnly,
        .BlendMode = Common::PipelineBlendMode::Opaque
    };
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetPipeline(pipelineKey));
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

    const auto viewProjection = camera.GetProjectionMatrix() * camera.GetViewMatrixNoTranslation();
    const auto packed = viewProjection.FlattenColumnMajor();
    struct SkyPushConstants {
        float ViewProjection[16];
        float Tint[4];
    } packedFloat{};
    for (int i = 0; i < 16; ++i) {
        packedFloat.ViewProjection[i] = static_cast<float>(packed[static_cast<std::size_t>(i)]);
    }
    packedFloat.Tint[0] = static_cast<float>(skyTint_.r);
    packedFloat.Tint[1] = static_cast<float>(skyTint_.g);
    packedFloat.Tint[2] = static_cast<float>(skyTint_.b);
    packedFloat.Tint[3] = 1.0F;
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout_,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(packedFloat),
        &packedFloat
    );

    skyboxMesh_->EnsureUploaded(context);
    VulkanRenderCommandBuffer renderCommandBuffer(commandBuffer);
    skyboxMesh_->Draw(renderCommandBuffer);
}

const CubemapUtils::CubemapHandle& SkyboxRenderer::GetCubemap() const {
    return cubemap_;
}

Math::Color3 SkyboxRenderer::GetSkyTint() const {
    return skyTint_;
}

void SkyboxRenderer::CreateDescriptorSetLayout(VulkanContext& context) {
    const VkDescriptorSetLayoutBinding skyBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };
    const VkDescriptorSetLayoutCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &skyBinding
    };
    if (vkCreateDescriptorSetLayout(context.GetDevice(), &createInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sky descriptor set layout.");
    }
}

void SkyboxRenderer::CreatePipelineLayout(VulkanContext& context) {
    const VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(float) * 20
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
        throw std::runtime_error("Failed to create sky pipeline layout.");
    }
}

void SkyboxRenderer::CreateGraphicsPipeline(VulkanContext& context) {
    pipelineVariants_.clear();
    const Common::PipelineVariantKey key{
        .CullMode = Common::PipelineCullMode::Front,
        .DepthMode = Common::PipelineDepthMode::ReadOnly,
        .BlendMode = Common::PipelineBlendMode::Opaque
    };
    pipelineVariants_.emplace(key, CreateGraphicsPipelineVariant(context, key));
}

VkPipeline SkyboxRenderer::CreateGraphicsPipelineVariant(VulkanContext& context, const Common::PipelineVariantKey& key) {
    const VkDevice device = context.GetDevice();
    const auto vertPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/Sky.vert.spv");
    const auto fragPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/Sky.frag.spv");

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
        .depthCompareOp = VK_COMPARE_OP_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0F,
        .maxDepthBounds = 1.0F
    };
    const VkPipelineColorBlendAttachmentState sceneBlendAttachment{
        .blendEnable = key.BlendMode == Common::PipelineBlendMode::AlphaBlend ? VK_TRUE : VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
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
        .colorWriteMask = 0
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
        throw std::runtime_error("Failed to create sky pipeline.");
    }

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);
    return pipeline;
}

void SkyboxRenderer::CreateDescriptorPool(VulkanContext& context) {
    const VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = context.GetFramesInFlight()
    };
    const VkDescriptorPoolCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = context.GetFramesInFlight(),
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    if (vkCreateDescriptorPool(context.GetDevice(), &createInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sky descriptor pool.");
    }
}

void SkyboxRenderer::CreateDescriptorSets(VulkanContext& context) {
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
        throw std::runtime_error("Failed to allocate sky descriptor sets.");
    }
}

void SkyboxRenderer::UpdateDescriptorSets(VulkanContext& context) {
    if (descriptorSets_.empty()) {
        return;
    }
    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        const VkDescriptorImageInfo skyInfo{
            .sampler = cubemap_.Sampler,
            .imageView = cubemap_.View,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSets_[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &skyInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        vkUpdateDescriptorSets(context.GetDevice(), 1, &write, 0, nullptr);
    }
}

void SkyboxRenderer::DestroySwapchainResources(VulkanContext& context) {
    for (auto& [key, pipeline] : pipelineVariants_) {
        static_cast<void>(key);
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.GetDevice(), pipeline, nullptr);
        }
    }
    pipelineVariants_.clear();
}

void SkyboxRenderer::UpdateSkyFromPlace() {
    const auto sky = GetSkybox(GetLighting());
    if (sky != activeSkybox_) {
        if (skyboxPropertyConnection_.has_value()) {
            skyboxPropertyConnection_->Disconnect();
            skyboxPropertyConnection_.reset();
        }
        activeSkybox_ = sky;
        if (activeSkybox_ != nullptr) {
            skyboxPropertyConnection_ = activeSkybox_->PropertyInvalidated.Connect([this]() {
                skyboxDirty_ = true;
            });
        }
        skyboxDirty_ = true;
    }
}

std::shared_ptr<DataModel::Lighting> SkyboxRenderer::GetLighting() const {
    if (place_ == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
}

std::shared_ptr<Objects::Skybox> SkyboxRenderer::GetSkybox(const std::shared_ptr<DataModel::Lighting>& lighting) const {
    if (lighting == nullptr) {
        return nullptr;
    }
    for (const auto& child : lighting->GetChildren()) {
        if (const auto sky = std::dynamic_pointer_cast<Objects::Skybox>(child); sky != nullptr) {
            return sky;
        }
    }
    return nullptr;
}

VkPipeline SkyboxRenderer::GetPipeline(const Common::PipelineVariantKey& key) const {
    const auto it = pipelineVariants_.find(key);
    if (it == pipelineVariants_.end() || it->second == VK_NULL_HANDLE) {
        throw std::runtime_error("Skybox pipeline variant is not available.");
    }
    return it->second;
}

} // namespace Lvs::Engine::Rendering::Vulkan
