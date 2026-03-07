#include "Lvs/Engine/Rendering/Vulkan/VulkanCubemapUtils.hpp"

#include "Lvs/Engine/Rendering/Vulkan/VulkanBufferUtils.hpp"
#include "Lvs/Engine/Rendering/Vulkan/VulkanImageUtils.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/ImageIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace Lvs::Engine::Rendering::Vulkan::CubemapUtils {

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

void ApplySkyboxFaceProcessing(
    Utils::ImageIO::ImageRgba8& image,
    const std::size_t faceIndex,
    const int resolutionCap,
    const bool compression
) {
    static_cast<void>(faceIndex);
    if (resolutionCap > 0) {
        const int maxSide = std::max(static_cast<int>(image.Width), static_cast<int>(image.Height));
        if (maxSide > resolutionCap) {
            image = Utils::ImageIO::ResizeRgba8(
                image,
                resolutionCap,
                resolutionCap,
                true,
                true
            );
        }
    }

    if (compression) {
        image = Utils::ImageIO::ResizeRgba8(
            image,
            std::max(1, static_cast<int>(image.Width) / 2),
            std::max(1, static_cast<int>(image.Height) / 2),
            false,
            false
        );
    }
}

CubemapHandle CreateCubemapFromFaceImages(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkQueue queue,
    const std::uint32_t queueFamilyIndex,
    const std::array<Utils::ImageIO::ImageRgba8, 6>& faces,
    const bool linearFiltering
) {
    const std::uint32_t width = faces[0].Width;
    const std::uint32_t height = faces[0].Height;

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

    auto image = ImageUtils::CreateImage2D(
        physicalDevice,
        device,
        width,
        height,
        1,
        6,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
    );
    cubemap.Image = image.Image;
    cubemap.Memory = image.Memory;

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
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 6}
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
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 6}
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

    cubemap.View = ImageUtils::CreateImageView(
        device,
        cubemap.Image,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,
        1,
        6
    );
    cubemap.Sampler = ImageUtils::CreateSampler(
        device,
        linearFiltering ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    );

    BufferUtils::DestroyBuffer(device, staging);
    return cubemap;
}

} // namespace

CubemapHandle CreateCubemapFromPaths(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkQueue queue,
    const std::uint32_t queueFamilyIndex,
    const std::array<std::filesystem::path, 6>& facePaths,
    const bool linearFiltering,
    const int resolutionCap,
    const bool compression
) {
    std::array<Utils::ImageIO::ImageRgba8, 6> faces;
    for (std::size_t i = 0; i < faces.size(); ++i) {
        const auto resolved = ResolvePath(facePaths[i]);
        auto image = Utils::ImageIO::LoadRgba8(resolved);
        ApplySkyboxFaceProcessing(image, i, resolutionCap, compression);
        if (i > 0U && (image.Width != faces[0].Width || image.Height != faces[0].Height)) {
            throw std::runtime_error("Skybox faces must have identical dimensions.");
        }
        faces[i] = image;
    }

    return CreateCubemapFromFaceImages(
        physicalDevice,
        device,
        queue,
        queueFamilyIndex,
        faces,
        linearFiltering
    );
}

CubemapHandle CreateCubemapFromCrossPath(
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const VkQueue queue,
    const std::uint32_t queueFamilyIndex,
    const std::filesystem::path& crossPath,
    const bool linearFiltering,
    const int resolutionCap,
    const bool compression
) {
    const auto resolved = ResolvePath(crossPath);
    const auto cross = Utils::ImageIO::LoadRgba8(resolved);

    if (cross.Width % 4 != 0 || cross.Height % 3 != 0) {
        throw std::runtime_error("Skybox cross texture dimensions must be divisible by 4x3.");
    }

    const int faceWidth = static_cast<int>(cross.Width) / 4;
    const int faceHeight = static_cast<int>(cross.Height) / 3;
    if (faceWidth <= 0 || faceHeight <= 0 || faceWidth != faceHeight) {
        throw std::runtime_error("Skybox cross texture must contain square faces in a 4x3 layout.");
    }

    std::array<Utils::ImageIO::ImageRgba8, 6> faces{
        Utils::ImageIO::CropRgba8(cross, faceWidth * 2, faceHeight, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 0, faceHeight, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 1, faceHeight * 0, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 1, faceHeight * 2, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 1, faceHeight, faceWidth, faceHeight),
        Utils::ImageIO::CropRgba8(cross, faceWidth * 3, faceHeight, faceWidth, faceHeight),
    };

    for (std::size_t i = 0; i < faces.size(); ++i) {
        ApplySkyboxFaceProcessing(faces[i], i, resolutionCap, compression);
    }

    return CreateCubemapFromFaceImages(
        physicalDevice,
        device,
        queue,
        queueFamilyIndex,
        faces,
        linearFiltering
    );
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
    if (cubemap.Image != VK_NULL_HANDLE || cubemap.Memory != VK_NULL_HANDLE) {
        ImageUtils::ImageHandle image{.Image = cubemap.Image, .Memory = cubemap.Memory};
        ImageUtils::DestroyImage(device, image);
        cubemap.Image = VK_NULL_HANDLE;
        cubemap.Memory = VK_NULL_HANDLE;
    }
    cubemap.Width = 0;
    cubemap.Height = 0;
}

} // namespace Lvs::Engine::Rendering::Vulkan::CubemapUtils
