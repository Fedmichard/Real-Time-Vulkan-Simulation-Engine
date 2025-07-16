#include "vk_descriptors.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStageFlag) {
    // every binding needs to be described
    // it can take in a list of bindings
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = binding;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = type;
    uboLayoutBinding.stageFlags = shaderStageFlag;

    bindings.push_back(uboLayoutBinding);
}

void DescriptorLayoutBuilder::clear() {
    bindings.clear();
}


VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0) {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = pNext;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    createInfo.flags = flags;

    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout));

    return descriptorSetLayout;
}