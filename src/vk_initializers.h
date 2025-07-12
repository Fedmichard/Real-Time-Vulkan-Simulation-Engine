#pragma once

#include "vk_types.h"

// for initializing vulkan objects
namespace vkinit {
    // begin info for allocated command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags);

    // allocate and begin command buffer for single time commands
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkCommandBufferUsageFlags flags);

    // end single time commands
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue graphicsQueue, VkDevice device, VkCommandPool commandPool);
 
    // transition an images layout
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
}
