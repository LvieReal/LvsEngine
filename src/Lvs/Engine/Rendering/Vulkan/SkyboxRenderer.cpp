#include "Lvs/Engine/Rendering/Vulkan/SkyboxRenderer.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"
#include "Lvs/Engine/Rendering/Common/Primitives.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBinding.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanVertexLayout.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

namespace {

VulkanBindingLayout& GetBindingLayout(Common::BindingLayout& layout) {
    return dynamic_cast<VulkanBindingLayout&>(layout);
}

VulkanResourceBinding& GetBinding(Common::ResourceBinding& binding) {
    return dynamic_cast<VulkanResourceBinding&>(binding);
}

} // namespace

SkyboxRenderer::~SkyboxRenderer() = default;

void SkyboxRenderer::Initialize(Common::GraphicsContext& context) {
    Initialize(static_cast<VulkanContext&>(context));
}

void SkyboxRenderer::RecreateSwapchain(Common::GraphicsContext& context) {
    RecreateSwapchain(static_cast<VulkanContext&>(context));
}

void SkyboxRenderer::Shutdown(Common::GraphicsContext& context) {
    Shutdown(static_cast<VulkanContext&>(context));
}

void SkyboxRenderer::Initialize(VulkanContext& context) {
    if (initialized_) {
        return;
    }

    skyboxMesh_ = std::make_shared<Common::Mesh>(Common::Primitives::GenerateCube());
    bindingLayout_ = CreateBindingLayout(context);
    CreatePipelineLayout(context);
    bindings_ = CreateBindings(context);
    UpdateResources(context);
    CreateGraphicsPipelines(context);
    initialized_ = true;
}

void SkyboxRenderer::RecreateSwapchain(VulkanContext& context) {
    if (!initialized_) {
        Initialize(context);
        return;
    }
    DestroySwapchainResources(context);
    CreateGraphicsPipelines(context);
}

void SkyboxRenderer::Shutdown(VulkanContext& context) {
    DestroySwapchainResources(context);

    bindings_.clear();

    pipelineLayout_.reset();
    bindingLayout_.reset();

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

void SkyboxRenderer::UpdateResources(Common::GraphicsContext& context) {
    UpdateResources(static_cast<VulkanContext&>(context));
}

void SkyboxRenderer::WriteSceneBinding(Common::GraphicsContext& context, Common::ResourceBinding& binding) const {
    static_cast<void>(context);
    if (cubemap_.View == VK_NULL_HANDLE || cubemap_.Sampler == VK_NULL_HANDLE) {
        return;
    }

    const VkDescriptorImageInfo skyInfo{
        .sampler = cubemap_.Sampler,
        .imageView = cubemap_.View,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    GetBinding(binding).UpdateImage(1, skyInfo);
}

void SkyboxRenderer::Draw(
    Common::GraphicsContext& context,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex,
    const Objects::Camera& camera
) {
    Draw(static_cast<VulkanContext&>(context), commandBuffer, frameIndex, camera);
}

void SkyboxRenderer::Draw(
    VulkanContext& context,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex,
    const Objects::Camera& camera
) {
    if (!initialized_ || pipelines_.empty() || skyboxMesh_ == nullptr || bindings_.empty()) {
        return;
    }

    const Common::PipelineVariantKey pipelineKey{
        .CullMode = Common::PipelineCullMode::Front,
        .DepthMode = Common::PipelineDepthMode::ReadOnly,
        .BlendMode = Common::PipelineBlendMode::Opaque
    };
    GetPipeline(pipelineKey).Bind(commandBuffer);
    bindings_[frameIndex]->Bind(commandBuffer, *pipelineLayout_, 0);

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
    commandBuffer.PushConstants(
        *pipelineLayout_,
        Common::ShaderStageFlags::Vertex | Common::ShaderStageFlags::Fragment,
        &packedFloat,
        sizeof(packedFloat)
    );

    skyboxMesh_->EnsureUploaded(context);
    skyboxMesh_->Draw(commandBuffer);
}

const CubemapUtils::CubemapHandle& SkyboxRenderer::GetCubemap() const {
    return cubemap_;
}

Math::Color3 SkyboxRenderer::GetSkyTint() const {
    return skyTint_;
}

std::unique_ptr<Common::BindingLayout> SkyboxRenderer::CreateBindingLayout(VulkanContext& context) const {
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
    const std::vector<VkDescriptorPoolSize> poolSizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = context.GetFramesInFlight()
        }
    };
    return VulkanBindingLayout::Create(context.GetDevice(), createInfo, poolSizes, context.GetFramesInFlight());
}

void SkyboxRenderer::CreatePipelineLayout(VulkanContext& context) {
    const VkDescriptorSetLayout descriptorSetLayout = GetBindingLayout(*bindingLayout_).GetLayoutHandle();
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
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    pipelineLayout_ = VulkanPipelineLayout::Create(context.GetDevice(), createInfo);
}

void SkyboxRenderer::CreateGraphicsPipelines(VulkanContext& context) {
    pipelines_.clear();
    for (const auto& key : GetPipelineVariants()) {
        pipelines_.emplace(key, CreateGraphicsPipelineVariant(context, key));
    }
}

std::unique_ptr<VulkanPipelineVariant> SkyboxRenderer::CreateGraphicsPipelineVariant(
    VulkanContext& context,
    const Common::PipelineVariantKey& key
) {
    const VkDevice device = context.GetDevice();
    const auto vertPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/Sky.vert.spv");
    const auto fragPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/Sky.frag.spv");

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
        .layout = pipelineLayout_->GetHandle(),
        .renderPass = context.GetSceneRenderPassHandle(),
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    return VulkanPipelineVariant::CreateGraphicsPipeline(device, pipelineInfo, *pipelineLayout_);
}

std::vector<std::unique_ptr<Common::ResourceBinding>> SkyboxRenderer::CreateBindings(VulkanContext& context) const {
    auto& bindingLayout = GetBindingLayout(*bindingLayout_);
    std::vector<std::unique_ptr<Common::ResourceBinding>> bindings;
    bindings.reserve(context.GetFramesInFlight());
    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        bindings.push_back(bindingLayout.AllocateBinding());
    }
    return bindings;
}

std::vector<Common::PipelineVariantKey> SkyboxRenderer::GetPipelineVariants() const {
    return {{
        .CullMode = Common::PipelineCullMode::Front,
        .DepthMode = Common::PipelineDepthMode::ReadOnly,
        .BlendMode = Common::PipelineBlendMode::Opaque
    }};
}

void SkyboxRenderer::UpdateDescriptorSets(VulkanContext& context) {
    if (bindings_.empty()) {
        return;
    }
    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        const VkDescriptorImageInfo skyInfo{
            .sampler = cubemap_.Sampler,
            .imageView = cubemap_.View,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        GetBinding(*bindings_[i]).UpdateImage(0, skyInfo);
    }
}

void SkyboxRenderer::DestroySwapchainResources(VulkanContext& context) {
    static_cast<void>(context);
    pipelines_.clear();
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

const VulkanPipelineVariant& SkyboxRenderer::GetPipeline(const Common::PipelineVariantKey& key) const {
    const auto it = pipelines_.find(key);
    if (it == pipelines_.end() || it->second == nullptr) {
        throw std::runtime_error("Skybox pipeline variant is not available.");
    }
    return *it->second;
}

} // namespace Lvs::Engine::Rendering::Vulkan
