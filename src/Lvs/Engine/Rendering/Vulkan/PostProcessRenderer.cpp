#include "Lvs/Engine/Rendering/Vulkan/PostProcessRenderer.hpp"

#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanContext.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanShaderUtils.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan {

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
    CreateCompositeDescriptorSetLayout(context);
    CreateBlurDescriptorSetLayout(context);
    CreatePipelineLayouts(context);
    CreateRenderPasses(context);
    CreateBlurResources(context, static_cast<std::uint32_t>(sceneViews.size()));
    CreateDescriptorPool(context, static_cast<std::uint32_t>(sceneViews.size()), static_cast<std::uint32_t>(blurExtents_.size()));
    CreateDescriptorSets(context, sceneViews, glowViews);
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
    CreateDescriptorPool(context, static_cast<std::uint32_t>(sceneViews.size()), static_cast<std::uint32_t>(blurExtents_.size()));
    CreateDescriptorSets(context, sceneViews, glowViews);
    CreatePipelines(context);
}

void PostProcessRenderer::Shutdown(VulkanContext& context) {
    DestroySwapchainResources(context);

    if (compositePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.GetDevice(), compositePipelineLayout_, nullptr);
        compositePipelineLayout_ = VK_NULL_HANDLE;
    }
    if (blurPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.GetDevice(), blurPipelineLayout_, nullptr);
        blurPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (compositeDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.GetDevice(), compositeDescriptorSetLayout_, nullptr);
        compositeDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (blurDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.GetDevice(), blurDescriptorSetLayout_, nullptr);
        blurDescriptorSetLayout_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
}

void PostProcessRenderer::BindToPlace(const std::shared_ptr<DataModel::Place>& place) {
    place_ = place;
}

void PostProcessRenderer::Unbind() {
    place_.reset();
}

void PostProcessRenderer::RecordBlurCommands(
    VulkanContext& context,
    const VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex
) {
    if (!initialized_ || blurDownPipeline_ == VK_NULL_HANDLE || blurUpPipeline_ == VK_NULL_HANDLE ||
        imageIndex >= downLevels_.size() || imageIndex >= downDescriptorSets_.size() || blurExtents_.empty()) {
        return;
    }

    const std::uint32_t levelsUsed = ComputeUsedBlurLevels();
    if (levelsUsed == 0) {
        return;
    }

    const float blurAmount = std::max(GetBlurAmount(), 1.0F);
    const VkDevice device = context.GetDevice();

    const auto updateSourceDescriptor = [&](const VkDescriptorSet descriptorSet, const VkImageView sourceView) {
        const VkDescriptorImageInfo sourceInfo{
            .sampler = blurSampler_,
            .imageView = sourceView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        const VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &sourceInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        };
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
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
            commandBuffer,
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

    VkExtent2D currentSourceExtent = context.GetSwapchainExtent();

    // Rebind upsample sources for the active blur chain. This prevents sampling stale levels.
    if (levelsUsed > 1 && imageIndex < upDescriptorSets_.size()) {
        for (std::uint32_t level = 0; level < (levelsUsed - 1); ++level) {
            const VkImageView sourceView = (level == levelsUsed - 2)
                ? downLevels_[imageIndex][levelsUsed - 1].View
                : upLevels_[imageIndex][level + 1].View;
            updateSourceDescriptor(upDescriptorSets_[imageIndex][level], sourceView);
        }
    }

    const VkImageView finalBlurView = levelsUsed > 1 ? upLevels_[imageIndex][0].View : downLevels_[imageIndex][0].View;
    const VkDescriptorImageInfo compositeGlowInfo{
        .sampler = blurSampler_,
        .imageView = finalBlurView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    const VkWriteDescriptorSet compositeGlowWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = compositeDescriptorSets_[imageIndex],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &compositeGlowInfo,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };
    vkUpdateDescriptorSets(device, 1, &compositeGlowWrite, 0, nullptr);

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

        vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = extent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurDownPipeline_);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            blurPipelineLayout_,
            0,
            1,
            &downDescriptorSets_[imageIndex][level],
            0,
            nullptr
        );

        const std::array<float, 4> settings{
            1.0F / static_cast<float>(currentSourceExtent.width),
            1.0F / static_cast<float>(currentSourceExtent.height),
            blurAmount,
            0.0F
        };
        vkCmdPushConstants(commandBuffer, blurPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(settings), settings.data());
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        issueSampleBarrier(downLevels_[imageIndex][level].Image);
        currentSourceExtent = extent;
    }

    if (levelsUsed <= 1 || imageIndex >= upDescriptorSets_.size()) {
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

        vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport{
            .x = 0.0F,
            .y = 0.0F,
            .width = static_cast<float>(extent.width),
            .height = static_cast<float>(extent.height),
            .minDepth = 0.0F,
            .maxDepth = 1.0F
        };
        const VkRect2D scissor{.offset = {0, 0}, .extent = extent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurUpPipeline_);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            blurPipelineLayout_,
            0,
            1,
            &upDescriptorSets_[imageIndex][static_cast<std::uint32_t>(level)],
            0,
            nullptr
        );

        const std::array<float, 4> settings{
            1.0F / static_cast<float>(currentSourceExtent.width),
            1.0F / static_cast<float>(currentSourceExtent.height),
            blurAmount,
            0.0F
        };
        vkCmdPushConstants(commandBuffer, blurPipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(settings), settings.data());
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        issueSampleBarrier(upLevels_[imageIndex][static_cast<std::uint32_t>(level)].Image);
        currentSourceExtent = extent;
    }
}

void PostProcessRenderer::DrawComposite(
    VulkanContext& context,
    const VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex,
    const std::uint32_t frameIndex
) {
    static_cast<void>(context);
    if (!initialized_ || compositePipeline_ == VK_NULL_HANDLE || imageIndex >= compositeDescriptorSets_.size()) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        compositePipelineLayout_,
        0,
        1,
        &compositeDescriptorSets_[imageIndex],
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
        commandBuffer,
        compositePipelineLayout_,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(settings),
        settings.data()
    );
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void PostProcessRenderer::CreateCompositeDescriptorSetLayout(VulkanContext& context) {
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
    if (vkCreateDescriptorSetLayout(context.GetDevice(), &createInfo, nullptr, &compositeDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process composite descriptor set layout.");
    }
}

void PostProcessRenderer::CreateBlurDescriptorSetLayout(VulkanContext& context) {
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
    if (vkCreateDescriptorSetLayout(context.GetDevice(), &createInfo, nullptr, &blurDescriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create blur descriptor set layout.");
    }
}

void PostProcessRenderer::CreatePipelineLayouts(VulkanContext& context) {
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
        .pSetLayouts = &compositeDescriptorSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    if (vkCreatePipelineLayout(context.GetDevice(), &compositeInfo, nullptr, &compositePipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process composite pipeline layout.");
    }

    const VkPipelineLayoutCreateInfo blurInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &blurDescriptorSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange
    };
    if (vkCreatePipelineLayout(context.GetDevice(), &blurInfo, nullptr, &blurPipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create blur pipeline layout.");
    }
}

void PostProcessRenderer::CreateDescriptorPool(
    VulkanContext& context,
    const std::uint32_t imageCount,
    const std::uint32_t levelCount
) {
    if (imageCount == 0 || levelCount == 0) {
        return;
    }

    const std::uint32_t downSets = imageCount * levelCount;
    const std::uint32_t upSets = imageCount * (levelCount > 1 ? levelCount - 1 : 0);
    const std::uint32_t compositeSets = imageCount;
    const std::uint32_t totalSets = downSets + upSets + compositeSets;
    const std::uint32_t totalDescriptors = downSets + upSets + (compositeSets * 2);

    const VkDescriptorPoolSize poolSize{
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = totalDescriptors
    };
    const VkDescriptorPoolCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = totalSets,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    if (vkCreateDescriptorPool(context.GetDevice(), &createInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create post-process descriptor pool.");
    }
}

void PostProcessRenderer::CreateBlurResources(VulkanContext& context, const std::uint32_t imageCount) {
    const VkExtent2D swapchainExtent = context.GetSwapchainExtent();
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
            .format = context.GetSwapchainImageFormat(),
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
            .format = context.GetSwapchainImageFormat(),
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

void PostProcessRenderer::CreateDescriptorSets(
    VulkanContext& context,
    const std::vector<VkImageView>& sceneViews,
    const std::vector<VkImageView>& glowViews
) {
    const std::uint32_t imageCount = static_cast<std::uint32_t>(sceneViews.size());
    const std::uint32_t levelCount = static_cast<std::uint32_t>(blurExtents_.size());
    if (imageCount == 0 || levelCount == 0 || glowViews.size() != sceneViews.size() || blurSampler_ == VK_NULL_HANDLE) {
        return;
    }

    const auto allocateSets = [&](const VkDescriptorSetLayout layout, const std::uint32_t count) {
        std::vector<VkDescriptorSetLayout> layouts(count, layout);
        std::vector<VkDescriptorSet> sets(count);
        const VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = descriptorPool_,
            .descriptorSetCount = count,
            .pSetLayouts = layouts.data()
        };
        if (vkAllocateDescriptorSets(context.GetDevice(), &allocInfo, sets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate post-process descriptor sets.");
        }
        return sets;
    };

    compositeDescriptorSets_ = allocateSets(compositeDescriptorSetLayout_, imageCount);
    downDescriptorSets_.assign(imageCount, std::vector<VkDescriptorSet>(levelCount));
    upDescriptorSets_.assign(imageCount, std::vector<VkDescriptorSet>(levelCount > 1 ? levelCount - 1 : 0));

    for (std::uint32_t image = 0; image < imageCount; ++image) {
        const auto downSets = allocateSets(blurDescriptorSetLayout_, levelCount);
        for (std::uint32_t level = 0; level < levelCount; ++level) {
            downDescriptorSets_[image][level] = downSets[level];
            const VkImageView sourceView = (level == 0) ? glowViews[image] : downLevels_[image][level - 1].View;
            const VkDescriptorImageInfo sourceInfo{
                .sampler = blurSampler_,
                .imageView = sourceView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            const VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = downDescriptorSets_[image][level],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &sourceInfo,
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            };
            vkUpdateDescriptorSets(context.GetDevice(), 1, &write, 0, nullptr);
        }

        if (levelCount > 1) {
            const auto upSets = allocateSets(blurDescriptorSetLayout_, levelCount - 1);
            for (std::uint32_t level = 0; level < (levelCount - 1); ++level) {
                upDescriptorSets_[image][level] = upSets[level];
                const VkImageView sourceView = (level == levelCount - 2)
                    ? downLevels_[image][levelCount - 1].View
                    : upLevels_[image][level + 1].View;
                const VkDescriptorImageInfo sourceInfo{
                    .sampler = blurSampler_,
                    .imageView = sourceView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
                const VkWriteDescriptorSet write{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = upDescriptorSets_[image][level],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &sourceInfo,
                    .pBufferInfo = nullptr,
                    .pTexelBufferView = nullptr
                };
                vkUpdateDescriptorSets(context.GetDevice(), 1, &write, 0, nullptr);
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
        const std::array writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = compositeDescriptorSets_[image],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfos[0],
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            },
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = nullptr,
                .dstSet = compositeDescriptorSets_[image],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfos[1],
                .pBufferInfo = nullptr,
                .pTexelBufferView = nullptr
            }
        };
        vkUpdateDescriptorSets(context.GetDevice(), static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void PostProcessRenderer::CreateRenderPasses(VulkanContext& context) {
    const VkAttachmentDescription colorAttachment{
        .flags = 0,
        .format = context.GetSwapchainImageFormat(),
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

    const QString vertPath = Utils::SourcePath::GetResourcePath("Shaders/Vulkan/PostProcess.vert.spv");
    const auto vertCode = ShaderUtils::ReadBinaryFile(vertPath);
    const VkShaderModule vertModule = ShaderUtils::CreateShaderModule(device, vertCode);

    const auto createPipeline = [&](const QString& fragPath, const VkRenderPass renderPass, const VkPipelineLayout pipelineLayout) {
        const auto fragCode = ShaderUtils::ReadBinaryFile(fragPath);
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

        VkPipeline pipeline = VK_NULL_HANDLE;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            vkDestroyShaderModule(device, fragModule, nullptr);
            throw std::runtime_error("Failed to create post-process graphics pipeline.");
        }
        vkDestroyShaderModule(device, fragModule, nullptr);
        return pipeline;
    };

    compositePipeline_ = createPipeline(
        Utils::SourcePath::GetResourcePath("Shaders/Vulkan/PostProcess.frag.spv"),
        context.GetPostProcessRenderPass(),
        compositePipelineLayout_
    );
    blurDownPipeline_ = createPipeline(
        Utils::SourcePath::GetResourcePath("Shaders/Vulkan/DualKawaseDown.frag.spv"),
        blurRenderPass_,
        blurPipelineLayout_
    );
    blurUpPipeline_ = createPipeline(
        Utils::SourcePath::GetResourcePath("Shaders/Vulkan/DualKawaseUp.frag.spv"),
        blurRenderPass_,
        blurPipelineLayout_
    );

    vkDestroyShaderModule(device, vertModule, nullptr);
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
    if (compositePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.GetDevice(), compositePipeline_, nullptr);
        compositePipeline_ = VK_NULL_HANDLE;
    }
    if (blurDownPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.GetDevice(), blurDownPipeline_, nullptr);
        blurDownPipeline_ = VK_NULL_HANDLE;
    }
    if (blurUpPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.GetDevice(), blurUpPipeline_, nullptr);
        blurUpPipeline_ = VK_NULL_HANDLE;
    }
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

    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.GetDevice(), descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }

    compositeDescriptorSets_.clear();
    downDescriptorSets_.clear();
    upDescriptorSets_.clear();
}

std::shared_ptr<DataModel::Lighting> PostProcessRenderer::GetLighting() const {
    if (place_ == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<DataModel::Lighting>(place_->FindService("Lighting"));
}

} // namespace Lvs::Engine::Rendering::Vulkan
