#include "Lvs/Engine/Rendering/Vulkan/VulkanTextureUtils.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/ImageIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace Lvs::Engine::Rendering::Vulkan::TextureUtils {

namespace {

std::filesystem::path ResolvePath(const std::filesystem::path& path) {
    if (path.empty()) {
        return path;
    }
    if (Utils::FileIO::Exists(path)) {
        return path;
    }
    return Utils::PathUtils::ToOsPath(path.string());
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
        throw std::runtime_error("Failed to create transient command pool for cubemap upload.");
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
        throw std::runtime_error("Failed to allocate cubemap upload command buffer.");
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
        throw std::runtime_error("Failed to begin cubemap upload command buffer.");
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
        throw std::runtime_error("Failed to end cubemap upload command buffer.");
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
        throw std::runtime_error("Failed to submit cubemap upload command buffer.");
    }
    vkQueueWaitIdle(queue);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

} // namespace

CubemapHandle CreateCubemapFromPaths(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkQueue queue,
    const std::uint32_t queueFamilyIndex,
    const std::array<std::filesystem::path, 6>& facePaths,
    const bool linearFiltering
) {
    std::array<Utils::ImageIO::ImageRgba8, 6> faces;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto resolved = ResolvePath(facePaths[i]);
        const auto image = Utils::ImageIO::LoadRgba8(resolved);
        if (i == 0) {
            width = image.Width;
            height = image.Height;
        } else if (image.Width != width || image.Height != height) {
            throw std::runtime_error("Skybox faces must have identical dimensions.");
        }
        faces[i] = image;
    }

    const VkDeviceSize faceSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
    const VkDeviceSize totalSize = faceSize * 6;

    auto staging = BufferUtils::CreateBuffer(
        physicalDevice,
        device,
        totalSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void* mapped = nullptr;
    vkMapMemory(device, staging.Memory, 0, totalSize, 0, &mapped);
    auto* bytes = static_cast<std::uint8_t*>(mapped);
    for (std::size_t i = 0; i < faces.size(); ++i) {
        std::memcpy(
            bytes + static_cast<std::size_t>(faceSize * static_cast<VkDeviceSize>(i)),
            faces[i].Pixels.data(),
            static_cast<std::size_t>(faceSize)
        );
    }
    vkUnmapMemory(device, staging.Memory);

    CubemapHandle cubemap{};
    cubemap.Width = width;
    cubemap.Height = height;

    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {.width = width, .height = height, .depth = 1},
        .mipLevels = 1,
        .arrayLayers = 6,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if (vkCreateImage(device, &imageInfo, nullptr, &cubemap.Image) != VK_SUCCESS) {
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create cubemap image.");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, cubemap.Image, &memRequirements);
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
    if (vkAllocateMemory(device, &allocInfo, nullptr, &cubemap.Memory) != VK_SUCCESS) {
        vkDestroyImage(device, cubemap.Image, nullptr);
        cubemap.Image = VK_NULL_HANDLE;
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to allocate cubemap memory.");
    }
    vkBindImageMemory(device, cubemap.Image, cubemap.Memory, 0);

    VkCommandPool commandPool = VK_NULL_HANDLE;
    const VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, queueFamilyIndex, commandPool);

    const VkImageMemoryBarrier toTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = cubemap.Image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 6}
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

    std::array<VkBufferImageCopy, 6> regions{};
    for (std::uint32_t face = 0; face < 6; ++face) {
        regions[face] = VkBufferImageCopy{
            .bufferOffset = faceSize * face,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = face, .layerCount = 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {width, height, 1}
        };
    }
    vkCmdCopyBufferToImage(
        commandBuffer,
        staging.Buffer,
        cubemap.Image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<std::uint32_t>(regions.size()),
        regions.data()
    );

    const VkImageMemoryBarrier toShaderRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = cubemap.Image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 6}
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
    EndOneTimeCommands(device, queue, commandPool, commandBuffer);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = cubemap.Image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 6}
    };
    if (vkCreateImageView(device, &viewInfo, nullptr, &cubemap.View) != VK_SUCCESS) {
        DestroyCubemap(device, cubemap);
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create cubemap image view.");
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
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
    if (vkCreateSampler(device, &samplerInfo, nullptr, &cubemap.Sampler) != VK_SUCCESS) {
        DestroyCubemap(device, cubemap);
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create cubemap sampler.");
    }

    BufferUtils::DestroyBuffer(device, staging);
    return cubemap;
}

Texture2DHandle CreateTexture2DFromPath(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkQueue queue,
    const std::uint32_t queueFamilyIndex,
    const std::filesystem::path& imagePath,
    const bool linearFiltering,
    const VkFormat format,
    const VkSamplerAddressMode addressMode
) {
    const auto resolvedPath = ResolvePath(imagePath);
    const auto image = Utils::ImageIO::LoadRgba8(resolvedPath);

    const auto width = image.Width;
    const auto height = image.Height;
    const VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;

    auto staging = BufferUtils::CreateBuffer(
        physicalDevice,
        device,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    void* mapped = nullptr;
    vkMapMemory(device, staging.Memory, 0, imageSize, 0, &mapped);
    std::memcpy(mapped, image.Pixels.data(), static_cast<std::size_t>(imageSize));
    vkUnmapMemory(device, staging.Memory);

    Texture2DHandle texture{};
    texture.Width = width;
    texture.Height = height;

    const VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {.width = width, .height = height, .depth = 1},
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
    if (vkCreateImage(device, &imageInfo, nullptr, &texture.Image) != VK_SUCCESS) {
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create texture image.");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, texture.Image, &memRequirements);
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
    if (vkAllocateMemory(device, &allocInfo, nullptr, &texture.Memory) != VK_SUCCESS) {
        vkDestroyImage(device, texture.Image, nullptr);
        texture.Image = VK_NULL_HANDLE;
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to allocate texture memory.");
    }
    vkBindImageMemory(device, texture.Image, texture.Memory, 0);

    VkCommandPool commandPool = VK_NULL_HANDLE;
    const VkCommandBuffer commandBuffer = BeginOneTimeCommands(device, queueFamilyIndex, commandPool);

    const VkImageMemoryBarrier toTransfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.Image,
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

    const VkBufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };
    vkCmdCopyBufferToImage(commandBuffer, staging.Buffer, texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    const VkImageMemoryBarrier toShaderRead{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.Image,
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
    EndOneTimeCommands(device, queue, commandPool, commandBuffer);

    const VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = texture.Image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
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
    if (vkCreateImageView(device, &viewInfo, nullptr, &texture.View) != VK_SUCCESS) {
        DestroyTexture2D(device, texture);
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create texture image view.");
    }

    const VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = addressMode,
        .addressModeV = addressMode,
        .addressModeW = addressMode,
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
    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.Sampler) != VK_SUCCESS) {
        DestroyTexture2D(device, texture);
        BufferUtils::DestroyBuffer(device, staging);
        throw std::runtime_error("Failed to create texture sampler.");
    }

    BufferUtils::DestroyBuffer(device, staging);
    return texture;
}

void DestroyTexture2D(const VkDevice device, Texture2DHandle& texture) {
    if (texture.Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, texture.Sampler, nullptr);
        texture.Sampler = VK_NULL_HANDLE;
    }
    if (texture.View != VK_NULL_HANDLE) {
        vkDestroyImageView(device, texture.View, nullptr);
        texture.View = VK_NULL_HANDLE;
    }
    if (texture.Image != VK_NULL_HANDLE) {
        vkDestroyImage(device, texture.Image, nullptr);
        texture.Image = VK_NULL_HANDLE;
    }
    if (texture.Memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, texture.Memory, nullptr);
        texture.Memory = VK_NULL_HANDLE;
    }
    texture.Width = 0;
    texture.Height = 0;
}

void DestroyCubemap(const VkDevice device, CubemapHandle& cubemap) {
    if (cubemap.Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, cubemap.Sampler, nullptr);
        cubemap.Sampler = VK_NULL_HANDLE;
    }
    if (cubemap.View != VK_NULL_HANDLE) {
        vkDestroyImageView(device, cubemap.View, nullptr);
        cubemap.View = VK_NULL_HANDLE;
    }
    if (cubemap.Image != VK_NULL_HANDLE) {
        vkDestroyImage(device, cubemap.Image, nullptr);
        cubemap.Image = VK_NULL_HANDLE;
    }
    if (cubemap.Memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, cubemap.Memory, nullptr);
        cubemap.Memory = VK_NULL_HANDLE;
    }
    cubemap.Width = 0;
    cubemap.Height = 0;
}

} // namespace Lvs::Engine::Rendering::Vulkan::TextureUtils
