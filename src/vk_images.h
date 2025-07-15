#pragma once

#include <vulkan/vulkan.h>

namespace vkutil {
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

    void copyImageToImage(VkCommandBuffer commandBuffer, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
};