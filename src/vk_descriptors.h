#pragma once

#include "vk_types.h"

// build a descriptor set layout
struct DescriptorLayoutBuilder {
    // bindings
    std::vector<VkDescriptorSetLayoutBinding> bindings; // a vector of all the descriptor set layout bindings we'll use

    // functions
    void addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStageFlag); // add a binding to our vector
    void clear();
    VkDescriptorSetLayout build(VkDevice device, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator {
    
};