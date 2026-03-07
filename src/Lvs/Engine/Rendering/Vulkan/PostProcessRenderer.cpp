#include "Lvs/Engine/Rendering/Vulkan/PostProcessRenderer.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBinding.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanFrameManager.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanGpuResources.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanPipeline.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

PostProcessRenderer::PostProcessRenderer() = default;

PostProcessRenderer::~PostProcessRenderer() = default;

void PostProcessRenderer::Initialize(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    const auto& frameManager = static_cast<const VulkanFrameManager&>(surface);
    std::vector<VkImageView> sceneViews;
    std::vector<VkImageView> glowViews;
    sceneViews.reserve(frameManager.GetImageCount());
    glowViews.reserve(frameManager.GetImageCount());
    for (std::uint32_t i = 0; i < frameManager.GetImageCount(); ++i) {
        const auto sceneImage = frameManager.GetOffscreenColorImage(i);
        const auto glowImage = frameManager.GetOffscreenGlowImage(i);
        sceneViews.push_back(reinterpret_cast<VkImageView>(sceneImage.View));
        glowViews.push_back(reinterpret_cast<VkImageView>(glowImage.View));
    }
    compositeRenderPass_ = &surface.GetPostProcessRenderPass();
    Initialize(
        vkContext,
        sceneViews,
        glowViews,
        reinterpret_cast<VkSampler>(frameManager.GetOffscreenColorImage(0).Sampler)
    );
}

void PostProcessRenderer::RecreateSwapchain(Common::GraphicsContext& context, const Common::RenderSurface& surface) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    const auto& frameManager = static_cast<const VulkanFrameManager&>(surface);
    std::vector<VkImageView> sceneViews;
    std::vector<VkImageView> glowViews;
    sceneViews.reserve(frameManager.GetImageCount());
    glowViews.reserve(frameManager.GetImageCount());
    for (std::uint32_t i = 0; i < frameManager.GetImageCount(); ++i) {
        const auto sceneImage = frameManager.GetOffscreenColorImage(i);
        const auto glowImage = frameManager.GetOffscreenGlowImage(i);
        sceneViews.push_back(reinterpret_cast<VkImageView>(sceneImage.View));
        glowViews.push_back(reinterpret_cast<VkImageView>(glowImage.View));
    }
    compositeRenderPass_ = &surface.GetPostProcessRenderPass();
    RecreateSwapchain(
        vkContext,
        sceneViews,
        glowViews,
        reinterpret_cast<VkSampler>(frameManager.GetOffscreenColorImage(0).Sampler)
    );
}

void PostProcessRenderer::DestroySwapchainResources(Common::GraphicsContext& context) {
    DestroySwapchainResources(static_cast<VulkanContext&>(context));
}

void PostProcessRenderer::Shutdown(Common::GraphicsContext& context) {
    Shutdown(static_cast<VulkanContext&>(context));
}

void PostProcessRenderer::Initialize(
    VulkanContext& context,
    const std::vector<VkImageView>& sceneViews,
    const std::vector<VkImageView>& glowViews,
    const VkSampler sourceSampler
) {
    if (initialized_) {
        return;
    }

    blurSampler_ = sourceSampler;
    CreateRenderPasses(context);
    CreateBlurResources(context, static_cast<std::uint32_t>(sceneViews.size()));
    CreateCompositeBindingLayout(context, static_cast<std::uint32_t>(sceneViews.size()), static_cast<std::uint32_t>(blurExtents_.size()));
    CreateBlurBindingLayout(context, static_cast<std::uint32_t>(sceneViews.size()), static_cast<std::uint32_t>(blurExtents_.size()));
    CreatePipelineLayouts(context);
    CreateBindings(context, sceneViews, glowViews);
    CreatePipelines(context);
    initialized_ = true;
}

void PostProcessRenderer::RecreateSwapchain(
    VulkanContext& context,
    const std::vector<VkImageView>& sceneViews,
    const std::vector<VkImageView>& glowViews,
    const VkSampler sourceSampler
) {
    blurSampler_ = sourceSampler;
    if (!initialized_) {
        Initialize(context, sceneViews, glowViews, sourceSampler);
        return;
    }

    DestroySwapchainResources(context);
    CreateRenderPasses(context);
    CreateBlurResources(context, static_cast<std::uint32_t>(sceneViews.size()));
    CreateCompositeBindingLayout(context, static_cast<std::uint32_t>(sceneViews.size()), static_cast<std::uint32_t>(blurExtents_.size()));
    CreateBlurBindingLayout(context, static_cast<std::uint32_t>(sceneViews.size()), static_cast<std::uint32_t>(blurExtents_.size()));
    CreateBindings(context, sceneViews, glowViews);
    CreatePipelines(context);
}

void PostProcessRenderer::Shutdown(VulkanContext& context) {
    DestroySwapchainResources(context);

    compositePipelineLayout_.reset();
    blurPipelineLayout_.reset();
    compositeBindingLayout_.reset();
    blurBindingLayout_.reset();

    initialized_ = false;
}

void PostProcessRenderer::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
}

void PostProcessRenderer::Unbind() {
    place_.reset();
}

void PostProcessRenderer::RecordBlurCommands(
    Common::GraphicsContext& context,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t imageIndex
) {
    auto& vkContext = static_cast<VulkanContext&>(context);
    const auto& vkCommandBuffer = dynamic_cast<VulkanRenderCommandBuffer&>(commandBuffer);
    const VkCommandBuffer handle = vkCommandBuffer.GetHandle();
    if (!initialized_ || blurDownPipeline_ == nullptr || blurUpPipeline_ == nullptr ||
        imageIndex >= downLevels_.size() || imageIndex >= downBindings_.size() || blurExtents_.empty()) {
        return;
    }

    const std::uint32_t levelsUsed = ComputeUsedBlurLevels();
    if (levelsUsed == 0) {
        return;
    }

    const float blurAmount = std::max(GetBlurAmount(), 1.0F);

    const auto updateSourceDescriptor = [&](Common::ResourceBinding& binding, const VkImageView sourceView) {
        const VkDescriptorImageInfo sourceInfo{
            .sampler = blurSampler_,
            .imageView = sourceView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        GetBinding(binding).UpdateImage(
            0,
            reinterpret_cast<void*>(sourceInfo.sampler),
            reinterpret_cast<void*>(sourceInfo.imageView),
            static_cast<std::uint32_t>(sourceInfo.imageLayout)
        );
    };

    const auto issueSampleBarrier = [&](const VkImage image) {
        const VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vkCmdPipelineBarrier(
            handle,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );
    };

    VkExtent2D currentSourceExtent = vkContext.GetSwapchainExtentVk();

    // Rebind upsample sources for the active blur chain. This prevents sampling stale levels.
    if (levelsUsed > 1 && imageIndex < upBindings_.size()) {
        for (std::uint32_t level = 0; level < (levelsUsed - 1); ++level) {
            const VkImageView sourceView = (level == levelsUsed - 2)
                ? downLevels_[imageIndex][levelsUsed - 1].View
                : upLevels_[imageIndex][level + 1].View;
            updateSourceDescriptor(*upBindings_[imageIndex][level], sourceView);
        }
    }

    const VkImageView finalBlurView = levelsUsed > 1 ? upLevels_[imageIndex][0].View : downLevels_[imageIndex][0].View;
    const VkDescriptorImageInfo compositeGlowInfo{
        .sampler = blurSampler_,
        .imageView = finalBlurView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    GetBinding(*compositeBindings_[imageIndex]).UpdateImage(
        1,
        reinterpret_cast<void*>(compositeGlowInfo.sampler),
        reinterpret_cast<void*>(compositeGlowInfo.imageView),
        static_cast<std::uint32_t>(compositeGlowInfo.imageLayout)
    );

    for (std::uint32_t level = 0; level < levelsUsed; ++level) {
        const VkExtent2D extent = blurExtents_[level];
        const VkClearValue clearValue{.color = {{0.0F, 0.0F, 0.0F, 0.0F}}};
        const VkRenderPassBeginInfo passInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = blurRenderPass_,
            .framebuffer = downLevels_[imageIndex][level].Framebuffer,
            .renderArea = {.offset = {0, 0}, .extent = extent},
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };

        vkCmdBeginRenderPass(handle, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = extent};
        vkCmdSetViewport(handle, 0, 1, &viewport);
        vkCmdSetScissor(handle, 0, 1, &scissor);

        vkCmdBindPipeline(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, blurDownPipeline_->GetHandle());
        const VkDescriptorSet downBinding = GetBinding(*downBindings_[imageIndex][level]).GetHandle();
        vkCmdBindDescriptorSets(
            handle,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            blurPipelineLayout_->GetHandle(),
            0,
            1,
            &downBinding,
            0,
            nullptr
        );

        const std::array<float, 4> settings{
            1.0F / static_cast<float>(currentSourceExtent.width),
            1.0F / static_cast<float>(currentSourceExtent.height),
            blurAmount,
            0.0F
        };
        vkCmdPushConstants(
            handle,
            blurPipelineLayout_->GetHandle(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(settings),
            settings.data()
        );
        vkCmdDraw(handle, 3, 1, 0, 0);
        vkCmdEndRenderPass(handle);

        issueSampleBarrier(downLevels_[imageIndex][level].Image);
        currentSourceExtent = extent;
    }

    if (levelsUsed <= 1 || imageIndex >= upBindings_.size()) {
        return;
    }

    for (std::int32_t level = static_cast<std::int32_t>(levelsUsed) - 2; level >= 0; --level) {
        const VkExtent2D extent = blurExtents_[static_cast<std::uint32_t>(level)];
        const VkClearValue clearValue{.color = {{0.0F, 0.0F, 0.0F, 0.0F}}};
        const VkRenderPassBeginInfo passInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = blurRenderPass_,
            .framebuffer = upLevels_[imageIndex][static_cast<std::uint32_t>(level)].Framebuffer,
            .renderArea = {.offset = {0, 0}, .extent = extent},
            .clearValueCount = 1,
            .pClearValues = &clearValue
        };

        vkCmdBeginRenderPass(handle, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = extent};
        vkCmdSetViewport(handle, 0, 1, &viewport);
        vkCmdSetScissor(handle, 0, 1, &scissor);

        vkCmdBindPipeline(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, blurUpPipeline_->GetHandle());
        const VkDescriptorSet upBinding = GetBinding(*upBindings_[imageIndex][static_cast<std::uint32_t>(level)]).GetHandle();
        vkCmdBindDescriptorSets(
            handle,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            blurPipelineLayout_->GetHandle(),
            0,
            1,
            &upBinding,
            0,
            nullptr
        );

        const std::array<float, 4> settings{
            1.0F / static_cast<float>(currentSourceExtent.width),
            1.0F / static_cast<float>(currentSourceExtent.height),
            blurAmount,
            0.0F
        };
        vkCmdPushConstants(
            handle,
            blurPipelineLayout_->GetHandle(),
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(settings),
            settings.data()
        );
        vkCmdDraw(handle, 3, 1, 0, 0);
        vkCmdEndRenderPass(handle);

        issueSampleBarrier(upLevels_[imageIndex][static_cast<std::uint32_t>(level)].Image);
        currentSourceExtent = extent;
    }
}

void PostProcessRenderer::DrawComposite(
    Common::GraphicsContext& context,
    Common::CommandBuffer& commandBuffer,
    const std::uint32_t imageIndex,
    const std::uint32_t frameIndex
) {
    static_cast<void>(context);
    const auto& vkCommandBuffer = dynamic_cast<VulkanRenderCommandBuffer&>(commandBuffer);
    const VkCommandBuffer handle = vkCommandBuffer.GetHandle();
    if (!initialized_ || compositePipeline_ == nullptr || imageIndex >= compositeBindings_.size()) {
        return;
    }

    vkCmdBindPipeline(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_->GetHandle());
    const VkDescriptorSet compositeBinding = GetBinding(*compositeBindings_[imageIndex]).GetHandle();
    vkCmdBindDescriptorSets(
        handle,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        compositePipelineLayout_->GetHandle(),
        0,
        1,
        &compositeBinding,
        0,
        nullptr
    );

    float gammaEnabled = 0.0F;
    float ditheringEnabled = 0.0F;
    if (const auto lighting = GetLighting(); lighting != nullptr) {
        gammaEnabled = lighting->GetProperty("GammaCorrection").toBool() ? 1.0F : 0.0F;
        ditheringEnabled = lighting->GetProperty("Dithering").toBool() ? 1.0F : 0.0F;
    }
    const std::array<float, 4> settings{
        gammaEnabled,
        ditheringEnabled,
        static_cast<float>((frameIndex + 1) * 17U),
        0.0F
    };
    vkCmdPushConstants(
        handle,
        compositePipelineLayout_->GetHandle(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(settings),
        settings.data()
    );
    vkCmdDraw(handle, 3, 1, 0, 0);
}

void PostProcessRenderer::CreateCompositeBindingLayout(
    VulkanContext& context,
    const std::uint32_t imageCount,
    const std::uint32_t levelCount
) {
    if (imageCount == 0 || levelCount == 0) {
        compositeBindingLayout_.reset();
        return;
    }

    const std::array bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    };
    const VkDescriptorSetLayoutCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<std::uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };
    const std::vector<VkDescriptorPoolSize> poolSizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = imageCount * 2
        }
    };
    compositeBindingLayout_ = VulkanBindingLayout::Create(context.GetDevice(), createInfo, poolSizes, imageCount);
}

void PostProcessRenderer::CreateBlurBindingLayout(
    VulkanContext& context,
    const std::uint32_t imageCount,
    const std::uint32_t levelCount
) {
    if (imageCount == 0 || levelCount == 0) {
        blurBindingLayout_.reset();
        return;
    }

    const VkDescriptorSetLayoutBinding sourceBinding{
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
        .pBindings = &sourceBinding
    };
    const std::uint32_t downSets = imageCount * levelCount;
    const std::uint32_t upSets = imageCount * (levelCount > 1 ? levelCount - 1 : 0);
    const std::vector<VkDescriptorPoolSize> poolSizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = downSets + upSets
        }
    };
    blurBindingLayout_ = VulkanBindingLayout::Create(context.GetDevice(), createInfo, poolSizes, downSets + upSets);
}

void PostProcessRenderer::CreatePipelineLayouts(VulkanContext& context) {
    const VkDescriptorSetLayout compositeBindingLayout = GetBindingLayout(*compositeBindingLayout_).GetLayoutHandle();
    const VkDescriptorSetLayout blurBindingLayout = GetBindingLayout(*blurBindingLayout_).GetLayoutHandle();
    const VkPushConstantRange pushRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(float) * 4
    };

    const VkPipelineLayoutCreateInfo compositeInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &compositeBindingLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    compositePipelineLayout_ = VulkanPipelineLayout::Create(context.GetDevice(), compositeInfo);

    const VkPipelineLayoutCreateInfo blurInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &blurBindingLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    blurPipelineLayout_ = VulkanPipelineLayout::Create(context.GetDevice(), blurInfo);
}

void PostProcessRenderer::CreateBlurResources(VulkanContext& context, const std::uint32_t imageCount) {
    const VkExtent2D swapchainExtent = context.GetSwapchainExtentVk();
    std::uint32_t width = swapchainExtent.width;
    std::uint32_t height = swapchainExtent.height;

    blurExtents_.clear();
    for (std::uint32_t level = 0; level < MAX_BLUR_LEVELS; ++level) {
        width = std::max(width / 2, 1U);
        height = std::max(height / 2, 1U);
        blurExtents_.push_back({width, height});
        if (width == 1 && height == 1) {
            break;
        }
    }

    downLevels_.assign(imageCount, std::vector<BlurImageLevel>(blurExtents_.size()));
    upLevels_.assign(imageCount, std::vector<BlurImageLevel>(blurExtents_.size()));

    const auto createLevel = [&](BlurImageLevel& level, const VkExtent2D extent) {
        const VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = context.GetOffscreenImageFormat(),
            .extent = {.width = extent.width, .height = extent.height, .depth = 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        if (vkCreateImage(context.GetDevice(), &imageInfo, nullptr, &level.Image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blur image.");
        }

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(context.GetDevice(), level.Image, &memReq);
        const VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memReq.size,
            .memoryTypeIndex = BufferUtils::FindMemoryType(
                context.GetPhysicalDevice(),
                memReq.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            )
        };
        if (vkAllocateMemory(context.GetDevice(), &allocInfo, nullptr, &level.Memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate blur image memory.");
        }
        vkBindImageMemory(context.GetDevice(), level.Image, level.Memory, 0);

        const VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = level.Image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = context.GetOffscreenImageFormat(),
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        if (vkCreateImageView(context.GetDevice(), &viewInfo, nullptr, &level.View) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blur image view.");
        }

        const VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .renderPass = blurRenderPass_,
            .attachmentCount = 1,
            .pAttachments = &level.View,
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context.GetDevice(), &framebufferInfo, nullptr, &level.Framebuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create blur framebuffer.");
        }
    };

    for (std::uint32_t image = 0; image < imageCount; ++image) {
        for (std::size_t level = 0; level < blurExtents_.size(); ++level) {
            createLevel(downLevels_[image][level], blurExtents_[level]);
            createLevel(upLevels_[image][level], blurExtents_[level]);
        }
    }
}

void PostProcessRenderer::CreateBindings(
    VulkanContext& context,
    const std::vector<VkImageView>& sceneViews,
    const std::vector<VkImageView>& glowViews
) {
    static_cast<void>(context);
    const std::uint32_t imageCount = static_cast<std::uint32_t>(sceneViews.size());
    const std::uint32_t levelCount = static_cast<std::uint32_t>(blurExtents_.size());
    if (imageCount == 0 || levelCount == 0 || glowViews.size() != sceneViews.size() || blurSampler_ == VK_NULL_HANDLE) {
        return;
    }

    auto& compositeLayout = GetBindingLayout(*compositeBindingLayout_);
    auto& blurLayout = GetBindingLayout(*blurBindingLayout_);
    compositeBindings_.clear();
    compositeBindings_.reserve(imageCount);
    downBindings_.clear();
    downBindings_.resize(imageCount);
    upBindings_.clear();
    upBindings_.resize(imageCount);

    for (std::uint32_t image = 0; image < imageCount; ++image) {
        compositeBindings_.push_back(compositeLayout.AllocateBinding());
        downBindings_[image].reserve(levelCount);
        for (std::uint32_t level = 0; level < levelCount; ++level) {
            downBindings_[image].push_back(blurLayout.AllocateBinding());
            const VkImageView sourceView = (level == 0) ? glowViews[image] : downLevels_[image][level - 1].View;
            const VkDescriptorImageInfo sourceInfo{
                .sampler = blurSampler_,
                .imageView = sourceView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            GetBinding(*downBindings_[image][level]).UpdateImage(
                0,
                reinterpret_cast<void*>(sourceInfo.sampler),
                reinterpret_cast<void*>(sourceInfo.imageView),
                static_cast<std::uint32_t>(sourceInfo.imageLayout)
            );
        }

        if (levelCount > 1) {
            upBindings_[image].reserve(levelCount - 1);
            for (std::uint32_t level = 0; level < (levelCount - 1); ++level) {
                upBindings_[image].push_back(blurLayout.AllocateBinding());
                const VkImageView sourceView = (level == levelCount - 2)
                    ? downLevels_[image][levelCount - 1].View
                    : upLevels_[image][level + 1].View;
                const VkDescriptorImageInfo sourceInfo{
                    .sampler = blurSampler_,
                    .imageView = sourceView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
                GetBinding(*upBindings_[image][level]).UpdateImage(
                    0,
                    reinterpret_cast<void*>(sourceInfo.sampler),
                    reinterpret_cast<void*>(sourceInfo.imageView),
                    static_cast<std::uint32_t>(sourceInfo.imageLayout)
                );
            }
        }

        const VkImageView finalBlurView = levelCount > 1 ? upLevels_[image][0].View : downLevels_[image][0].View;
        const std::array imageInfos{
            VkDescriptorImageInfo{
                .sampler = blurSampler_,
                .imageView = sceneViews[image],
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            VkDescriptorImageInfo{
                .sampler = blurSampler_,
                .imageView = finalBlurView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        };
        GetBinding(*compositeBindings_[image]).UpdateImage(
            0,
            reinterpret_cast<void*>(imageInfos[0].sampler),
            reinterpret_cast<void*>(imageInfos[0].imageView),
            static_cast<std::uint32_t>(imageInfos[0].imageLayout)
        );
        GetBinding(*compositeBindings_[image]).UpdateImage(
            1,
            reinterpret_cast<void*>(imageInfos[1].sampler),
            reinterpret_cast<void*>(imageInfos[1].imageView),
            static_cast<std::uint32_t>(imageInfos[1].imageLayout)
        );
    }
}

void PostProcessRenderer::CreateRenderPasses(VulkanContext& context) {
    const VkAttachmentDescription colorAttachment{
        .flags = 0,
        .format = context.GetOffscreenImageFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };
    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };
    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0
    };

    const VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    if (vkCreateRenderPass(context.GetDevice(), &renderPassInfo, nullptr, &blurRenderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create blur render pass.");
    }
}

void PostProcessRenderer::CreatePipelines(VulkanContext& context) {
    const VkDevice device = context.GetDevice();

    const auto vertPath = Utils::PathUtils::GetResourcePath("Shaders/Vulkan/PostProcess.vert.spv");
    const auto vertCode = ShaderUtils::ReadBinaryFile(vertPath);
    const auto vertModule = VulkanShaderModule::Create(device, vertCode);

    const auto createPipeline = [&](
        const std::filesystem::path& fragPath,
        const VkRenderPass renderPass,
        const VkPipelineLayout pipelineLayout,
        const Common::PipelineLayout& layout
    ) {
        const auto fragCode = ShaderUtils::ReadBinaryFile(fragPath);
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

        const VkPipelineVertexInputStateCreateInfo vertexInput{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr
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
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0F,
            .maxDepthBounds = 1.0F
        };
        const VkPipelineColorBlendAttachmentState colorBlendAttachment{
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
        const VkPipelineColorBlendStateCreateInfo colorBlend{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
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
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        return VulkanPipelineVariant::CreateGraphicsPipeline(device, pipelineInfo, layout);
    };

    compositePipeline_ = createPipeline(
        Utils::PathUtils::GetResourcePath("Shaders/Vulkan/PostProcess.frag.spv"),
        reinterpret_cast<VkRenderPass>(compositeRenderPass_->GetNativeHandle()),
        compositePipelineLayout_->GetHandle(),
        *compositePipelineLayout_
    );
    blurDownPipeline_ = createPipeline(
        Utils::PathUtils::GetResourcePath("Shaders/Vulkan/DualKawaseDown.frag.spv"),
        blurRenderPass_,
        blurPipelineLayout_->GetHandle(),
        *blurPipelineLayout_
    );
    blurUpPipeline_ = createPipeline(
        Utils::PathUtils::GetResourcePath("Shaders/Vulkan/DualKawaseUp.frag.spv"),
        blurRenderPass_,
        blurPipelineLayout_->GetHandle(),
        *blurPipelineLayout_
    );
}

void PostProcessRenderer::DestroyBlurResources(VulkanContext& context) {
    for (auto& imageLevels : downLevels_) {
        for (auto& level : imageLevels) {
            if (level.Framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(context.GetDevice(), level.Framebuffer, nullptr);
                level.Framebuffer = VK_NULL_HANDLE;
            }
            if (level.View != VK_NULL_HANDLE) {
                vkDestroyImageView(context.GetDevice(), level.View, nullptr);
                level.View = VK_NULL_HANDLE;
            }
            if (level.Image != VK_NULL_HANDLE) {
                vkDestroyImage(context.GetDevice(), level.Image, nullptr);
                level.Image = VK_NULL_HANDLE;
            }
            if (level.Memory != VK_NULL_HANDLE) {
                vkFreeMemory(context.GetDevice(), level.Memory, nullptr);
                level.Memory = VK_NULL_HANDLE;
            }
        }
    }
    for (auto& imageLevels : upLevels_) {
        for (auto& level : imageLevels) {
            if (level.Framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(context.GetDevice(), level.Framebuffer, nullptr);
                level.Framebuffer = VK_NULL_HANDLE;
            }
            if (level.View != VK_NULL_HANDLE) {
                vkDestroyImageView(context.GetDevice(), level.View, nullptr);
                level.View = VK_NULL_HANDLE;
            }
            if (level.Image != VK_NULL_HANDLE) {
                vkDestroyImage(context.GetDevice(), level.Image, nullptr);
                level.Image = VK_NULL_HANDLE;
            }
            if (level.Memory != VK_NULL_HANDLE) {
                vkFreeMemory(context.GetDevice(), level.Memory, nullptr);
                level.Memory = VK_NULL_HANDLE;
            }
        }
    }

    downLevels_.clear();
    upLevels_.clear();
    blurExtents_.clear();
}

void PostProcessRenderer::DestroyPipelines(VulkanContext& context) {
    static_cast<void>(context);
    compositePipeline_.reset();
    blurDownPipeline_.reset();
    blurUpPipeline_.reset();
    if (blurRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.GetDevice(), blurRenderPass_, nullptr);
        blurRenderPass_ = VK_NULL_HANDLE;
    }
}

std::uint32_t PostProcessRenderer::ComputeUsedBlurLevels() const {
    if (blurExtents_.empty()) {
        return 0;
    }
    const float blurAmount = std::max(GetBlurAmount(), 0.0F);
    const std::uint32_t availableLevels = static_cast<std::uint32_t>(blurExtents_.size());
    const std::uint32_t requested = static_cast<std::uint32_t>(std::ceil(blurAmount)) + 1;
    return std::max(1U, std::min(availableLevels, requested));
}

float PostProcessRenderer::GetBlurAmount() const {
    if (const auto lighting = GetLighting(); lighting != nullptr) {
        return static_cast<float>(lighting->GetProperty("NeonBlur").toDouble());
    }
    return 2.0F;
}

void PostProcessRenderer::DestroySwapchainResources(VulkanContext& context) {
    DestroyPipelines(context);
    DestroyBlurResources(context);

    compositeBindings_.clear();
    downBindings_.clear();
    upBindings_.clear();
    compositeBindingLayout_.reset();
    blurBindingLayout_.reset();
}

std::shared_ptr<DataModel::Lighting> PostProcessRenderer::GetLighting() const {
    if (place_ == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
}

} // namespace Lvs::Engine::Rendering::Vulkan

