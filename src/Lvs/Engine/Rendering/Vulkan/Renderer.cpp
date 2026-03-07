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
#include "Lvs/Engine/Rendering/Common/Mesh.hpp"
#include "Lvs/Engine/Rendering/Vulkan/RenderPartProxy.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBinding.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanVertexLayout.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

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

Renderer::Renderer(std::shared_ptr<::Lvs::Engine::Rendering::RenderingFactory> factory)
    : factory_(std::move(factory)),
      meshCache_(meshUploader_) {
}

Renderer::~Renderer() = default;

void Renderer::Initialize(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    surface_ = &surface;
    Initialize(static_cast<VulkanContext&>(context));
}

void Renderer::RecreateSwapchain(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    surface_ = &surface;
    RecreateSwapchain(static_cast<VulkanContext&>(context));
}

void Renderer::DestroySwapchainResources(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    static_cast<void>(surface);
    DestroySwapchainResources(static_cast<VulkanContext&>(context));
}

void Renderer::Shutdown(Common::GraphicsContext& context) {
    Shutdown(static_cast<VulkanContext&>(context));
}

void Renderer::Initialize(VulkanContext& context) {
    context_ = &context;
    if (initialized_) {
        return;
    }

    meshCache_.Initialize();
    bindingLayout_ = CreateBindingLayout(context);
    CreatePipelineLayout(context);
    uniformBuffers_ = AllocateUniformBuffers(context);
    bindings_ = CreateResourceBindings(context);
    CreateGraphicsPipelines(context);
    InitializeSubRenderers(context);
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
    CreateGraphicsPipelines(context);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->RecreateSwapchain(context);
    }
    if (shadowRenderer_ != nullptr) {
        shadowRenderer_->RecreateSwapchain(context);
    }
}

void Renderer::DestroySwapchainResources(VulkanContext& context) {
    static_cast<void>(context);
    pipelines_.clear();
}

void Renderer::Shutdown(VulkanContext& context) {
    context_ = &context;
    DestroySwapchainResources(context);

    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->Shutdown(context);
    }
    if (shadowRenderer_ != nullptr) {
        shadowRenderer_->Shutdown(context);
    }
    DestroySurfaceTextures(context);

    for (auto& uniform : uniformBuffers_) {
        BufferUtils::DestroyBuffer(context.GetDevice(), uniform);
    }
    uniformBuffers_.clear();
    bindings_.clear();
    boundSkyImageView_ = VK_NULL_HANDLE;
    boundSurfaceAtlasView_ = VK_NULL_HANDLE;
    boundSurfaceNormalAtlasView_ = VK_NULL_HANDLE;

    pipelineLayout_.reset();
    bindingLayout_.reset();

    meshCache_.Clear();
    initialized_ = false;
}

void Renderer::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
    workspace_ = place_ != nullptr ? std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace")) : nullptr;
    scene_.Build(place_);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->BindToPlace(place_);
    }
}

void Renderer::Unbind() {
    place_.reset();
    workspace_.reset();
    scene_.Build(nullptr);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->Unbind();
    }
    overlayPrimitives_.clear();
}

void Renderer::SetOverlayPrimitives(std::vector<Rendering::Common::OverlayPrimitive> primitives) {
    overlayPrimitives_ = std::move(primitives);
}

void Renderer::RecordShadowCommands(
    Common::GraphicsContext& context,
    const Common::RenderSurface& surface,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t frameIndex
) {
    static_cast<void>(surface);
    static_cast<void>(frameIndex);
    auto& vkContext = static_cast<VulkanContext&>(context);
    context_ = &vkContext;
    if (!initialized_ || place_ == nullptr || workspace_ == nullptr) {
        return;
    }

    const auto camera = GetCamera();
    if (camera == nullptr) {
        return;
    }
    camera->Resize(GetAspect(vkContext));

    const auto cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
    scene_.BuildDrawLists(*this, cameraPosition);

    Common::ShadowRenderer::ShadowSettings shadowSettings{};
    const auto lighting = GetLighting();
    auto directionalLight = lighting != nullptr ? GetDirectionalLight(lighting) : nullptr;
    Math::Vector3 lightDirection{};

    if (lighting != nullptr) {
        shadowSettings.Enabled = lighting->GetProperty("ShadowsEnabled").toBool();
        shadowSettings.BlurAmount = static_cast<float>(lighting->GetProperty("ShadowBlur").toDouble());
        shadowSettings.TapCount = lighting->GetProperty("DefaultShadowTapCount").toInt();
        shadowSettings.CascadeCount = lighting->GetProperty("DefaultShadowCascadeCount").toInt();
        shadowSettings.MaxDistance = static_cast<float>(lighting->GetProperty("DefaultShadowMaxDistance").toDouble());
        shadowSettings.MapResolution = static_cast<std::uint32_t>(lighting->GetProperty("DefaultShadowMapResolution").toInt());
        shadowSettings.CascadeResolutionScale =
            static_cast<float>(lighting->GetProperty("DefaultShadowCascadeResolutionScale").toDouble());
        shadowSettings.CascadeSplitLambda = static_cast<float>(lighting->GetProperty("DefaultShadowCascadeSplitLambda").toDouble());
    }

    if (directionalLight != nullptr && directionalLight->GetProperty("Enabled").toBool()) {
        lightDirection = directionalLight->GetProperty("Direction").value<Math::Vector3>();
    } else {
        shadowSettings.Enabled = false;
    }

    if (shadowRenderer_ != nullptr) {
        std::vector<std::shared_ptr<Common::RenderProxy>> shadowCasters;
        const auto& opaqueProxies = scene_.GetOpaqueProxies();
        shadowCasters.reserve(opaqueProxies.size());
        for (const auto& proxy : opaqueProxies) {
            shadowCasters.push_back(proxy);
        }
        shadowRenderer_->Render(
            vkContext,
            commandBuffer,
            shadowCasters,
            *camera,
            lightDirection,
            GetAspect(vkContext),
            shadowSettings
        );
    }
}

void Renderer::RecordDrawCommands(
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
    context_ = &vkContext;
    if (!initialized_ || place_ == nullptr || workspace_ == nullptr || pipelines_.empty()) {
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
        skyboxRenderer_->UpdateResources(vkContext);
    }
    UpdateMainSkyDescriptorSets(vkContext);
    UpdateShadowDescriptorSets(vkContext);
    UpdateSurfaceDescriptorSets(vkContext);
    UpdateCameraUniformAndLighting(vkContext, frameContext);

    GetResourceBinding(frameContext.FrameIndex).Bind(frameContext.Commands, *pipelineLayout_, 0);

    const auto cameraPosition = camera->GetProperty("CFrame").value<Math::CFrame>().Position;
    scene_.BuildDrawLists(*this, cameraPosition);
    scene_.DrawOpaque(frameContext.Commands, *this);
    if (skyboxRenderer_ != nullptr) {
        skyboxRenderer_->Draw(vkContext, frameContext.Commands, frameContext.FrameIndex, *camera);
    }
    scene_.DrawTransparent(frameContext.Commands, *this);
    for (const auto& primitive : overlayPrimitives_) {
        DrawOverlayPrimitive(frameContext.Commands, primitive);
    }
}

Common::MeshCache& Renderer::GetMeshCache() {
    return meshCache_;
}

std::unique_ptr<Common::BindingLayout> Renderer::CreateBindingLayout(VulkanContext& context) const {
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

void Renderer::CreatePipelineLayout(VulkanContext& context) {
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

void Renderer::CreateGraphicsPipelines(VulkanContext& context) {
    pipelines_.clear();
    for (const auto& key : GetPipelineVariants()) {
        pipelines_.emplace(key, CreateGraphicsPipelineVariant(context, key));
    }
}

std::unique_ptr<VulkanPipelineVariant> Renderer::CreateGraphicsPipelineVariant(
    VulkanContext& context,
    const Common::PipelineVariantKey& key
) {
    const VkDevice device = context.GetDevice();
    const auto vertPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/Main.vert.spv");
    const auto fragPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/Main.frag.spv");

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
        .renderPass = surface_ != nullptr ? reinterpret_cast<VkRenderPass>(surface_->GetSceneRenderPass().GetNativeHandle())
                                          : VK_NULL_HANDLE,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };
    return VulkanPipelineVariant::CreateGraphicsPipeline(device, pipelineInfo, *pipelineLayout_);
}

std::vector<BufferUtils::BufferHandle> Renderer::AllocateUniformBuffers(VulkanContext& context) const {
    const VkDeviceSize size = sizeof(CameraUniform);
    std::vector<BufferUtils::BufferHandle> uniformBuffers(context.GetFramesInFlight());
    for (auto& buffer : uniformBuffers) {
        buffer = BufferUtils::CreateBuffer(
            context.GetPhysicalDevice(),
            context.GetDevice(),
            size,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
    }
    return uniformBuffers;
}

std::vector<std::unique_ptr<Common::ResourceBinding>> Renderer::CreateResourceBindings(VulkanContext& context) const {
    const auto& bindingLayout = dynamic_cast<const VulkanBindingLayout&>(*bindingLayout_);
    std::vector<std::unique_ptr<Common::ResourceBinding>> bindings;
    bindings.reserve(context.GetFramesInFlight());

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        bindings.push_back(bindingLayout.AllocateBinding());
        const VkDescriptorBufferInfo bufferInfo{
            .buffer = uniformBuffers_[i].Buffer,
            .offset = 0,
            .range = sizeof(CameraUniform)
        };
        dynamic_cast<VulkanResourceBinding&>(*bindings.back()).UpdateBuffer(0, bufferInfo);
    }
    return bindings;
}

std::vector<Common::PipelineVariantKey> Renderer::GetPipelineVariants() const {
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

void Renderer::InitializeSubRenderers(VulkanContext& context) {
    if (skyboxRenderer_ == nullptr) {
        skyboxRenderer_ = factory_ != nullptr ? factory_->CreateSkyboxRenderer() : std::make_unique<VulkanSkyboxRenderer>();
    }
    if (shadowRenderer_ == nullptr) {
        shadowRenderer_ = factory_ != nullptr ? factory_->CreateShadowRenderer() : std::make_unique<VulkanShadowRenderer>();
    }
    skyboxRenderer_->Initialize(context);
    shadowRenderer_->Initialize(context);
}

void Renderer::UpdateMainSkyDescriptorSets(VulkanContext& context) {
    if (skyboxRenderer_ == nullptr) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        skyboxRenderer_->WriteSceneBinding(context, *bindings_[i]);
    }
    boundSkyImageView_ = VK_NULL_HANDLE;
}

void Renderer::UpdateShadowDescriptorSets(VulkanContext& context) {
    if (shadowRenderer_ == nullptr) {
        return;
    }

    for (std::uint32_t i = 0; i < context.GetFramesInFlight(); ++i) {
        shadowRenderer_->WriteSceneBinding(context, *bindings_[i]);
    }
}

void Renderer::CreateSurfaceTextures(VulkanContext& context) {
    DestroySurfaceTextures(context);

    const auto device = context.GetDevice();
    const auto physicalDevice = context.GetPhysicalDevice();
    const auto queue = context.GetGraphicsQueue();
    const auto queueFamily = context.GetGraphicsQueueFamily();

    const auto atlasPath = Utils::PathUtils::GetResourcePath("Surfaces/Surfaces2.png");
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

    const auto normalAtlasPath = Utils::PathUtils::GetResourcePath("Surfaces/Surfaces2_normals.png");
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

void Renderer::UpdateCameraUniformAndLighting(VulkanContext& context, const Common::FrameContext& frameContext) {
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
        uniform.RenderSettings[3] = lighting->GetProperty("InaccurateNeon").toBool() ? 1.0F : 0.0F;

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

        const auto* shadowData = shadowRenderer_ != nullptr ? &shadowRenderer_->GetShadowData() : nullptr;
        if (shadowData != nullptr && shadowData->HasShadowData) {
            for (int cascade = 0; cascade < 3; ++cascade) {
                const auto flattened =
                    shadowData->LightViewProjectionMatrices[static_cast<std::size_t>(cascade)].FlattenColumnMajor();
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
            uniform.ShadowState[0] = 1.0F;
            uniform.ShadowState[1] = shadowData->JitterScaleX;
            uniform.ShadowState[2] = shadowData->JitterScaleY;
        }

        if (!lighting->GetProperty("ShadowsEnabled").toBool()) {
            uniform.ShadowState[0] = 0.0F;
        }
        uniform.ShadowParams[1] = static_cast<float>(lighting->GetProperty("ShadowBlur").toDouble());
        uniform.ShadowParams[2] = static_cast<float>(lighting->GetProperty("DefaultShadowTapCount").toInt());
    }

    void* mapped = nullptr;
    vkMapMemory(context.GetDevice(), uniformBuffers_[frameContext.FrameIndex].Memory, 0, sizeof(CameraUniform), 0, &mapped);
    std::memcpy(mapped, &uniform, sizeof(CameraUniform));
    vkUnmapMemory(context.GetDevice(), uniformBuffers_[frameContext.FrameIndex].Memory);
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
    if (extent.Height == 0) {
        return 1.0F;
    }
    return static_cast<float>(extent.Width) / static_cast<float>(extent.Height);
}

void Renderer::DrawPart(Common::CommandBuffer& commandBuffer, const RenderPartProxy& proxy, const bool transparent) {
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

    if (context_ == nullptr) {
        return;
    }
    mesh->EnsureUploaded(*context_);
    mesh->Draw(commandBuffer);
}

void Renderer::DrawOverlayPrimitive(
    Common::CommandBuffer& commandBuffer,
    const Rendering::Common::OverlayPrimitive& primitive
) {
    const auto mesh = std::dynamic_pointer_cast<Common::Mesh>(meshCache_.GetPrimitive(primitive.Shape));
    if (mesh == nullptr || context_ == nullptr) {
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

    mesh->EnsureUploaded(*context_);
    mesh->Draw(commandBuffer);
}

const VulkanPipelineVariant& Renderer::GetPipeline(const Common::PipelineVariantKey& key) const {
    const auto it = pipelines_.find(key);
    if (it == pipelines_.end() || it->second == nullptr) {
        throw std::runtime_error("Requested graphics pipeline variant is not available.");
    }
    return *it->second;
}

const Common::ResourceBinding& Renderer::GetResourceBinding(const std::uint32_t frameIndex) const {
    if (frameIndex >= bindings_.size() || bindings_[frameIndex] == nullptr) {
        throw std::runtime_error("Requested frame resource binding is not available.");
    }
    return *bindings_[frameIndex];
}

} // namespace Lvs::Engine::Rendering::Vulkan
