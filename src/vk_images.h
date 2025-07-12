#pragma once

#include <vulkan/vulkan.h>

namespace vkutil {
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
};